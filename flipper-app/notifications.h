#pragma once

#include <notification/notification.h>
#include <notification/notification_messages.h>

typedef enum {
    SoundSuccess,        // Ascending C5-E5-G5 + green solid
    SoundError,          // Low double beep G4-G4 + red flash
    SoundAlert,          // Single E5 blip + cyan flash
    SoundVoiceRequest,   // G5 blip + red flash (immediate on button press, before host confirms)
    SoundVoiceStart,     // A5 ding + red blink (host confirmed dictation started)
    SoundVoiceStartLed,  // red blink only — no sound, no vibro (used by host when button feedback is local)
    SoundVoiceStop,      // C5 dong + vibro + red flash then LED off
    SoundVoiceStopQuiet, // C5 dong + LED reset (no vibro)
    SoundEsc,            // Quick descending E5-C5 + yellow flash
    SoundEnter,          // Short confirm blip (no LED)
    SoundCmd,            // C5 blip + vibro + cyan flash
    SoundPerm,           // Rising C5-E5 + magenta blink
    SoundConnect,        // Startup chord C5-E5-G5-C6 + green solid
    SoundDisconnect,     // Shutdown chord C6-G5-E5-C5 + red flash
    SoundLedWorking,     // Blue solid LED (no sound)
    SoundLedOff,         // LED off (no sound)
    SoundInterrupt,      // Descending E5-C5 + yellow solid
    SoundSessionEnd,     // Descending C6-G5-E5-C5 + yellow flash
    SoundReady,          // Ascending C5-E5-G5 + cyan flash
    SoundMuteOn,         // Descending two-note blip (going quiet)
    SoundMuteOff,        // Ascending two-note blip (coming back)
    SoundAlertWorking,   // Cyan flash returning to blue (used when working LED is active)
    SoundEnterWorking,   // Cyan flash returning to blue (enter/backspace while working)
    SoundEscWorking,     // Yellow flash returning to blue (esc while working)
    SoundCmdWorking,     // Cyan flash + vibro returning to blue (cmd while working)
} SoundType;

void notify_play(NotificationApp* app, SoundType sound, bool vibro);
