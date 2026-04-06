#!/bin/bash
# Install Flipper Claude Code Buddy
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=== Flipper Claude Code Buddy - Install ==="
echo ""

# Install Python bridge
echo "[1/3] Installing host bridge..."
cd "$PROJECT_DIR/host-bridge"
pip3 install -e . --quiet
echo "  Done."

# Make hooks executable
echo "[2/3] Setting up Claude Code hooks..."
chmod +x "$PROJECT_DIR/.claude/hooks/"*.sh
echo "  Done."

# Build Flipper app (if ufbt available)
echo "[3/3] Building Flipper app..."
if command -v ufbt &>/dev/null; then
    cd "$PROJECT_DIR/flipper-app"
    ufbt build
    echo "  Done. Flash with: ufbt launch"
else
    echo "  ufbt not found. Install it with: pip3 install ufbt"
    echo "  Then run: cd flipper-app && ufbt build && ufbt launch"
fi

echo ""
echo "=== Installation complete ==="
echo ""
echo "Usage:"
echo "  1. Start the bridge:  python3 -m bridge"
echo "  2. Connect Flipper Zero via USB"
echo "  3. Launch Claude Buddy app on Flipper"
echo "  4. Use Claude Code as normal - Flipper will respond!"
echo ""
echo "Buttons:"
echo "  UP    = Voice dictation"
echo "  LEFT  = Slash command menu"
echo "  RIGHT = Send ESC"
echo "  OK    = Send Enter"
echo "  BACK (long) = Exit app"
