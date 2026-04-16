# Flipper Claude Code Buddy

Turn your Flipper Zero into a physical companion for Claude Code. Get tactile feedback for every AI action — feel the difference between a completed task, an error, and an approval request — and control Claude with real buttons instead of typing.

> Supports macOS and Linux. Windows is not tested.

## What it does

**You feel what Claude is doing.**
Every significant event triggers a distinct sound and vibration pattern on your Flipper, so you always know Claude's status even when you're not looking at the screen.

**You control Claude with physical buttons.**
Interrupt a runaway task, submit a prompt, trigger voice dictation, or open the slash command menu — all without touching the keyboard.

## Buttons

| Button | Action |
|--------|--------|
| UP | Start / stop voice dictation |
| UP (hold) | Hold Space for voice input |
| LEFT | Interrupt Claude (Esc) |
| LEFT (hold) | Send Ctrl+C |
| RIGHT | Open slash command menu |
| RIGHT (hold) | Open menu |
| OK | Submit Enter (⏎) |
| OK (hold) | Type "yes" and submit |
| DOWN | Send Down arrow (↓) |
| DOWN (hold) | Toggle mute |
| BACK | Send Backspace (⌫) |
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

**macOS — First-time Bluetooth pairing:** on first BLE connection macOS will pair with the Flipper. Accept the pairing prompt on both sides. If the connection fails after a firmware flash or factory reset, remove the Flipper from System Settings → Bluetooth and let it re-pair.

**macOS — Bluetooth permission:** Terminal (or your terminal app) must have Bluetooth access. Grant it in System Settings → Privacy & Security → Bluetooth.

**Linux — USB:** Flipper appears as `/dev/ttyACM*`. No additional drivers needed. If you get a permission error, add your user to the `dialout` group:
```bash
sudo usermod -aG dialout $USER  # log out and back in to apply
```

**Linux — Keystroke forwarding:** Flipper button presses are forwarded to your terminal via `xdotool` (X11 only). Install it if needed:
```bash
sudo apt install xdotool
```
Wayland is not yet supported for keystroke forwarding.

**Linux — BLE:** BLE transport works via BlueZ. Make sure BlueZ is installed and running:
```bash
sudo apt install bluetooth bluez
sudo systemctl enable --now bluetooth
sudo usermod -aG bluetooth $USER  # log out and back in to apply
```

## Troubleshooting

| Problem | Fix |
|---|---|
| Flipper not found over USB (macOS) | Make sure no other app (qFlipper, Chrome serial, etc.) is using the port. If it still fails, set `FLIPPER_SERIAL_PORT=/dev/cu.usbmodemXXX` explicitly. |
| Flipper not found over USB (Linux) | Check `ls /dev/ttyACM*` — if empty, try a different USB cable. If the port exists but access is denied, run `sudo usermod -aG dialout $USER` and log out/in. Set `FLIPPER_SERIAL_PORT=/dev/ttyACMX` explicitly if needed. |
| Flipper not found over BLE | Make sure Bluetooth is on and the app is running on the Flipper |
| No sound on task complete | Check that the bridge is running: `cat /tmp/claude-flipper-bridge.log` |

## Support

If you find this useful, consider [buying me a coffee](https://ko-fi.com/jxw1102).

## License

MIT — see [LICENSE](LICENSE).
