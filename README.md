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
| LEFT (hold) | Send Ctrl+C |
| RIGHT | Open slash command menu |
| OK | Submit (Enter) |
| OK (hold) | Type "yes" and submit |
| DOWN (hold) | Toggle mute |
| BACK | Send backspace |
| BACK (hold) | Exit |

## Setup

### 1. Install the Flipper app

Download `claude_buddy.fap` from the [latest release](../../releases/latest) and copy it to your Flipper Zero:

- **Via qFlipper:** SD Card → `apps/USB/` → drag and drop
- **Via SD card:** copy to `apps/USB/` directly

### 2. Install the Claude Code plugin

```bash
claude plugin marketplace add jxw1102/flipper-claude-buddy
claude plugin install flipper-claude-buddy@flipper-claude-buddy
```

Claude Code will ask for your connection preference (`auto`, `usb`, or `ble`). Leave everything else empty for auto-detect.

The plugin starts automatically with every Claude Code session and stops when you close it.

### 3. Launch Claude Buddy on your Flipper

Go to **Applications → USB → Claude Buddy**. You'll hear the startup fanfare when the connection is established.

## Connection

Connects over USB (plug-and-play) or Bluetooth LE — whichever is available. USB is preferred when the cable is plugged in; it falls back to BLE automatically.

**First-time Bluetooth pairing:** on first BLE connection macOS will pair with the Flipper. Accept the pairing prompt on both sides. If the connection fails after a firmware flash or factory reset, remove the Flipper from macOS System Settings → Bluetooth and let it re-pair.

**Bluetooth permission:** macOS requires Terminal (or your terminal app) to have Bluetooth access. Grant it in System Settings → Privacy & Security → Bluetooth.

## Troubleshooting

| Problem | Fix |
|---|---|
| Flipper not found over USB | Set `FLIPPER_SERIAL_PORT=/dev/cu.usbmodemXXX` |
| Flipper not found over BLE | Make sure Bluetooth is on and the app is running on the Flipper |
| No sound on task complete | Check that the bridge is running: `cat /tmp/claude-flipper-bridge.log` |

## License

MIT — see [LICENSE](LICENSE).
