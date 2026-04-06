#!/bin/bash
set -euo pipefail

# PreToolUse hook: dismiss any pending permission screen on Flipper
# (handles case where permission was granted on Mac, not Flipper)

SOCKET="/tmp/claude-flipper-bridge.sock"

if [ ! -S "$SOCKET" ]; then
    exit 0
fi

echo '{"action":"dismiss_permission"}' \
    | nc -U "$SOCKET" 2>/dev/null &

exit 0
