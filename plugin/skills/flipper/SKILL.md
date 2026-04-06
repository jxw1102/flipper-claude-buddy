---
name: flipper
description: Send notifications, sounds, or messages to the connected Flipper Zero device.
allowed-tools: Bash
---

# Flipper Claude Code Buddy

Send data to the connected Flipper Zero via the host bridge daemon.

The bridge listens on a Unix socket at `/tmp/claude-flipper-bridge.sock`.

## Commands

### Notify (sound + vibration + two-line text)
```bash
echo '{"action":"notify","sound":"success","vibro":true,"text":"Title","subtext":"Subtitle"}' | nc -U /tmp/claude-flipper-bridge.sock
```

### Display status text (two lines)
```bash
echo '{"action":"display","text":"Line 1","subtext":"Line 2"}' | nc -U /tmp/claude-flipper-bridge.sock
```

## Sound options
- `success` - Happy ascending chime (task complete)
- `error` - Low warning beeps (something failed)
- `alert` - Rising question tone (needs attention)
- `cmd` - Confirm tone (command executed)
- `enter` - Soft blip (file written)
- `esc` - Dismissal tone

## Rules
- Keep text under 20 characters per line
- Use `success` for completions, `error` for failures, `alert` for attention needed
- Always set `vibro: true` for important notifications
