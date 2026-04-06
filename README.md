# Flipper Claude Code Buddy

Turn your Flipper Zero into a physical companion for Claude Code. Get tactile feedback for every AI action — feel the difference between a completed task, an error, and an approval request — and control Claude with real buttons instead of typing.

> macOS only for now.

## What it does

**You feel what Claude is doing.**
Every significant event triggers a distinct sound and vibration pattern on your Flipper, so you always know Claude's status even when you're not looking at the screen.

**You control Claude with physical buttons.**
Interrupt a runaway task, submit a prompt, trigger voice dictation, or open the slash command menu — all without touching the keyboard.

## Buttons

| Button | Action |
|--------|--------|
| UP | Start / stop voice dictation |
| LEFT | Interrupt Claude (ESC) |
| RIGHT | Open slash command menu |
| OK | Submit (Enter) |
| BACK | Dismiss notification |
| BACK (hold) | Exit |

## Feedback sounds

| What you hear | What it means |
|---|---|
| Ascending chime (C-E-G) | Task completed |
| Double low beep | Error |
| Rising question tone | Claude needs your approval |
| High ding / low dong | Voice dictation on / off |
| Startup fanfare | Connected and ready |

Vibration accompanies every sound for eyes-free awareness.

## Setup

### 1. Install the Flipper app

Download `claude_buddy.fap` from the [latest release](../../releases/latest) and copy it to your Flipper Zero:

- **Via qFlipper:** SD Card → `apps/USB/` → drag and drop
- **Via SD card:** copy to `apps/USB/` directly

### 2. Install the Claude Code plugin

```bash
claude plugin add /path/to/flipper-claude-buddy/plugin
```

Claude Code will ask for your connection preference (`auto`, `usb`, or `ble`). Leave everything else empty for auto-detect.

The plugin starts automatically with every Claude Code session and stops when you close it.

### 3. Launch Claude Buddy on your Flipper

Go to **Applications → USB → Claude Buddy**. You'll hear the startup fanfare when the connection is established.

## Connection

Connects over USB (plug-and-play) or Bluetooth LE — whichever is available. USB is preferred when the cable is plugged in; it falls back to BLE automatically.

## `/flipper` command

Send a notification to your Flipper from inside a Claude Code session:

```
/flipper celebrate   → success chime
/flipper alert       → attention tone
```

## Troubleshooting

| Problem | Fix |
|---|---|
| Flipper not found over USB | Set `FLIPPER_SERIAL_PORT=/dev/cu.usbmodemXXX` |
| Flipper not found over BLE | Make sure Bluetooth is on and the app is running on the Flipper |
| No sound on task complete | Check that the bridge is running (`claude plugin status`) |

## License

MIT — see [LICENSE](LICENSE).
