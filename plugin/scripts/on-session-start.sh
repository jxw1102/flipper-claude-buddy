#!/bin/bash
set -euo pipefail

# SessionStart hook: ensure bridge is running, then notify Flipper.
# Adapted for Claude Code plugin: uses CLAUDE_PLUGIN_ROOT and CLAUDE_PLUGIN_DATA.

SOCKET="/tmp/claude-flipper-bridge.sock"
PIDFILE="/tmp/claude-flipper-bridge.pid"
LOG="/tmp/claude-flipper-bridge.log"

# Read hook payload from stdin and extract the model for display.
PAYLOAD=$(cat)
MODEL=$(echo "$PAYLOAD" | python3 -c '
import json, sys
try:
    data = json.load(sys.stdin)
    print((data.get("model") or "")[:21])
except Exception:
    print("")
' 2>/dev/null)

PLUGIN_ROOT="${CLAUDE_PLUGIN_ROOT:-.}"
PLUGIN_DATA="${CLAUDE_PLUGIN_DATA:-/tmp/flipper-claude-buddy}"
BRIDGE_DIR="$PLUGIN_ROOT/host-bridge"
VENV_DIR="$PLUGIN_DATA/venv"
MARKER="$VENV_DIR/.installed-hash"
CURRENT_HASH=$(cat "$BRIDGE_DIR/pyproject.toml" "$BRIDGE_DIR"/bridge/*.py 2>/dev/null | md5 -q 2>/dev/null || cat "$BRIDGE_DIR/pyproject.toml" "$BRIDGE_DIR"/bridge/*.py 2>/dev/null | md5sum | cut -d' ' -f1 || echo "none")

# Forward plugin userConfig serial_port to bridge env var
if [ -n "${CLAUDE_PLUGIN_OPTION_serial_port:-}" ]; then
    export FLIPPER_SERIAL_PORT="$CLAUDE_PLUGIN_OPTION_serial_port"
fi

# Forward plugin userConfig transport to bridge env var
if [ -n "${CLAUDE_PLUGIN_OPTION_transport:-}" ]; then
    export FLIPPER_TRANSPORT="$CLAUDE_PLUGIN_OPTION_transport"
fi

# Forward plugin userConfig bluetoothName to bridge env var.
# If not set, fall back to previously auto-detected name from cache.
BT_NAME_CACHE="$PLUGIN_DATA/bt_name"
if [ -n "${CLAUDE_PLUGIN_OPTION_bluetoothName:-}" ]; then
    export FLIPPER_BT_NAME="$CLAUDE_PLUGIN_OPTION_bluetoothName"
elif [ -f "$BT_NAME_CACHE" ]; then
    export FLIPPER_BT_NAME="$(cat "$BT_NAME_CACHE")"
fi

# Pass PLUGIN_DATA to bridge so it can write the BT name cache after hello
export FLIPPER_PLUGIN_DATA="$PLUGIN_DATA"

# Skip if no Flipper is connected (unless bridge is already running)
if [ ! -S "$SOCKET" ]; then
    TRANSPORT="${FLIPPER_TRANSPORT:-auto}"
    if [ -n "${FLIPPER_SERIAL_PORT:-}" ]; then
        # Explicit USB port configured — check it exists
        if [ ! -e "$FLIPPER_SERIAL_PORT" ]; then
            exit 0
        fi
    elif [ "$TRANSPORT" = "usb" ]; then
        # USB-only mode — require a USB device
        if ! ls /dev/cu.usbmodem* 1>/dev/null 2>&1; then
            exit 0
        fi
    fi
    # "ble" or "auto" transport: always attempt — BLE cannot be pre-checked from shell
fi

# Clean up stale socket/pid if the bridge process is dead
if [ -S "$SOCKET" ] && [ -f "$PIDFILE" ]; then
    OLD_PID=$(cat "$PIDFILE" 2>/dev/null || echo "")
    INSTALLED_HASH=$(cat "$MARKER" 2>/dev/null || echo "")
    if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null && [ "$INSTALLED_HASH" != "$CURRENT_HASH" ]; then
        echo "[bridge] Bridge code changed; restarting daemon $OLD_PID..." >&2
        kill "$OLD_PID" 2>/dev/null || true
        rm -f "$SOCKET" "$PIDFILE" "/tmp/claude-flipper-bridge.refcount"
        OLD_PID=""
    fi
    if [ -n "$OLD_PID" ] && ! kill -0 "$OLD_PID" 2>/dev/null; then
        echo "[bridge] Cleaning up stale bridge (pid $OLD_PID gone)..." >&2
        rm -f "$SOCKET" "$PIDFILE" "/tmp/claude-flipper-bridge.refcount"
    fi
elif [ -S "$SOCKET" ] && [ ! -f "$PIDFILE" ]; then
    # Socket exists but no pidfile — check if anything is listening
    if ! echo '{}' | nc -U "$SOCKET" -w 1 >/dev/null 2>&1; then
        echo "[bridge] Cleaning up orphaned socket..." >&2
        rm -f "$SOCKET"
    fi
fi

# Start bridge if not already running
if [ ! -S "$SOCKET" ]; then
    echo "[bridge] Starting flipper bridge..." >&2

    # Create venv if it doesn't exist or any source file changed
    if [ ! -d "$VENV_DIR" ] || [ ! -f "$MARKER" ] || [ "$(cat "$MARKER" 2>/dev/null)" != "$CURRENT_HASH" ]; then
        echo "[bridge] Setting up Python environment..." >&2
        mkdir -p "$PLUGIN_DATA"
        python3 -m venv "$VENV_DIR"
        "$VENV_DIR/bin/pip" install -q --force-reinstall "$BRIDGE_DIR" 2>&1 | tail -1 >&2
        echo "$CURRENT_HASH" > "$MARKER"
    fi

    nohup "$VENV_DIR/bin/python" -m bridge >> "$LOG" 2>&1 &
    echo $! > "$PIDFILE"

    for i in $(seq 1 30); do
        [ -S "$SOCKET" ] && break
        sleep 0.1
    done
fi

if [ ! -S "$SOCKET" ]; then
    echo "[bridge] Socket not available, bridge may have failed. Check $LOG" >&2
    exit 0
fi

# Register the current runner session before Claude connects so input
# targeting is ready as soon as Flipper events arrive.
python3 "$PLUGIN_ROOT/scripts/session-target.py" register_target "$SOCKET" >/dev/null 2>&1 || true

# Increment session reference counter
REFCOUNT_FILE="/tmp/claude-flipper-bridge.refcount"
COUNT=$(cat "$REFCOUNT_FILE" 2>/dev/null || echo 0)
echo $((COUNT + 1)) > "$REFCOUNT_FILE"

echo '{"action":"claude_connect"}' \
    | nc -U "$SOCKET" 2>/dev/null || true

echo "{\"action\":\"notify\",\"sound\":\"connect\",\"vibro\":true,\"text\":\"Session Start\",\"subtext\":\"$MODEL\"}" \
    | nc -U "$SOCKET" 2>/dev/null || true

exit 0
