"""Configuration for the host bridge daemon."""

import os
from pathlib import Path

SOCKET_PATH = os.environ.get(
    "FLIPPER_BRIDGE_SOCKET", "/tmp/claude-flipper-bridge.sock"
)

SERIAL_BAUD = 115200

SERIAL_PORT = os.environ.get("FLIPPER_SERIAL_PORT", "")

PING_INTERVAL = 5.0

DICTATION_POLL_INTERVAL = 2.0

# ---------------------------------------------------------------------------
# Dictation backend
# ---------------------------------------------------------------------------
# "macos"  — macOS native dictation via AppleScript + pmset (default)
# "custom" — user-supplied shell commands (see DICTATION_*_CMD below)
DICTATION_BACKEND = os.environ.get("FLIPPER_DICTATION_BACKEND", "macos")

# Shell command to START dictation (required when DICTATION_BACKEND="custom")
DICTATION_START_CMD = os.environ.get("FLIPPER_DICTATION_START_CMD", "")

# Shell command to STOP dictation (optional; if empty, only ESC is sent)
DICTATION_STOP_CMD = os.environ.get("FLIPPER_DICTATION_STOP_CMD", "")

# Shell command that exits 0 while dictation is active (optional).
# If empty, the bridge assumes dictation is still running until the user
# presses UP again (manual toggle mode).
DICTATION_CHECK_CMD = os.environ.get("FLIPPER_DICTATION_CHECK_CMD", "")

# Label shown on Flipper display when dictation starts (max ~21 chars)
DICTATION_DISPLAY_NAME = os.environ.get("FLIPPER_DICTATION_NAME", "Start Dictation")

# Transport selection: "auto" (default), "usb", or "ble"
# "auto" tries USB first, then BLE — matching Flipper app behaviour.
TRANSPORT = os.environ.get("FLIPPER_TRANSPORT", "auto")

# Path to cache file for auto-detected BT name (set by plugin session-start hook)
_plugin_data = os.environ.get("FLIPPER_PLUGIN_DATA", "")
BT_NAME_CACHE = os.path.join(_plugin_data, "bt_name") if _plugin_data else ""

# Bluetooth settings (used when TRANSPORT="ble")
BT_DEVICE_NAME   = os.environ.get("FLIPPER_BT_NAME", "Flip")
BT_SCAN_TIMEOUT  = float(os.environ.get("FLIPPER_BT_SCAN_TIMEOUT", "10"))
BT_WRITE_CHUNK   = 128  # max bytes per BLE write; capped to negotiated MTU-3 at runtime

# Dual CDC mode: Flipper exposes two serial ports.
# Channel 0 = CLI (first port), Channel 1 = our app (second port).
# On macOS, the second port typically has a higher suffix number.
SERIAL_GLOB_PATTERN = "/dev/cu.usbmodem*"

# Project root for scanning .claude/commands/
PROJECT_DIR = os.environ.get(
    "FLIPPER_PROJECT_DIR",
    str(Path(__file__).parent.parent.parent),
)

# Custom commands files (project-level overrides user-level)
CUSTOM_COMMANDS_FILES = [
    str(Path.home() / ".claude" / "flipper-commands.txt"),
    os.environ.get("FLIPPER_PROJECT_DIR", str(Path(__file__).parent.parent.parent))
    + "/.claude/flipper-commands.txt",
]
