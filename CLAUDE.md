# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run Commands

### Flipper App (C)
```bash
# Build (requires ufbt: pip3 install ufbt)
cd flipper-app && ufbt build

# Build and flash to connected Flipper
cd flipper-app && ufbt launch

# Or use the script
./scripts/build-flipper.sh           # build only
./scripts/build-flipper.sh --flash   # build + flash
```

### Host Bridge (Python)
```bash
# Install (editable)
cd plugin/host-bridge && pip3 install -e .

# Run bridge daemon
python3 -m bridge

# Run with explicit transport
python3 -m bridge --transport ble   # BLE only
python3 -m bridge --transport usb   # USB only

# Key environment overrides
FLIPPER_BT_NAME="Flip"            # BLE device name prefix
FLIPPER_TRANSPORT=ble              # force BLE
FLIPPER_LOG_LEVEL=debug            # verbose logs
```

### Testing IPC
```bash
echo '{"action":"notify","sound":"success","vibro":true,"text":"Test","subtext":""}' \
  | nc -U /tmp/claude-flipper-bridge.sock
```

## Architecture

```
Flipper Zero (C app)
  ↕ USB CDC serial  OR  BLE serial
Host Bridge (Python daemon, macOS)
  ↕ Unix socket  /tmp/claude-flipper-bridge.sock
Claude Code hook scripts (plugin/ or .claude/)
```

The system has three components:

1. **`flipper-app/`** — Flipper Zero FAP (C). Handles button input and audio/haptic feedback. Auto-selects USB or BLE transport at startup based on USB power state.

2. **`plugin/host-bridge/`** — Python asyncio daemon. Bridges serial ↔ Unix socket. Manages BLE/USB connection with auto-reconnect. Serves a Unix socket that hook scripts connect to.

3. **`plugin/`** — Claude Code plugin (self-contained, shareable). Hook scripts that translate Claude Code lifecycle events (notifications, permissions, tool use) into socket messages.

## Threading Model — Critical

### Flipper App
- **BLE/serial RX callback** runs on a worker thread. It must NOT call any UI functions or `transport_send`.
- The callback queues parsed `ProtocolMessage` into a `FuriMessageQueue` and signals the GUI thread via `view_dispatcher_send_custom_event`.
- **GUI thread** (the Furi event loop) drains the queue and calls `transport_send` safely.
- Calling `ble_profile_serial_tx` from inside `bt_serial_event_cb` deadlocks on Momentum firmware — always defer TX to the GUI thread.

### Host Bridge
- Single asyncio event loop. All transport I/O, IPC, and ping tasks are async coroutines.
- `serial_conn.py` runs a reconnect loop that re-establishes the transport on disconnect.
- `transport_bt.py`: `readline()` must handle disconnect without blocking — checks `_closed` flag before and after `_rx_event.clear()`.

## Protocol

JSON lines (`\n`-terminated) over serial (USB or BLE):
```json
{"v": 1, "t": "<type>", "d": {...}}
```

**Host → Flipper:** `ping`, `notify`, `state`, `status`, `menu`, `perm`
**Flipper → Host:** `hello`, `pong`, `enter`, `esc`, `voice`, `down`, `cmd`, `perm_resp`

The Flipper sends `hello` on the first received `ping` (from the GUI thread), not at BLE connect time. This is because the host's CCCD write (enabling notifications) hasn't happened yet when the connection status callback fires.

## BLE Transport Details

- Service UUID: `8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000`
- RX (Flipper→host, notify): `19ed82ae-ed21-4c9d-4145-228e61fe0000`
- TX (host→Flipper, write): `19ed82ae-ed21-4c9d-4145-228e62fe0000`
- Host writes with `response=False` (write-without-response), chunk size capped to `negotiated_mtu - 3`
- `BT_WRITE_CHUNK = 128` in `config.py` (runtime cap applies)

## Key Files

| File | Purpose |
|------|---------|
| `flipper-app/claude_buddy.c` | App entry point, GUI event loop, message dispatch |
| `flipper-app/transport_bt.c` | BLE transport — RX callback, connection state |
| `flipper-app/ui.c` | Display rendering, button input handlers |
| `flipper-app/protocol.c` | JSON parse/build for all message types |
| `plugin/host-bridge/bridge/daemon.py` | Main event loop, message routing |
| `plugin/host-bridge/bridge/transport_bt.py` | BLE transport (bleak), readline, write |
| `plugin/host-bridge/bridge/serial_conn.py` | Reconnect loop, disconnect detection |
| `plugin/host-bridge/bridge/config.py` | All tunables (timeouts, UUIDs, chunk sizes) |
| `plugin/scripts/` | Hook scripts for each Claude Code lifecycle event |

## Runtime Files (macOS)
- Socket: `/tmp/claude-flipper-bridge.sock`
- PID: `/tmp/claude-flipper-bridge.pid`
- Log: `/tmp/claude-flipper-bridge.log`
- Session refcount: `/tmp/claude-flipper-bridge.refcount`
