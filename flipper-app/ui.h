#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

typedef enum {
    ViewIdStatus,
    ViewIdMenu,
    ViewIdPerm,
} ViewId;

typedef enum {
    UiEventEnter,
    UiEventEsc,
    UiEventDown,
    UiEventVoice,
    UiEventOpenMenu,
    UiEventMenuSelect,
    UiEventMenuBack,
    UiEventDismiss,
    UiEventExitApp,
    UiEventPermAllow,
    UiEventPermAlways,
    UiEventPermDeny,
    UiEventPermEsc,
    UiEventBackspace,    // short-press Back: send backspace
    UiEventInterrupt,    // long-press Left: send Ctrl+C interrupt
    UiEventToggleMute,   // long-press Down: toggle sound mute
    UiEventYes,          // long-press Ok: type "yes" + enter
} UiEventType;

typedef enum {
    PoseIdle,      // default: periodic blink
    PoseListening, // eyes up, header shows REC indicator
    PoseThinking,  // eyes right, animated dots above head
    PoseHappy,     // squinted eyes + smile + sparkles, brief bounce (auto-resets)
    PoseAlert,     // wide eyes, blinking ! + horizontal shake (auto-resets)
    PoseSleeping,  // closed eyes, floating z
    PoseExcited,   // arms raised, fast bounce, cycling sparkles (auto-resets)
    PoseWorried,   // shifty eyes, sweat drop, gentle wobble (used on perm screen)
} CharacterPose;

typedef void (*UiEventCallback)(UiEventType event, const char* data, void* context);

// View model structs (stored inside each View's model allocation)
typedef struct {
    char text[22];    // primary status line
    char subtext[22]; // secondary info line (empty = hide)
    bool connected;         // serial/flipper connected
    bool claude_connected;  // claude code session active
    bool muted;             // sound mute active (shown as indicator in header)
    uint8_t pose;           // CharacterPose
    uint8_t anim_frame;     // animation counter (incremented by timer)
    uint8_t transport_mode; // 0 = USB, 1 = BT (shown in header)
    uint8_t rssi_bars;      // BLE signal bars 0–4 (only used when transport_mode == 1)
} StatusModel;

#define MAX_MENU_ITEMS 64
#define MAX_MENU_ITEM_LEN 27

typedef struct {
    int index;
    int count;
    char items[MAX_MENU_ITEMS][MAX_MENU_ITEM_LEN];
} MenuModel;

typedef struct {
    char tool[22];
    char detail[22];
    uint8_t anim_frame;
    uint8_t mode; // 0 = once, 1 = always (persists across requests)
} PermModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    View* status_view;
    View* menu_view;
    View* perm_view;
    FuriTimer* anim_timer;
    UiEventCallback event_callback;
    void* event_context;
    ViewId current_view;
} UiState;

UiState* ui_alloc(Gui* gui);
void ui_free(UiState* ui);
void ui_set_event_callback(UiState* ui, UiEventCallback callback, void* context);
void ui_show_status(UiState* ui, const char* text, bool connected);
void ui_show_status2(UiState* ui, const char* text, const char* subtext, bool connected);
void ui_show_menu(UiState* ui);
void ui_show_listening(UiState* ui);
void ui_set_claude_connected(UiState* ui, bool connected);
void ui_set_pose(UiState* ui, uint8_t pose);
void ui_set_transport_mode(UiState* ui, bool is_bt);
void ui_update_menu(UiState* ui, const char* pipe_delimited);
void ui_show_permission(UiState* ui, const char* tool, const char* detail);
void ui_back_to_status(UiState* ui);
void ui_set_muted(UiState* ui, bool muted);
void ui_run(UiState* ui);
void ui_stop(UiState* ui);
