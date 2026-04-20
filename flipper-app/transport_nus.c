/**
 * Nordic UART Service BLE transport.
 *
 * Parallel to transport_bt.c, but uses ble_profile_nus (standard NUS UUIDs
 * and "Claude FZ XXXX" device name) so Claude Desktop / Claude Cowork's
 * built-in BLE bridge can pair with us directly.
 *
 * Flow is the same as the Bt transport:
 *   1. Open Bt service, disconnect any existing link
 *   2. bt_profile_start with ble_profile_nus (restarts BLE core2)
 *   3. Register RX callback, start advertising
 *   4. On stop, restore the default profile
 *
 * Threading caveats identical to transport_bt.c:
 *   - RX callback runs on BLE event thread; never call nus_profile_tx or
 *     UI functions from inside it.  Line-buffer and deliver to app via cb.
 *   - TX happens from the GUI thread through nus_profile_tx.
 */

#include "transport.h"
#include "protocol.h"
#include "nus_profile.h"
#include <furi.h>
#include <furi_hal_bt.h>
#include <bt/bt_service/bt.h>

#define TAG "NusTransport"

/* At most one NUS transport is active at a time.  Module-local state:
 *   s_bt         — record handle so the unpair API can reach bonding state
 *   s_secure     — true once a central has connected with our link (NUS
 *                  chars require ATTR_PERMISSION_AUTHEN_*, so a successful
 *                  connection implies the pairing handshake completed) */
static Bt* s_bt = NULL;
static bool s_secure = false;

typedef struct {
    Transport base; /* MUST be first */
    Bt* bt;
    FuriHalBleProfileBase* profile;
    TransportRxCallback callback;
    void* callback_ctx;
    bool connected;
    bool hello_sent;
    /* Line-buffered RX */
    char rx_buf[PROTOCOL_MAX_MSG_LEN];
    int rx_pos;
} NusTransport;

/* ── BLE event callbacks (BLE thread — NOT GUI thread) ──────────── */

static void nus_rx_cb(const uint8_t* data, uint16_t len, void* context) {
    NusTransport* nt = context;
    if(!nt || !data) return;

    FURI_LOG_D(TAG, "RX chunk: %u bytes", len);

    for(uint16_t i = 0; i < len; i++) {
        if(data[i] == '\n') {
            if(nt->rx_pos > 0) {
                nt->rx_buf[nt->rx_pos] = '\0';
                FURI_LOG_D(TAG, "RX line: %s", nt->rx_buf);
                if(nt->callback) nt->callback(nt->rx_buf, nt->callback_ctx);
                nt->rx_pos = 0;
            }
        } else if(nt->rx_pos < (int)sizeof(nt->rx_buf) - 1) {
            nt->rx_buf[nt->rx_pos++] = (char)data[i];
        }
    }
}

static void nus_status_changed_cb(BtStatus status, void* context) {
    NusTransport* nt = context;
    if(!nt) return;
    if(status == BtStatusConnected) {
        FURI_LOG_I(TAG, "BLE central connected");
        nt->connected = true;
        nt->hello_sent = false;
        s_secure = true; /* NUS chars require auth → reaching Connected implies bonded */
    } else {
        if(nt->connected) {
            FURI_LOG_I(TAG, "BLE central disconnected");
        }
        nt->connected = false;
        nt->hello_sent = false;
        s_secure = false;
    }
}

/* ── vtable ─────────────────────────────────────────────────────── */

static void nus_start(Transport* t, TransportRxCallback cb, void* ctx) {
    if(!t) return;
    NusTransport* nt = (NusTransport*)t;
    nt->callback = cb;
    nt->callback_ctx = ctx;
    nt->rx_pos = 0;
    nt->connected = false;
    nt->hello_sent = false;

    nt->bt = furi_record_open(RECORD_BT);
    s_bt = nt->bt;

    /* bt_profile_start internally restarts BLE core2; disconnect any
     * existing link first so the handover is clean. */
    bt_disconnect(nt->bt);
    furi_delay_ms(200);

    nt->profile = bt_profile_start(nt->bt, ble_profile_nus, NULL);
    if(!nt->profile) {
        FURI_LOG_E(TAG, "bt_profile_start returned NULL");
        furi_record_close(RECORD_BT);
        nt->bt = NULL;
        s_bt = NULL;
        return;
    }

    nus_profile_set_rx_callback(nt->profile, nus_rx_cb, nt);
    furi_hal_bt_start_advertising();
    bt_set_status_changed_callback(nt->bt, nus_status_changed_cb, nt);

    FURI_LOG_I(TAG, "NUS transport started");
}

static void nus_stop(Transport* t) {
    if(!t) return;
    NusTransport* nt = (NusTransport*)t;
    if(!nt->bt) return;

    bt_set_status_changed_callback(nt->bt, NULL, NULL);
    bt_disconnect(nt->bt);
    furi_delay_ms(200);

    bt_profile_restore_default(nt->bt);

    furi_record_close(RECORD_BT);
    nt->bt = NULL;
    s_bt = NULL;
    s_secure = false;
    nt->profile = NULL;
    nt->connected = false;

    FURI_LOG_I(TAG, "NUS transport stopped");
}

static void nus_send(Transport* t, const char* data, int len) {
    if(!t || !data) return;
    NusTransport* nt = (NusTransport*)t;
    if(!nt->profile || !nt->connected) return;

    nus_profile_tx(nt->profile, (const uint8_t*)data, (uint16_t)len);
}

static void nus_free(Transport* t) {
    if(!t) return;
    free(t);
}

/* ── public factory ─────────────────────────────────────────────── */

Transport* transport_nus_alloc(void) {
    NusTransport* nt = malloc(sizeof(NusTransport));
    furi_check(nt != NULL);
    memset(nt, 0, sizeof(*nt));
    nt->base.start = nus_start;
    nt->base.stop = nus_stop;
    nt->base.send = nus_send;
    nt->base.free_fn = nus_free;
    return (Transport*)nt;
}

/* ── public helpers for the app layer ───────────────────────── */

bool transport_nus_is_secure(void) {
    return s_secure;
}

void transport_nus_forget_bonds(void) {
    /* Prefer the active transport's Bt handle; if nothing is active we
     * still do the right thing by opening the record briefly. */
    Bt* bt = s_bt;
    bool opened_here = false;
    if(!bt) {
        bt = furi_record_open(RECORD_BT);
        opened_here = true;
    }
    if(bt) bt_forget_bonded_devices(bt);
    if(opened_here) furi_record_close(RECORD_BT);
    s_secure = false;
}
