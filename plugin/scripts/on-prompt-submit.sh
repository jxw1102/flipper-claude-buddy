#!/bin/bash
set -euo pipefail

# UserPromptSubmit hook: show "Working..." on Flipper when user sends a prompt

SOCKET="/tmp/claude-flipper-bridge.sock"

if [ ! -S "$SOCKET" ]; then
    exit 0
fi

echo '{"action":"display","text":"Working...","subtext":""}' \
    | nc -U "$SOCKET" 2>/dev/null &

exit 0
