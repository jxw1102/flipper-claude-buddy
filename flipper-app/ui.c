#include "ui.h"
#include <gui/elements.h>
#include <furi_hal_bt.h>
#include <string.h>

// Default slash commands (shown until bridge sends an updated list)
static const char* default_menu_items[] = {
    "/btw",
    "/clear",
    "/compact",
    "/model",
    "/effort",
    "/config",
    "/usage",
    "/doctor",
    "/help",
    "/init",
    "/login",
    "/logout",
    "/pr-comments",
    "/review",
    "/status",
};
#define DEFAULT_MENU_COUNT 15

// ── Layout ───────────────────────────────────────────────────────

#define HDR_H  9   // inverted header bar height (y 0..8)
#define FTR_Y  53  // footer separator y

// ── Animation Timer ──────────────────────────────────────────────

static void anim_tick(void* context) {
    UiState* ui = context;
    StatusModel* sm = view_get_model(ui->status_view);
    sm->anim_frame++;
    // Auto-reset transient poses after ~3s (20 frames * 150ms)
    if((sm->pose == PoseHappy || sm->pose == PoseAlert || sm->pose == PoseExcited) &&
       sm->anim_frame > 20) {
        sm->pose = PoseIdle;
        sm->anim_frame = 0;
    }
    // Update BLE RSSI bars every ~5s (33 frames × 150ms)
    if(sm->anim_frame % 33 == 0) {
        if(sm->transport_mode == 1 && sm->connected) {
            float rssi = furi_hal_bt_get_rssi();
            if(rssi > -65.0f)      sm->rssi_bars = 4;
            else if(rssi > -75.0f) sm->rssi_bars = 3;
            else if(rssi > -85.0f) sm->rssi_bars = 2;
            else                   sm->rssi_bars = 1;
        } else {
            sm->rssi_bars = 0;
        }
    }
    view_commit_model(ui->status_view, true);

    PermModel* pm = view_get_model(ui->perm_view);
    pm->anim_frame++;
    view_commit_model(ui->perm_view, true);

}

// ── Button Hints ─────────────────────────────────────────────────

// ◄ triangle + label, bottom-left
static void hint_left(Canvas* canvas, const char* label) {
    canvas_set_font(canvas, FontSecondary);
    const int y = 59;
    canvas_draw_line(canvas, 0, y, 6, y);
    canvas_draw_line(canvas, 2, y - 1, 6, y - 1);
    canvas_draw_line(canvas, 2, y + 1, 6, y + 1);
    canvas_draw_line(canvas, 4, y - 2, 6, y - 2);
    canvas_draw_line(canvas, 4, y + 2, 6, y + 2);
    canvas_draw_dot(canvas, 6, y - 3);
    canvas_draw_dot(canvas, 6, y + 3);
    canvas_draw_str_aligned(canvas, 9, y + 1, AlignLeft, AlignCenter, label);
}

// ► triangle + label, bottom-right
static void hint_right(Canvas* canvas, const char* label) {
    canvas_set_font(canvas, FontSecondary);
    int lw = (int)canvas_string_width(canvas, label);
    const int y = 59;
    int bx = 127 - lw - 2 - 7;
    canvas_draw_line(canvas, bx, y, bx + 6, y);
    canvas_draw_line(canvas, bx, y - 1, bx + 4, y - 1);
    canvas_draw_line(canvas, bx, y + 1, bx + 4, y + 1);
    canvas_draw_line(canvas, bx, y - 2, bx + 2, y - 2);
    canvas_draw_line(canvas, bx, y + 2, bx + 2, y + 2);
    canvas_draw_dot(canvas, bx, y - 3);
    canvas_draw_dot(canvas, bx, y + 3);
    canvas_draw_str(canvas, 127 - lw, 63, label);
}

// ● circle + label, bottom-center
static void hint_ok(Canvas* canvas, const char* label) {
    canvas_set_font(canvas, FontSecondary);
    int lw = (int)canvas_string_width(canvas, label);
    int total = 6 + 2 + lw;
    int ix = 64 - total / 2 + 3;
    const int y = 59;
    canvas_draw_circle(canvas, ix, y, 4);
    canvas_draw_disc(canvas, ix, y, 2);
    canvas_draw_str(canvas, ix + 6, 63, label);
}

// ↩ arrow + label, bottom-right
static void hint_back(Canvas* canvas, const char* label) {
    canvas_set_font(canvas, FontSecondary);
    int lw = (int)canvas_string_width(canvas, label);
    int ax = 127 - lw - 11;
    int ay = 57;
    canvas_draw_line(canvas, ax + 3, ay + 4, ax + 7, ay + 4);
    canvas_draw_line(canvas, ax + 7, ay, ax + 7, ay + 4);
    canvas_draw_line(canvas, ax, ay, ax + 7, ay);
    canvas_draw_line(canvas, ax, ay, ax + 2, ay - 2);
    canvas_draw_line(canvas, ax, ay, ax + 2, ay + 2);
    canvas_draw_str(canvas, 127 - lw, 63, label);
}

// ── Shared Drawing ───────────────────────────────────────────────

// Header bar with bottom separator (dark = inverted white-on-black)
static void draw_header(Canvas* canvas, const char* title, bool dark) {
    if(dark) {
        canvas_draw_box(canvas, 0, 0, 128, HDR_H);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str_aligned(canvas, 64, HDR_H / 2 + 1, AlignCenter, AlignCenter, title);
    if(dark) {
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_line(canvas, 0, HDR_H, 127, HDR_H);
    }
}

// Footer separator line
static void draw_footer_sep(Canvas* canvas) {
    canvas_draw_line(canvas, 0, FTR_Y, 127, FTR_Y);
}

// Vertical scrollbar on right edge
static void draw_scrollbar(Canvas* canvas, int idx, int count, int y0, int y1) {
    if(count <= 1) return;
    int h = y1 - y0;
    int bh = h / count;
    if(bh < 4) bh = 4;
    int by = y0 + (idx * (h - bh)) / (count - 1);
    canvas_draw_line(canvas, 126, y0, 126, y1); // track
    canvas_draw_box(canvas, 125, by, 3, bh);    // thumb
}

// Claude character (original Claude Code icon design + animated poses)
// Body: 18x10 rectangle, ears: dots, legs: 4 lines, eyes: 2x3 white cutouts
// Small + sparkle at (x, y)
static void draw_sparkle(Canvas* canvas, int x, int y) {
    canvas_draw_dot(canvas, x, y);
    canvas_draw_dot(canvas, x - 1, y);
    canvas_draw_dot(canvas, x + 1, y);
    canvas_draw_dot(canvas, x, y - 1);
    canvas_draw_dot(canvas, x, y + 1);
}

static void draw_claude(Canvas* canvas, int cx, int cy, uint8_t pose, uint8_t frame) {
    // ── Pose-specific body offsets ──
    if(pose == PoseHappy) {
        // Gentle bounce: up for first 4 frames
        if(frame < 2) cy -= 2;
        else if(frame < 4) cy -= 1;
    }
    if(pose == PoseExcited) {
        // Fast bounce: repeating 4-frame cycle
        int ph = frame % 4;
        if(ph == 0) cy -= 3;
        else if(ph <= 2) cy -= 1;
    }
    if(pose == PoseAlert) {
        // Horizontal shake: ±1px alternating every 2 frames
        cx += (frame % 4 < 2) ? 1 : -1;
    }
    if(pose == PoseWorried) {
        // Gentle horizontal wobble: ±1px every 3 frames
        cx += (frame % 6 < 3) ? 1 : -1;
    }

    canvas_set_color(canvas, ColorBlack);

    // Body (18x10 rectangle - the Claude Code icon)
    canvas_draw_box(canvas, cx, cy, 18, 10);

    // Ears (single dots on each side)
    canvas_draw_dot(canvas, cx - 1, cy + 4);
    canvas_draw_dot(canvas, cx + 18, cy + 4);

    // Legs (4 lines of 2px, 1px gap below body)
    canvas_draw_line(canvas, cx + 4, cy + 11, cx + 4, cy + 12);
    canvas_draw_line(canvas, cx + 6, cy + 11, cx + 6, cy + 12);
    canvas_draw_line(canvas, cx + 12, cy + 11, cx + 12, cy + 12);
    canvas_draw_line(canvas, cx + 14, cy + 11, cx + 14, cy + 12);

    // ── Eyes (white cutouts, vary by pose & frame) ──
    canvas_set_color(canvas, ColorWhite);

    switch(pose) {
    case PoseIdle:
    default:
        if(frame % 20 >= 18) {
            // Blink: thin horizontal lines
            canvas_draw_line(canvas, cx + 4, cy + 4, cx + 5, cy + 4);
            canvas_draw_line(canvas, cx + 12, cy + 4, cx + 13, cy + 4);
        } else {
            // Normal eyes
            canvas_draw_box(canvas, cx + 4, cy + 3, 2, 3);
            canvas_draw_box(canvas, cx + 12, cy + 3, 2, 3);
        }
        break;

    case PoseListening:
        // Eyes shifted up (looking toward mic)
        canvas_draw_box(canvas, cx + 4, cy + 2, 2, 3);
        canvas_draw_box(canvas, cx + 12, cy + 2, 2, 3);
        break;

    case PoseThinking:
        // Eyes shifted right (looking at status text)
        canvas_draw_box(canvas, cx + 5, cy + 3, 2, 3);
        canvas_draw_box(canvas, cx + 13, cy + 3, 2, 3);
        break;

    case PoseHappy:
    case PoseExcited:
        // Squinted happy eyes (shorter) + smile
        canvas_draw_box(canvas, cx + 4, cy + 3, 2, 2);
        canvas_draw_box(canvas, cx + 12, cy + 3, 2, 2);
        canvas_draw_dot(canvas, cx + 7, cy + 7);
        canvas_draw_dot(canvas, cx + 10, cy + 7);
        canvas_draw_line(canvas, cx + 8, cy + 8, cx + 9, cy + 8);
        break;

    case PoseAlert:
        // Wide eyes (taller)
        canvas_draw_box(canvas, cx + 4, cy + 2, 2, 4);
        canvas_draw_box(canvas, cx + 12, cy + 2, 2, 4);
        break;

    case PoseSleeping:
        // Closed eyes (horizontal lines)
        canvas_draw_line(canvas, cx + 4, cy + 4, cx + 5, cy + 4);
        canvas_draw_line(canvas, cx + 12, cy + 4, cx + 13, cy + 4);
        break;

    case PoseWorried: {
        // Shifty eyes: cycle left → center → right every 5 frames
        int shift = ((frame / 5) % 3) - 1; // -1, 0, +1
        canvas_draw_box(canvas, cx + 4 + shift, cy + 3, 2, 3);
        canvas_draw_box(canvas, cx + 12 + shift, cy + 3, 2, 3);
        break;
    }
    }

    canvas_set_color(canvas, ColorBlack);

    // ── Extra animations outside the body ──
    switch(pose) {
    case PoseThinking: {
        // Animated thinking dots above character
        int phase = (frame / 4) % 4;
        if(phase >= 1) canvas_draw_dot(canvas, cx + 10, cy - 3);
        if(phase >= 2) canvas_draw_dot(canvas, cx + 13, cy - 3);
        if(phase >= 3) canvas_draw_dot(canvas, cx + 16, cy - 3);
        break;
    }
    case PoseAlert: {
        // Blinking exclamation mark above character
        if(frame % 6 < 3) {
            canvas_draw_line(canvas, cx + 8, cy - 5, cx + 8, cy - 3);
            canvas_draw_dot(canvas, cx + 8, cy - 1);
        }
        break;
    }
    case PoseSleeping: {
        // Floating "z" above character
        canvas_set_font(canvas, FontSecondary);
        int zy = cy - 2 - ((frame % 10) / 2);
        if(zy > HDR_H) {
            canvas_draw_str(canvas, cx + 16, zy, "z");
        }
        break;
    }
    case PoseHappy: {
        // Sparkle stars appear on each side after the bounce settles
        if(frame >= 4) {
            if(frame % 5 < 4)
                draw_sparkle(canvas, cx - 3, cy + 5);
            if((frame + 2) % 5 < 4)
                draw_sparkle(canvas, cx + 21, cy + 5);
        }
        break;
    }
    case PoseExcited: {
        // Arms raised (lines from body sides pointing up-outward)
        canvas_draw_line(canvas, cx - 1, cy + 3, cx - 3, cy + 1);
        canvas_draw_line(canvas, cx + 18, cy + 3, cx + 20, cy + 1);
        // 4 sparkles cycling in with staggered phases
        const int8_t sp_dx[] = {-4, 21, 5, 14};
        const int8_t sp_dy[] = {4, 4, -5, -5};
        for(int i = 0; i < 4; i++) {
            if((frame + i * 3) % 8 < 6)
                draw_sparkle(canvas, cx + sp_dx[i], cy + sp_dy[i]);
        }
        break;
    }
    case PoseWorried: {
        // Sweat drop (upper-right of body, animated drip)
        int drop_y = cy - 1 + (frame % 4) / 2; // slowly slides down
        canvas_draw_dot(canvas, cx + 17, drop_y);
        canvas_draw_dot(canvas, cx + 16, drop_y + 1);
        canvas_draw_dot(canvas, cx + 18, drop_y + 1);
        canvas_draw_dot(canvas, cx + 17, drop_y + 2);
        break;
    }
    default:
        break;
    }
}

// ── Status View ──────────────────────────────────────────────────

static void status_draw(Canvas* canvas, void* model) {
    if(!canvas || !model) return;
    StatusModel* m = model;
    canvas_clear(canvas);

    // ── Header bar (light theme with bottom separator) ──
    canvas_set_font(canvas, FontSecondary);

    if(m->pose == PoseListening) {
        // Pulsing recording indicator centered in header
        int tw = (int)canvas_string_width(canvas, "REC");
        int total_w = 5 + 3 + tw;
        int ox = 64 - total_w / 2;
        if(m->anim_frame % 6 < 3)
            canvas_draw_disc(canvas, ox + 2, 5, 2);
        else
            canvas_draw_circle(canvas, ox + 2, 5, 2);
        canvas_draw_str(canvas, ox + 8, 8, "REC");
    } else {
        // Centered ▲ Dictation hint
        int tw = (int)canvas_string_width(canvas, "Dictation");
        int total_w = 5 + 3 + tw;
        int ox = 64 - total_w / 2;
        canvas_draw_dot(canvas, ox + 2, 3);
        canvas_draw_line(canvas, ox + 1, 4, ox + 3, 4);
        canvas_draw_line(canvas, ox, 5, ox + 4, 5);
        canvas_draw_str(canvas, ox + 8, 8, "Dictation");
    }

    // Mute indicator — small 'M' at top-left when sound is off
    if(m->muted) {
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, 1, 8, "M");
    }

    // Transport mode — only when connected
    if(m->connected) {
        if(m->transport_mode) {
            // BLE: draw signal bars (4 bars, increasing height, at right of header)
            // Bar positions x: 113, 115, 117, 119 — just left of the claude dot (124)
            const int bx = 113;
            const int by = 6; // bottom y of all bars
            const int heights[4] = {2, 3, 4, 5};
            for(int i = 0; i < 4; i++) {
                int top = by - heights[i] + 1;
                if(i < (int)m->rssi_bars) {
                    canvas_draw_line(canvas, bx + i * 2, top, bx + i * 2, by);
                } else {
                    // Empty bar: just top and bottom dots
                    canvas_draw_dot(canvas, bx + i * 2, top);
                    canvas_draw_dot(canvas, bx + i * 2, by);
                }
            }
        } else {
            canvas_set_font(canvas, FontKeyboard);
            canvas_draw_str_aligned(
                canvas,
                119, HDR_H / 2 + 1,
                AlignRight, AlignCenter,
                "USB");
            canvas_set_font(canvas, FontSecondary);
        }
    }

    // Connectivity dot (Claude session) — right edge
    if(m->claude_connected)
        canvas_draw_disc(canvas, 124, 4, 2);
    else
        canvas_draw_circle(canvas, 124, 4, 2);

    // Header bottom separator
    canvas_draw_line(canvas, 0, HDR_H, 127, HDR_H);

    // ── Content area ──
    // Claude character (left, vertically centered)
    draw_claude(canvas, 4, 22, m->pose, m->anim_frame);

    // Status text (right of character)
    const char* main_text = m->connected ? "Ready" : "No connection";
    if(m->text[0] != '\0') main_text = m->text;
    bool has_sub = m->subtext[0] != '\0';

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 77, has_sub ? 25 : 31, AlignCenter, AlignCenter, main_text);
    if(has_sub) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 77, 37, AlignCenter, AlignCenter, m->subtext);
    }

    // ── Footer ──
    draw_footer_sep(canvas);
    hint_left(canvas, "Esc");
    hint_ok(canvas, "Enter");
    hint_right(canvas, "Cmds");
}

static bool status_input(InputEvent* event, void* context) {
    if(!event || !context) return false;
    UiState* ui = context;
    if(event->type != InputTypeShort && event->type != InputTypeLong) return false;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        if(ui->event_callback) ui->event_callback(UiEventEnter, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyLeft && event->type == InputTypeShort) {
        if(ui->event_callback) ui->event_callback(UiEventEsc, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyLeft && event->type == InputTypeLong) {
        if(ui->event_callback) ui->event_callback(UiEventInterrupt, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyUp && event->type == InputTypeShort) {
        if(ui->event_callback) ui->event_callback(UiEventVoice, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyDown && event->type == InputTypeShort) {
        if(ui->event_callback) ui->event_callback(UiEventDown, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyDown && event->type == InputTypeLong) {
        if(ui->event_callback) ui->event_callback(UiEventToggleMute, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyRight && event->type == InputTypeShort) {
        if(ui->event_callback) ui->event_callback(UiEventOpenMenu, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        if(ui->event_callback) ui->event_callback(UiEventBackspace, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyBack && event->type == InputTypeLong) {
        if(ui->event_callback) ui->event_callback(UiEventExitApp, NULL, ui->event_context);
        return true;
    }
    return false;
}

// ── Menu View ────────────────────────────────────────────────────

static void menu_draw(Canvas* canvas, void* model) {
    if(!canvas || !model) return;
    MenuModel* m = model;
    canvas_clear(canvas);

    // ── Header ──
    draw_header(canvas, "COMMANDS", false);

    if(m->count == 0) {
        // Empty state with bordered message
        canvas_draw_rframe(canvas, 10, 18, 108, 28, 4);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "No commands yet");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Start Claude Code");
    } else {
        const int visible = 5;
        const int item_h = 8;
        const int list_y = 12;

        // Scroll window so selected item is always visible
        int start = 0;
        if(m->index >= visible) start = m->index - visible + 1;

        for(int vi = 0; vi < visible && start + vi < m->count; vi++) {
            int i = start + vi;
            int by = list_y + vi * item_h;

            canvas_set_font(canvas, FontSecondary);

            if(i == m->index) {
                // Selected: rounded inverted highlight
                canvas_draw_rbox(canvas, 1, by, 121, item_h, 1);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(canvas, 5, by + 6, m->items[i]);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_str(canvas, 5, by + 6, m->items[i]);
            }
        }

        // Scrollbar
        draw_scrollbar(canvas, m->index, m->count, list_y, list_y + visible * item_h);
    }

    // ── Footer ──
    draw_footer_sep(canvas);
    hint_ok(canvas, "Send");
    hint_back(canvas, "Back");
}

static bool menu_input(InputEvent* event, void* context) {
    if(!event || !context) return false;
    UiState* ui = context;
    if(event->type != InputTypeShort) return false;

    MenuModel* m = view_get_model(ui->menu_view);

    if(event->key == InputKeyUp) {
        if(m->count > 0) m->index = (m->index > 0) ? m->index - 1 : m->count - 1;
        view_commit_model(ui->menu_view, true);
        return true;
    }
    if(event->key == InputKeyDown) {
        if(m->count > 0) m->index = (m->index < m->count - 1) ? m->index + 1 : 0;
        view_commit_model(ui->menu_view, true);
        return true;
    }
    if(event->key == InputKeyOk) {
        if(ui->event_callback && m->count > 0) {
            ui->event_callback(
                UiEventMenuSelect, m->items[m->index], ui->event_context);
        }
        return true;
    }
    if(event->key == InputKeyBack) {
        if(ui->event_callback) ui->event_callback(UiEventMenuBack, NULL, ui->event_context);
        return true;
    }
    return false;
}

// ── Permission View ──────────────────────────────────────────────

static void perm_draw(Canvas* canvas, void* model) {
    if(!canvas || !model) return;
    PermModel* m = model;
    canvas_clear(canvas);

    // Header
    draw_header(canvas, "PERMISSION", true);

    // Content area: HDR_H(9) to FTR_Y(53), midpoint = 31
    // Three lines centered vertically: tool, detail, mode toggle
    int cy = (HDR_H + FTR_Y) / 2; // 31
    // Claude character (PoseWorried) on the left
    draw_claude(canvas, 4, cy - 9, PoseWorried, m->anim_frame);

    // Tool name — right half, centered at x=90
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 77, cy - 10, AlignCenter, AlignCenter, m->tool);

    // Detail — right half
    if(m->detail[0] != '\0') {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 77, cy, AlignCenter, AlignCenter, m->detail);
    }

    // Mode toggle indicator (in content area)
    canvas_set_font(canvas, FontSecondary);
    const char* mode_label = m->mode ? "Always" : "Once";
    int mw = (int)canvas_string_width(canvas, mode_label);
    int mx = 77 - (mw + 9) / 2; // center the triangle + label
    int my = cy + 10;
    // Small ▼ triangle
    canvas_draw_line(canvas, mx, my, mx + 4, my);
    canvas_draw_line(canvas, mx + 1, my + 1, mx + 3, my + 1);
    canvas_draw_dot(canvas, mx + 2, my + 2);
    canvas_draw_str(canvas, mx + 7, my + 4, mode_label);

    // Dark footer (hints drawn 1px higher to avoid bottom clipping)
    canvas_draw_box(canvas, 0, FTR_Y, 128, 64 - FTR_Y);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    {
        // ◄ Esc (left)
        const int y = 58;
        canvas_draw_line(canvas, 0, y, 6, y);
        canvas_draw_line(canvas, 2, y - 1, 6, y - 1);
        canvas_draw_line(canvas, 2, y + 1, 6, y + 1);
        canvas_draw_line(canvas, 4, y - 2, 6, y - 2);
        canvas_draw_line(canvas, 4, y + 2, 6, y + 2);
        canvas_draw_dot(canvas, 6, y - 3);
        canvas_draw_dot(canvas, 6, y + 3);
        canvas_draw_str_aligned(canvas, 9, y + 1, AlignLeft, AlignCenter, "Cancel");
    }
    {
        // ● OK (center)
        int lw = (int)canvas_string_width(canvas, "OK");
        int total = 6 + 2 + lw;
        int ix = 64 - total / 2 + 3;
        const int y = 58;
        canvas_draw_circle(canvas, ix, y, 4);
        canvas_draw_disc(canvas, ix, y, 2);
        canvas_draw_str(canvas, ix + 6, 62, "OK");
    }
    {
        // ↩ Deny (right)
        int lw = (int)canvas_string_width(canvas, "Deny");
        int ax = 127 - lw - 11;
        int ay = 56;
        canvas_draw_line(canvas, ax + 3, ay + 4, ax + 7, ay + 4);
        canvas_draw_line(canvas, ax + 7, ay, ax + 7, ay + 4);
        canvas_draw_line(canvas, ax, ay, ax + 7, ay);
        canvas_draw_line(canvas, ax, ay, ax + 2, ay - 2);
        canvas_draw_line(canvas, ax, ay, ax + 2, ay + 2);
        canvas_draw_str(canvas, 127 - lw, 62, "Deny");
    }
    canvas_set_color(canvas, ColorBlack);
}

static bool perm_input(InputEvent* event, void* context) {
    if(!event || !context) return false;
    UiState* ui = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk) {
        PermModel* m = view_get_model(ui->perm_view);
        uint8_t mode = m->mode;
        view_commit_model(ui->perm_view, false);
        if(ui->event_callback) {
            ui->event_callback(
                mode ? UiEventPermAlways : UiEventPermAllow, NULL, ui->event_context);
        }
        return true;
    }
    if(event->key == InputKeyDown) {
        PermModel* m = view_get_model(ui->perm_view);
        m->mode = m->mode ? 0 : 1;
        view_commit_model(ui->perm_view, true);
        return true;
    }
    if(event->key == InputKeyBack) {
        if(ui->event_callback) ui->event_callback(UiEventPermDeny, NULL, ui->event_context);
        return true;
    }
    if(event->key == InputKeyLeft) {
        if(ui->event_callback) ui->event_callback(UiEventPermEsc, NULL, ui->event_context);
        return true;
    }
    return false;
}

// ── Public API ───────────────────────────────────────────────────

UiState* ui_alloc(Gui* gui) {
    UiState* ui = malloc(sizeof(UiState));
    furi_check(ui != NULL);
    memset(ui, 0, sizeof(UiState));

    ui->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(ui->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    // Status view
    ui->status_view = view_alloc();
    view_allocate_model(ui->status_view, ViewModelTypeLockFree, sizeof(StatusModel));
    view_set_draw_callback(ui->status_view, status_draw);
    view_set_input_callback(ui->status_view, status_input);
    view_set_context(ui->status_view, ui);
    view_dispatcher_add_view(ui->view_dispatcher, ViewIdStatus, ui->status_view);

    // Menu view
    ui->menu_view = view_alloc();
    view_allocate_model(ui->menu_view, ViewModelTypeLockFree, sizeof(MenuModel));
    view_set_draw_callback(ui->menu_view, menu_draw);
    view_set_input_callback(ui->menu_view, menu_input);
    view_set_context(ui->menu_view, ui);
    {
        MenuModel* m = view_get_model(ui->menu_view);
        for(int i = 0; i < DEFAULT_MENU_COUNT && i < MAX_MENU_ITEMS; i++) {
            strncpy(m->items[i], default_menu_items[i], MAX_MENU_ITEM_LEN - 1);
            m->items[i][MAX_MENU_ITEM_LEN - 1] = '\0';
        }
        m->count = DEFAULT_MENU_COUNT;
        m->index = 0;
    }
    view_dispatcher_add_view(ui->view_dispatcher, ViewIdMenu, ui->menu_view);

    // Permission view
    ui->perm_view = view_alloc();
    view_allocate_model(ui->perm_view, ViewModelTypeLockFree, sizeof(PermModel));
    view_set_draw_callback(ui->perm_view, perm_draw);
    view_set_input_callback(ui->perm_view, perm_input);
    view_set_context(ui->perm_view, ui);
    view_dispatcher_add_view(ui->view_dispatcher, ViewIdPerm, ui->perm_view);

    // Animation timer (150ms tick for character animations)
    ui->anim_timer = furi_timer_alloc(anim_tick, FuriTimerTypePeriodic, ui);
    furi_timer_start(ui->anim_timer, 150);

    // Start on status
    ui->current_view = ViewIdStatus;
    view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdStatus);

    return ui;
}

void ui_free(UiState* ui) {
    if(!ui) return;
    furi_timer_stop(ui->anim_timer);
    furi_timer_free(ui->anim_timer);
    view_dispatcher_remove_view(ui->view_dispatcher, ViewIdStatus);
    view_dispatcher_remove_view(ui->view_dispatcher, ViewIdMenu);
    view_dispatcher_remove_view(ui->view_dispatcher, ViewIdPerm);
    view_free(ui->status_view);
    view_free(ui->menu_view);
    view_free(ui->perm_view);
    view_dispatcher_free(ui->view_dispatcher);
    free(ui);
}

void ui_set_event_callback(UiState* ui, UiEventCallback callback, void* context) {
    if(!ui) return;
    ui->event_callback = callback;
    ui->event_context = context;
}

void ui_show_status(UiState* ui, const char* text, bool connected) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    m->connected = connected;
    if(text) {
        strncpy(m->text, text, sizeof(m->text) - 1);
        m->text[sizeof(m->text) - 1] = '\0';
    } else {
        m->text[0] = '\0';
    }
    m->subtext[0] = '\0';
    if(!connected) {
        m->pose = PoseSleeping;
        m->anim_frame = 0;
    }
    view_commit_model(ui->status_view, true);
    if(ui->current_view != ViewIdMenu) {
        ui->current_view = ViewIdStatus;
        view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdStatus);
    }
}

void ui_show_status2(UiState* ui, const char* text, const char* subtext, bool connected) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    m->connected = connected;
    if(text) {
        strncpy(m->text, text, sizeof(m->text) - 1);
        m->text[sizeof(m->text) - 1] = '\0';
    } else {
        m->text[0] = '\0';
    }
    if(subtext) {
        strncpy(m->subtext, subtext, sizeof(m->subtext) - 1);
        m->subtext[sizeof(m->subtext) - 1] = '\0';
    } else {
        m->subtext[0] = '\0';
    }
    if(!connected) {
        m->pose = PoseSleeping;
        m->anim_frame = 0;
    }
    view_commit_model(ui->status_view, true);
    if(ui->current_view != ViewIdMenu) {
        ui->current_view = ViewIdStatus;
        view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdStatus);
    }
}

void ui_show_menu(UiState* ui) {
    if(!ui) return;
    MenuModel* m = view_get_model(ui->menu_view);
    m->index = 0;
    view_commit_model(ui->menu_view, true);
    ui->current_view = ViewIdMenu;
    view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdMenu);
}

void ui_show_listening(UiState* ui) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    m->connected = true;
    strncpy(m->text, "Listening...", sizeof(m->text) - 1);
    m->text[sizeof(m->text) - 1] = '\0';
    m->subtext[0] = '\0';
    m->pose = PoseListening;
    m->anim_frame = 0;
    view_commit_model(ui->status_view, true);
    ui->current_view = ViewIdStatus;
    view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdStatus);
}

void ui_set_claude_connected(UiState* ui, bool connected) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    m->claude_connected = connected;
    view_commit_model(ui->status_view, true);
}

void ui_set_transport_mode(UiState* ui, bool is_bt) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    m->transport_mode = is_bt ? 1 : 0;
    view_commit_model(ui->status_view, true);
}

void ui_set_pose(UiState* ui, uint8_t pose) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    // PoseListening is sticky: only voice_stop (PoseIdle) can clear it
    if(m->pose == PoseListening && pose != PoseIdle && pose != PoseListening) {
        view_commit_model(ui->status_view, false);
        return;
    }
    m->pose = pose;
    m->anim_frame = 0;
    view_commit_model(ui->status_view, true);
}

void ui_update_menu(UiState* ui, const char* pipe_delimited) {
    if(!ui || !pipe_delimited) return;
    MenuModel* m = view_get_model(ui->menu_view);
    m->count = 0;
    m->index = 0;

    const char* p = pipe_delimited;
    while(*p && m->count < MAX_MENU_ITEMS) {
        const char* sep = strchr(p, '|');
        int len = sep ? (int)(sep - p) : (int)strlen(p);
        if(len > MAX_MENU_ITEM_LEN - 1) len = MAX_MENU_ITEM_LEN - 1;
        if(len > 0) {
            memcpy(m->items[m->count], p, len);
            m->items[m->count][len] = '\0';
            m->count++;
        }
        if(!sep) break;
        p = sep + 1;
    }

    view_commit_model(ui->menu_view, false);
}

void ui_show_permission(UiState* ui, const char* tool, const char* detail) {
    if(!ui) return;
    PermModel* m = view_get_model(ui->perm_view);
    if(tool) {
        strncpy(m->tool, tool, sizeof(m->tool) - 1);
        m->tool[sizeof(m->tool) - 1] = '\0';
    } else {
        m->tool[0] = '\0';
    }
    if(detail) {
        strncpy(m->detail, detail, sizeof(m->detail) - 1);
        m->detail[sizeof(m->detail) - 1] = '\0';
    } else {
        m->detail[0] = '\0';
    }
    m->anim_frame = 0;
    view_commit_model(ui->perm_view, true);
    ui->current_view = ViewIdPerm;
    view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdPerm);
}

void ui_back_to_status(UiState* ui) {
    if(!ui) return;
    ui->current_view = ViewIdStatus;
    view_dispatcher_switch_to_view(ui->view_dispatcher, ViewIdStatus);
}

void ui_set_muted(UiState* ui, bool muted) {
    if(!ui) return;
    StatusModel* m = view_get_model(ui->status_view);
    m->muted = muted;
    view_commit_model(ui->status_view, true);
}

void ui_stop(UiState* ui) {
    if(!ui) return;
    view_dispatcher_stop(ui->view_dispatcher);
}

void ui_run(UiState* ui) {
    if(!ui) return;
    view_dispatcher_run(ui->view_dispatcher);
}
