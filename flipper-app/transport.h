#pragma once
#include <furi.h>

/**
 * Abstract transport layer — USB CDC or Bluetooth serial.
 *
 * Each implementation embeds Transport as its FIRST struct member so that
 * a pointer to the concrete struct can be safely cast to/from Transport*.
 *
 * Usage:
 *   Transport* t = transport_usb_alloc();   // or transport_bt_alloc()
 *   transport_start(t, my_rx_cb, ctx);
 *   transport_send(t, buf, len);
 *   transport_stop(t);
 *   transport_free(t);
 */

/** Called by the transport when a complete newline-terminated line is received. */
typedef void (*TransportRxCallback)(const char* line, void* context);

typedef struct Transport Transport;

struct Transport {
    void (*start)(Transport* t, TransportRxCallback cb, void* ctx);
    void (*stop)(Transport* t);
    /** Send len bytes. Each implementation handles its own chunking. */
    void (*send)(Transport* t, const char* data, int len);
    void (*free_fn)(Transport* t);
};

static inline void transport_start(Transport* t, TransportRxCallback cb, void* ctx) {
    if(t && t->start) t->start(t, cb, ctx);
}
static inline void transport_stop(Transport* t) {
    if(t && t->stop) t->stop(t);
}
static inline void transport_send(Transport* t, const char* data, int len) {
    if(t && t->send) t->send(t, data, len);
}
static inline void transport_free(Transport* t) {
    if(t && t->free_fn) t->free_fn(t);
}

Transport* transport_usb_alloc(void);
Transport* transport_bt_alloc(void);
