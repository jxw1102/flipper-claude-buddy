# Claude Buddy

Claude Buddy turns your Flipper Zero into a physical remote and status display for Claude Code.

It gives you haptic, audio, and LED feedback for important Claude events, and lets you control common actions from the Flipper without reaching for the keyboard.

**Note:** Claude Buddy works with Claude Code on macOS only.

## Features

- Physical controls for Enter, Escape, interrupt, menu navigation, and quick confirmation
- Distinct sounds, vibration, and LED patterns for working, success, error, permission, and disconnect states
- On-device permission prompts and status messages
- Voice dictation trigger and hold-space voice input from the Flipper
- Automatic transport selection: USB when available, Bluetooth LE otherwise

## Controls

| Button | Action |
|--------|--------|
| UP | Start / stop voice dictation |
| UP (hold) | Hold Space for voice input |
| LEFT | Interrupt Claude (Esc) |
| LEFT (hold) | Send Ctrl+C |
| RIGHT | Open slash command menu |
| OK | Submit Enter (⏎) |
| OK (hold) | Type "yes" and submit |
| DOWN | Send Down arrow (↓) |
| DOWN (hold) | Toggle mute |
| BACK | Send Backspace (⌫) |
| BACK (hold) | Exit |

## Usage

1. Install and launch Claude Buddy on your Flipper Zero.
2. Install the companion Claude Code plugin using:

    (bash)
    claude plugin marketplace add jxw1102/flipper-claude-buddy
    claude plugin install flipper-claude-buddy@flipper-claude-buddy
3. Start a Claude Code session.
4. Use the Flipper to monitor Claude activity and trigger common actions.

## Connection

Claude Buddy supports:

- USB
- Bluetooth

When a USB cable is connected, USB is preferred. Otherwise the app uses BLE.
