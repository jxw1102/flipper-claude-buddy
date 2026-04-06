/**
 * Claude Buddy - Flipper Zero companion for Claude Code
 *
 * Physical remote control + audio/haptic feedback device.
 * Buttons: UP=voice, LEFT=ESC, RIGHT=menu, OK=enter, BACK(short)=dismiss, BACK(long)=exit
 *
 * Threading model:
 *   Serial RX callback runs on a worker thread — it must NOT call UI functions.
 *   Instead it queues parsed messages into a FuriMessageQueue and wakes the
 *   GUI thread via view_dispatcher_send_custom_event.  The custom-event
 *   callback drains the queue and performs all UI updates safely.
 *
 * Transport selection (runtime, automatic):
 *   If a USB cable is detected at startup → USB CDC transport.
 *   Otherwise → BLE serial transport.
 *   The active mode is shown in the header ("USB" or "BLE").
 */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_power.h>
#include <gui/gui.h>
#include <notification/notification_messages.h>

#include "transport.h"
#include "protocol.h"
#include "notifications.h"
#include "ui.h"

#define SERIAL_QUEUE_SIZE 8

enum {
    CustomEventSerialMsg     = 100,
    // UI_CUSTOM_EVENT_TRANSITION = 101 is defined in ui.h
    CustomEventUsbDisconnect = 102,
};

typedef struct {
    UiState* ui;
    Transport* transport;
    NotificationApp* notifications;
    FuriMessageQueue* serial_queue;
    FuriTimer* usb_poll_timer; // non-NULL only in USB mode; polls for cable removal
    bool hello_sent;
    bool muted;      // suppress all sounds (toggled by long-press Down)
    bool is_working; // blue LED active — alerts should flash cyan then return to blue
    bool dictating;  // true while dictation is active (set by voice_start / cleared by voice_stop)
    ProtocolMessage rx_msg_buf;     // Buffer for parsing on transport thread
    ProtocolMessage process_msg_buf; // Buffer for executing on GUI thread
    char tx_buf[256];               // Shared TX building buffer (GUI thread)
} App;

// Play a sound unless muted. Interrupt/mute toggle always plays regardless.
static void app_notify(App* app, SoundType snd) {
    if(!app || !app->notifications) return;
    if(!app->muted) notify_play(app->notifications, snd,
        app->is_working ? LedStateWorking : LedStateOff);
}

/* ── Transport auto-detection ─────────────────────────────────── */

static bool detect_usb_cable(void) {
    /* Detect USB cable by checking either:
       - actively charging (furi_hal_power_is_charging()), OR
       - fully charged but still plugged in (furi_hal_power_is_charging_done()).
       Together these cover all USB-connected states. */
    return furi_hal_power_is_charging() || furi_hal_power_is_charging_done();
}

/* ── USB disconnect polling (timer thread) ────────────────────── */

static void usb_poll_cb(void* context) {
    App* app = context;
    if(!detect_usb_cable()) {
        view_dispatcher_send_custom_event(
            app->ui->view_dispatcher, CustomEventUsbDisconnect);
    }
}

/* ── Helpers ──────────────────────────────────────────────────── */

static SoundType sound_from_string(const char* name) {
    if(strcmp(name, "success") == 0) return SoundSuccess;
    if(strcmp(name, "error") == 0) return SoundError;
    if(strcmp(name, "alert") == 0) return SoundAlert;
    if(strcmp(name, "voice_start") == 0) return SoundVoiceStart;
    if(strcmp(name, "voice_start_led") == 0) return SoundVoiceStartLed;
    if(strcmp(name, "voice_stop") == 0) return SoundVoiceStop;
    if(strcmp(name, "voice_stop_quiet") == 0) return SoundVoiceStopQuiet;
    if(strcmp(name, "disconnect") == 0) return SoundDisconnect;
    if(strcmp(name, "connect") == 0) return SoundConnect;
    if(strcmp(name, "led_working") == 0) return SoundLedWorking;
    if(strcmp(name, "led_off") == 0) return SoundLedOff;
    if(strcmp(name, "interrupt") == 0) return SoundInterrupt;
    if(strcmp(name, "session_end") == 0) return SoundSessionEnd;
    if(strcmp(name, "ready") == 0) return SoundReady;
    return SoundAlert;
}

/* ── Serial RX (worker thread) ────────────────────────────────── */

static void on_serial_data(const char* line, void* context) {
    App* app = context;
    if(!app || !line) return;
    if(!protocol_parse(line, &app->rx_msg_buf)) return;
    
    /* Non-blocking put; queue copies structure by value. */
    if(furi_message_queue_put(app->serial_queue, &app->rx_msg_buf, 0) == FuriStatusOk) {
        view_dispatcher_send_custom_event(app->ui->view_dispatcher, CustomEventSerialMsg);
    }
}

/* ── Message processing (GUI thread) ──────────────────────────── */

static void process_message(App* app, ProtocolMessage* msg) {
    if(!app || !msg) return;
    /* Any host-originated message implies Claude is connected.
       Handles the case where Flipper launches after session-start.
       Exception: disconnect notification should not re-set the indicator. */
    if(msg->type == MsgTypeStatus || msg->type == MsgTypePing) {
        ui_set_claude_connected(app->ui, true);
    } else if(msg->type == MsgTypeNotify) {
        SoundType snd_check = sound_from_string(msg->sound);
        if(snd_check != SoundDisconnect) {
            ui_set_claude_connected(app->ui, true);
        }
    }

    switch(msg->type) {
    case MsgTypeNotify: {
        SoundType snd = sound_from_string(msg->sound);
        bool is_voice = (snd == SoundVoiceStart || snd == SoundVoiceStartLed ||
                         snd == SoundVoiceStop || snd == SoundVoiceStopQuiet);

        // Non-LED sounds clear the working state (turn complete, error, interrupt, etc.)
        if(snd != SoundLedWorking && snd != SoundAlert) {
            app->is_working = false;
        }

        if(!app->muted) notify_play(app->notifications, snd,
            app->is_working ? LedStateWorking : LedStateOff);

        if(snd == SoundVoiceStart || snd == SoundVoiceStartLed) {
            app->dictating = true;
            ui_set_pose(app->ui, PoseListening);
        } else if(snd == SoundVoiceStop || snd == SoundVoiceStopQuiet) {
            app->dictating = false;
            ui_set_pose(app->ui, PoseIdle);
        } else if(snd == SoundSuccess || snd == SoundReady) {
            ui_set_pose(app->ui, PoseHappy);
        } else if(snd == SoundConnect) {
            ui_set_pose(app->ui, PoseExcited);
        } else if(snd == SoundError || snd == SoundAlert || snd == SoundInterrupt) {
            ui_set_pose(app->ui, PoseAlert);
        } else if(snd == SoundSessionEnd) {
            ui_set_pose(app->ui, PoseSleeping);
        }

        if(msg->text[0] && !is_voice) {
            if(msg->text2[0]) {
                ui_show_status2(app->ui, msg->text, msg->text2, true);
            } else {
                ui_show_status(app->ui, msg->text, true);
            }
        }
        break;
    }

    case MsgTypePing: {
        int len;
        /* Send hello once per connection (first received ping triggers it).
         * This was previously done in the BLE RX callback, but calling
         * ble_profile_serial_tx from inside that callback deadlocks on
         * Momentum firmware.  Sending from the GUI thread is safe. */
        if(!app->hello_sent) {
            app->hello_sent = true;
            len = protocol_build_hello(app->tx_buf, sizeof(app->tx_buf));
            transport_send(app->transport, app->tx_buf, len);
        }
        len = protocol_build_pong(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;
    }

    case MsgTypeStatus:
        app->is_working = true;
        notify_play(app->notifications, SoundLedWorking, LedStateOff); // LED-only, not subject to mute
        ui_set_pose(app->ui, PoseThinking);
        ui_show_status2(
                app->ui,
                msg->text[0] ? msg->text : "Thinking...",
                msg->text2[0] ? msg->text2 : NULL,
                true);
        break;

    case MsgTypeMenu:
        if(msg->menu_data[0]) {
            ui_update_menu(app->ui, msg->menu_data);
        }
        break;

    case MsgTypePerm:
        // Permission always plays its alert sound (backlight + vibro) even when muted
        notify_play(app->notifications, SoundPerm, LedStateOff);
        ui_show_permission(app->ui, msg->text, msg->text2);
        break;

    case MsgTypeState:
        ui_set_claude_connected(app->ui, msg->claude_connected);
        break;

    default:
        break;
    }
}

/* ── ViewDispatcher callbacks (GUI thread) ────────────────────── */

static bool on_custom_event(void* context, uint32_t event) {
    App* app = context;
    if(!app) return false;
    if(event == CustomEventSerialMsg) {
        while(furi_message_queue_get(app->serial_queue, &app->process_msg_buf, 0) == FuriStatusOk) {
            process_message(app, &app->process_msg_buf);
        }
        return true;
    }
    if(event == CustomEventUsbDisconnect) {
        // Stop USB poll timer first so it won't fire again
        if(app->usb_poll_timer) {
            furi_timer_stop(app->usb_poll_timer);
        }

        // Tear down USB transport
        if(app->transport) {
            transport_stop(app->transport);
            transport_free(app->transport);
            app->transport = NULL;
        }

        // Switch to BLE
        app->transport = transport_bt_alloc();
        app->hello_sent = false;
        ui_set_transport_mode(app->ui, true);
        transport_start(app->transport, on_serial_data, app);

        notify_play(app->notifications, SoundConnect, LedStateOff);
        return true;
    }
    return false;
}

/* ── UI event callback (GUI thread — from view input handlers) ── */

static void on_ui_event(UiEventType event, const char* data, void* context) {
    App* app = context;
    if(!app) return;
    int len = 0;

    switch(event) {
    case UiEventBackspace:
        app_notify(app, SoundEnter);
        len = protocol_build_backspace(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventEnter:
        app_notify(app, SoundEnter);
        len = protocol_build_enter(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventEsc:
        app_notify(app, SoundEsc);
        len = protocol_build_esc(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventDown:
        len = protocol_build_down(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventInterrupt:
        // Long-press Left: send Ctrl+C to host (always audible, bypasses mute)
        notify_play(app->notifications, SoundAlert, LedStateOff);
        ui_set_pose(app->ui, PoseAlert);
        app->is_working = false;
        len = protocol_build_interrupt(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventToggleMute:
        // Long-press Down: toggle mute (the toggle sound itself always plays)
        app->muted = !app->muted;
        ui_set_muted(app->ui, app->muted);
        notify_play(app->notifications, app->muted ? SoundMuteOn : SoundMuteOff, LedStateOff);
        break;

    case UiEventVoice:
        // Immediate local feedback: sound + red flash before host responds.
        // If we're currently dictating (app will receive voice_stop from host),
        // play the stop sound right away; otherwise signal "requesting" dictation.
        app_notify(app, app->dictating ? SoundVoiceStop : SoundVoiceRequest);
        len = protocol_build_voice(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventOpenMenu:
        ui_show_menu(app->ui);
        break;

    case UiEventMenuSelect:
        if(data) {
            app_notify(app, SoundCmd);
            len = protocol_build_cmd(app->tx_buf, sizeof(app->tx_buf), data);
            transport_send(app->transport, app->tx_buf, len);
        }
        ui_back_to_status(app->ui);
        break;

    case UiEventMenuBack:
        ui_back_to_status(app->ui);
        break;

    case UiEventDismiss:
        ui_back_to_status(app->ui);
        break;

    case UiEventPermAllow:
        notify_play(app->notifications, SoundLedOff, LedStateOff);
        app_notify(app, SoundSuccess);
        len = protocol_build_perm_resp(app->tx_buf, sizeof(app->tx_buf), true, false, false);
        transport_send(app->transport, app->tx_buf, len);
        ui_back_to_status(app->ui);
        break;

    case UiEventPermAlways:
        notify_play(app->notifications, SoundLedOff, LedStateOff);
        app_notify(app, SoundSuccess);
        len = protocol_build_perm_resp(app->tx_buf, sizeof(app->tx_buf), true, true, false);
        transport_send(app->transport, app->tx_buf, len);
        ui_back_to_status(app->ui);
        break;

    case UiEventPermDeny:
        notify_play(app->notifications, SoundLedOff, LedStateOff);
        app_notify(app, SoundEsc);
        len = protocol_build_perm_resp(app->tx_buf, sizeof(app->tx_buf), false, false, false);
        transport_send(app->transport, app->tx_buf, len);
        ui_back_to_status(app->ui);
        break;

    case UiEventPermEsc:
        notify_play(app->notifications, SoundLedOff, LedStateOff);
        app_notify(app, SoundEsc);
        len = protocol_build_perm_resp(app->tx_buf, sizeof(app->tx_buf), false, false, true);
        transport_send(app->transport, app->tx_buf, len);
        ui_back_to_status(app->ui);
        break;

    case UiEventYes:
        app_notify(app, SoundCmd);
        len = protocol_build_yes(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventExitApp:
        ui_stop(app->ui);
        break;
    }
}

/* ── App entry point ──────────────────────────────────────────── */

int32_t claude_buddy_app(void* p) {
    UNUSED(p);


    App* app = malloc(sizeof(App));
    furi_check(app != NULL);
    memset(app, 0, sizeof(App));

    Gui* gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* Serial → GUI message queue (allocates memory internally to copy ProtocolMessage by value) */
    app->serial_queue = furi_message_queue_alloc(SERIAL_QUEUE_SIZE, sizeof(ProtocolMessage));

    /* UI */
    app->ui = ui_alloc(gui);
    ui_set_event_callback(app->ui, on_ui_event, app);

    /* Route custom events from serial queue to the GUI thread */
    view_dispatcher_set_event_callback_context(app->ui->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->ui->view_dispatcher, on_custom_event);

    ui_show_status(app->ui, NULL, false);

    /* Auto-select transport: USB if cable is plugged in, BT otherwise */
    bool use_bt = !detect_usb_cable();
    app->transport = use_bt ? transport_bt_alloc() : transport_usb_alloc();
    ui_set_transport_mode(app->ui, use_bt);

    /* In USB mode, poll every 5 s for cable removal and auto-switch to BLE */
    if(!use_bt) {
        app->usb_poll_timer = furi_timer_alloc(usb_poll_cb, FuriTimerTypePeriodic, app);
        furi_timer_start(app->usb_poll_timer, 5000);
    }

    transport_start(app->transport, on_serial_data, app);

    /* Hello + connect sound */
    char buf[128];
    int len = protocol_build_hello(buf, sizeof(buf));
    transport_send(app->transport, buf, len);
    notify_play(app->notifications, SoundConnect, LedStateOff);
    ui_show_status(app->ui, NULL, true);

    /* Run event loop (blocks until ui_stop) */
    ui_run(app->ui);

    /* Cleanup */
    notify_play(app->notifications, SoundDisconnect, LedStateOff);
    furi_delay_ms(500);

    if(app->usb_poll_timer) {
        furi_timer_stop(app->usb_poll_timer);
        furi_timer_free(app->usb_poll_timer);
    }

    transport_stop(app->transport);
    transport_free(app->transport);
    furi_message_queue_free(app->serial_queue);
    ui_free(app->ui);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);

    return 0;
}
