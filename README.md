# Flipper Claude Code Buddy

Flipper Zero as a physical remote control and feedback device for Claude Code.

## Design Philosophy

- **Flipper is primarily an input device** — physical buttons to control Claude Code
- **Sound and vibration are the main feedback** — distinct audio cues for each event type
- **Display is secondary** — minimal 1-2 line info, large font
- **macOS only** (for now)

## Architecture

```
Flipper Zero <--USB CDC Serial--> Host Bridge (Python, macOS) <--Unix Socket--> Claude Code
```

## Features

| Button | Action |
|--------|--------|
| **UP** | Trigger macOS voice dictation |
| **LEFT** | Send ESC (interrupt Claude Code) |
| **RIGHT** | Open slash command menu |
| **OK** | Send Enter |
| **BACK** | Dismiss notification text |
| **BACK (hold)** | Exit app |

**Automatic feedback:**
- Task complete → ascending happy chime + vibration
- Error → low warning beep + vibration
- Approval needed → rising question tone + double vibration
- Connected/disconnected → startup/shutdown chords

## Quick Start

```bash
# Install everything
./scripts/install.sh

# Or manually:

# 1. Install host bridge
cd host-bridge && pip3 install -e .

# 2. Build Flipper app (requires ufbt)
cd flipper-app && ufbt build && ufbt launch

# 3. Start the bridge daemon
python3 -m bridge

# 4. Launch Claude Buddy on your Flipper
# 5. Use Claude Code normally!
```

## Project Structure

```
flipper-claude-buddy/
├── .claude/           # Claude Code integration (hooks + skill)
├── flipper-app/       # Flipper Zero application (C)
├── host-bridge/       # Python bridge daemon (macOS)
├── plugin/            # Claude Code plugin (shareable)
└── scripts/           # Install and build scripts
```

## Using as a Claude Code Plugin

The `plugin/` directory is a self-contained Claude Code plugin. Others can use it without cloning the full repo:

```bash
# Install from a local clone
claude plugin add /path/to/flipper-claude-buddy/plugin

# Or point to the plugin dir directly
claude --plugin-dir /path/to/flipper-claude-buddy/plugin
```

The plugin will:
1. Auto-create a Python venv and install bridge dependencies on first session
2. Start the bridge daemon when a Claude Code session begins
3. Forward all hooks (notifications, permissions, tool use) to the Flipper
4. Stop the bridge when the session ends

**Configuration:** When adding the plugin, Claude Code will prompt for  `serial_port` — leave empty for auto-detect, or specify your Flipper's serial port (e.g. `/dev/cu.usbmodem0001`).

## Sound Guide

| Sound | Meaning |
|-------|---------|
| C5-E5-G5 ascending | Task completed successfully |
| G3-G3 double beep | Error occurred |
| C5-E5? rising | Approval needed (check Flipper) |
| High "ding" | Voice dictation started |
| Low "dong" | Voice dictation ended |
| Quick descending | ESC sent |
| Short blip | Enter sent |
| C5-E5-G5-C6 fanfare | Bridge connected |

## `/flipper` Skill

In Claude Code, use `/flipper` to send notifications to your Flipper:

```
/flipper celebrate  → plays success sound
/flipper alert      → plays alert sound
```
