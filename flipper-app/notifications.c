#include "notifications.h"

// NotificationSequence = const NotificationMessage*[]
// Each sequence is a NULL-terminated array of pointers.
//
// LED design:
//   Idle          = system default (no override, charging LED shows)
//   Permission    = magenta blink (persistent, needs user action)
//   Working       = blue solid
//   Dictation     = red blink
//   Turn complete = green solid
//   Interrupted   = yellow solid
//   Error         = red flash
//   Connected     = green solid
//   Disconnected  = red flash
//   Session end   = yellow flash
//   Ready         = cyan flash

// Success: ascending C5-E5-G5 + vibro + green flash
static const NotificationSequence seq_success = {
    &message_vibro_on,
    &message_red_0,
    &message_green_255,
    &message_blue_0,
    &message_note_c5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_green_0,
    NULL,
};

// Error: low double beep G4-G4 + vibro + backlight + red flash
static const NotificationSequence seq_error = {
    &message_display_backlight_on,
    &message_vibro_on,
    &message_red_255,
    &message_green_0,
    &message_blue_0,
    &message_note_g4,
    &message_delay_100,
    &message_sound_off,
    &message_delay_50,
    &message_note_g4,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

// Alert: single E5 blip + cyan flash (no vibro)
static const NotificationSequence seq_alert = {
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_note_e5,
    &message_delay_50,
    &message_sound_off,
    &message_green_0,
    &message_blue_0,
    NULL,
};

// Permission: rising C5-E5 + double vibro + backlight + magenta blink (persistent)
static const NotificationSequence seq_perm = {
    &message_display_backlight_on,
    &message_vibro_on,
    &message_note_c5,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_50,
    &message_vibro_on,
    &message_note_e5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_blink_start_10,
    &message_blink_set_color_magenta,
    &message_do_not_reset,
    NULL,
};

// Voice request: G5 blip + vibro + brief red flash (immediate button feedback)
static const NotificationSequence seq_voice_request = {
    &message_vibro_on,
    &message_red_255,
    &message_green_0,
    &message_blue_0,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

// Voice start LED-only: red blink (no sound, no vibro — used when button feedback was local)
static const NotificationSequence seq_voice_start_led = {
    &message_blink_start_10,
    &message_blink_set_color_red,
    &message_do_not_reset,
    NULL,
};

// Voice start: high ding (A5) + vibro + red blink (host confirmed)
static const NotificationSequence seq_voice_start = {
    &message_vibro_on,
    &message_note_a5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_blink_start_10,
    &message_blink_set_color_red,
    &message_do_not_reset,
    NULL,
};

// Voice stop: low dong (C5) + vibro + red flash then LED off
static const NotificationSequence seq_voice_stop = {
    &message_vibro_on,
    &message_blink_stop,
    &message_red_255,
    &message_green_0,
    &message_blue_0,
    &message_note_c5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

// Voice stop quiet: LED reset only (no sound, no vibro)
static const NotificationSequence seq_voice_stop_quiet = {
    &message_blink_stop,
    &message_red_0,
    &message_green_0,
    &message_blue_0,
    NULL,
};

// ESC: quick descending E5-C5 + yellow flash (no vibro)
static const NotificationSequence seq_esc = {
    &message_red_255,
    &message_green_255,
    &message_blue_0,
    &message_note_e5,
    &message_delay_50,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_red_0,
    &message_green_0,
    NULL,
};

// Enter sent: short confirm blip + cyan flash (no vibro)
static const NotificationSequence seq_enter = {
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_green_0,
    &message_blue_0,
    NULL,
};

// Command sent: C5 blip + vibro + cyan flash
static const NotificationSequence seq_cmd = {
    &message_vibro_on,
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    &message_green_0,
    &message_blue_0,
    NULL,
};

// Connect: startup ascending C5-E5-G5-C6 + vibro + green solid
static const NotificationSequence seq_connect = {
    &message_vibro_on,
    &message_red_0,
    &message_green_255,
    &message_blue_0,
    &message_note_c5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_note_c6,
    &message_delay_250,
    &message_sound_off,
    &message_vibro_off,
    &message_do_not_reset,
    NULL,
};

// Disconnect: descending C6-G5-E5-C5 + vibro + red flash
static const NotificationSequence seq_disconnect = {
    &message_vibro_on,
    &message_red_255,
    &message_green_0,
    &message_blue_0,
    &message_note_c6,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_c5,
    &message_delay_250,
    &message_sound_off,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

// Interrupt: descending E5-C5 + vibro + yellow solid
static const NotificationSequence seq_interrupt = {
    &message_vibro_on,
    &message_red_255,
    &message_green_255,
    &message_blue_0,
    &message_note_e5,
    &message_delay_50,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    &message_do_not_reset,
    NULL,
};

// Session end: descending C6-G5-E5-C5 + vibro + yellow flash
static const NotificationSequence seq_session_end = {
    &message_vibro_on,
    &message_red_255,
    &message_green_255,
    &message_blue_0,
    &message_note_c6,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_c5,
    &message_delay_250,
    &message_sound_off,
    &message_vibro_off,
    &message_red_0,
    &message_green_0,
    NULL,
};

// Ready: ascending C5-E5-G5 + vibro + cyan flash
static const NotificationSequence seq_ready = {
    &message_vibro_on,
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_note_c5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_green_0,
    &message_blue_0,
    NULL,
};

// LED-only sequences (no sound) for state changes via hooks

// Working: blue solid
static const NotificationSequence seq_led_working = {
    &message_blink_stop,
    &message_red_0,
    &message_green_0,
    &message_blue_255,
    &message_do_not_reset,
    NULL,
};

// Mute on: two descending notes (going quiet) + no LED change
static const NotificationSequence seq_mute_on = {
    &message_note_e5,
    &message_delay_50,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

// Mute off: two ascending notes (coming back) + no LED change
static const NotificationSequence seq_mute_off = {
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

// Enter while working: cyan flash returning to blue solid
static const NotificationSequence seq_enter_working = {
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_green_0,       // drop green → RGB stays 0,0,255 = blue
    &message_do_not_reset,
    NULL,
};

// Cmd while working: cyan flash + vibro returning to blue solid
static const NotificationSequence seq_cmd_working = {
    &message_vibro_on,
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    &message_green_0,       // drop green → RGB stays 0,0,255 = blue
    &message_do_not_reset,
    NULL,
};

// Esc while working: yellow flash returning to blue solid
static const NotificationSequence seq_esc_working = {
    &message_red_255,
    &message_green_255,
    &message_blue_0,        // yellow needs blue off temporarily
    &message_note_e5,
    &message_delay_50,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_red_0,
    &message_green_0,
    &message_blue_255,      // restore blue
    &message_do_not_reset,
    NULL,
};

// Alert while working: cyan flash returning to blue solid (instead of off)
static const NotificationSequence seq_alert_working = {
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_note_e5,
    &message_delay_50,
    &message_sound_off,
    &message_green_0,       // drop green → RGB stays 0,0,255 = blue
    &message_do_not_reset,
    NULL,
};

// LED off: stop blinking and release back to system
static const NotificationSequence seq_led_off = {
    &message_blink_stop,
    &message_red_0,
    &message_green_0,
    &message_blue_0,
    NULL,
};

void notify_play(NotificationApp* app, SoundType sound, bool vibro) {
    if(!app) return;
    (void)vibro; // vibro is embedded in sequences
    const NotificationSequence* seq = NULL;

    switch(sound) {
    case SoundSuccess:        seq = &seq_success; break;
    case SoundError:          seq = &seq_error; break;
    case SoundAlert:          seq = &seq_alert; break;
    case SoundVoiceRequest:   seq = &seq_voice_request; break;
    case SoundVoiceStart:     seq = &seq_voice_start; break;
    case SoundVoiceStartLed:  seq = &seq_voice_start_led; break;
    case SoundVoiceStop:      seq = &seq_voice_stop; break;
    case SoundVoiceStopQuiet: seq = &seq_voice_stop_quiet; break;
    case SoundEsc:            seq = &seq_esc; break;
    case SoundEnter:          seq = &seq_enter; break;
    case SoundCmd:            seq = &seq_cmd; break;
    case SoundPerm:           seq = &seq_perm; break;
    case SoundConnect:        seq = &seq_connect; break;
    case SoundDisconnect:     seq = &seq_disconnect; break;
    case SoundLedWorking:     seq = &seq_led_working; break;
    case SoundLedOff:         seq = &seq_led_off; break;
    case SoundInterrupt:      seq = &seq_interrupt; break;
    case SoundSessionEnd:     seq = &seq_session_end; break;
    case SoundReady:          seq = &seq_ready; break;
    case SoundMuteOn:         seq = &seq_mute_on; break;
    case SoundMuteOff:        seq = &seq_mute_off; break;
    case SoundAlertWorking:   seq = &seq_alert_working; break;
    case SoundEnterWorking:   seq = &seq_enter_working; break;
    case SoundEscWorking:     seq = &seq_esc_working; break;
    case SoundCmdWorking:     seq = &seq_cmd_working; break;
    }

    if(seq) {
        notification_message(app, seq);
    }
}
