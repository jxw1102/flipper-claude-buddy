#pragma once

#include <stdint.h>
#include <stdbool.h>

#define PROTOCOL_MAX_MSG_LEN 512
#define PROTOCOL_MAX_FIELD_LEN 64

typedef enum {
    MsgTypeUnknown = 0,
    // Host -> Flipper
    MsgTypeNotify,
    MsgTypePing,
    MsgTypeStatus,
    MsgTypeMenu,
    MsgTypeState,
    MsgTypePerm,
    // Flipper -> Host
    MsgTypeCmd,
    MsgTypeEnter,
    MsgTypeEsc,
    MsgTypeDown,
    MsgTypeVoice,
    MsgTypeHello,
    MsgTypePong,
    MsgTypeYes,
} MsgType;

typedef struct {
    MsgType type;
    char sound[32];
    bool vibro;
    char text[PROTOCOL_MAX_FIELD_LEN];   // line1
    char text2[PROTOCOL_MAX_FIELD_LEN];  // line2 (subtext)
    char menu_data[512]; // pipe-delimited menu items
    bool claude_connected; // claude code session state
} ProtocolMessage;

// Parse a JSON line into a ProtocolMessage. Returns true on success.
bool protocol_parse(const char* json_line, ProtocolMessage* msg);

// Build outgoing JSON messages into buf. Returns bytes written.
int protocol_build_hello(char* buf, int buf_size);
int protocol_build_cmd(char* buf, int buf_size, const char* text);
int protocol_build_enter(char* buf, int buf_size);
int protocol_build_esc(char* buf, int buf_size);
int protocol_build_voice(char* buf, int buf_size);
int protocol_build_down(char* buf, int buf_size);
int protocol_build_pong(char* buf, int buf_size);
int protocol_build_perm_resp(char* buf, int buf_size, bool allow, bool always, bool esc);
int protocol_build_interrupt(char* buf, int buf_size);
int protocol_build_backspace(char* buf, int buf_size);
int protocol_build_yes(char* buf, int buf_size);
