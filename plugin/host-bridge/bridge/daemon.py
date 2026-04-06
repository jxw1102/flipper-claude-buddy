"""Main daemon: routes messages between Flipper serial and Claude Code IPC."""

import asyncio
import logging
import os
from . import config, protocol
from .claude_ipc import ClaudeIPC
from .serial_conn import SerialConnection
from .transport import Transport
from .input import InputBackend, create_backend as create_input_backend
from .voice import DictationBackend, create_backend as create_dictation_backend

log = logging.getLogger(__name__)


class Daemon:
    def __init__(self, transport: Transport, dictation: DictationBackend | None = None, input_backend: InputBackend | None = None):
        self.serial = SerialConnection(transport)
        self.ipc = ClaudeIPC()
        self._dictation: DictationBackend = dictation or create_dictation_backend()
        self._input: InputBackend = input_backend or create_input_backend()
        self._dictating = False
        self._claude_connected = False
        self._perm_future: asyncio.Future | None = None
        self._menu_sent = False

        self.serial.on_message(self._handle_flipper_msg)
        self.serial.on_connect(self._send_initial_state)
        self.ipc.on_action(self._handle_ipc_action)

    async def _handle_flipper_msg(self, msg: dict):
        msg_type = msg.get("t", "")
        data = msg.get("d", {})
        log.debug("Flipper RX: type=%s data=%s", msg_type, data)

        if msg_type == "hello":
            bt_name = data.get("bt", "")
            log.info("Flipper connected: fw=%s bt=%s", data.get("fw", "?"), bt_name or "?")
            if bt_name and config.BT_NAME_CACHE:
                try:
                    with open(config.BT_NAME_CACHE, "w") as f:
                        f.write(bt_name)
                    log.info("Cached BT name: %s", bt_name)
                except Exception as e:
                    log.warning("Failed to cache BT name: %s", e)
            # Flipper app just (re)started — cancel any pending permission request
            if self._perm_future and not self._perm_future.done():
                log.info("Cancelling stale permission request")
                self._perm_future.set_result(None)
            await self.serial.send(
                protocol.notify_msg("ready", vibro=True, text="Flipper", subtext="Hello")
            )
            # Restore Claude state if already connected
            if self._claude_connected:
                await self.serial.send(protocol.state_msg(True))
            # Send command menu
            commands = self._load_commands()
            if commands:
                await self.serial.send(protocol.menu_msg(commands))
            self._menu_sent = True

        elif msg_type == "cmd":
            text = data.get("text", "")
            if text:
                await self._send_to_claude(text)

        elif msg_type == "yes":
            await self._send_to_claude("yes")

        elif msg_type == "enter":
            await self._send_keystroke("return")

        elif msg_type == "down":
            await self._send_keystroke("down")

        elif msg_type == "esc":
            await self._send_keystroke("escape")
            if self._dictating:
                self._dictating = False
                await self._dictation.stop()
                # ESC already played sound+vibro locally; just reset LED
                await self.serial.send(
                    protocol.notify_msg("voice_stop_quiet", vibro=False, text="")
                )

        elif msg_type == "voice":
            log.info("Voice UP received (dictating=%s)", self._dictating)
            try:
                if self._dictating:
                    self._dictating = False
                    await self._dictation.stop()
                    await self._send_keystroke("escape")
                    # Button already played sound+vibro locally; just reset LED
                    await self.serial.send(
                        protocol.notify_msg("voice_stop_quiet", vibro=False, text="")
                    )
                    log.info("Dictation stopped")
                else:
                    # Send Tab (amend) then start dictation
                    await self._send_keystroke("tab")
                    await self._dictation.start()
                    self._dictating = True
                    # Button already played sound+vibro locally; just start LED blink
                    await self.serial.send(
                        protocol.notify_msg("voice_start_led", vibro=False, text="")
                    )
                    log.info("Tab + dictation started")
            except Exception as e:
                log.error("Voice handler error: %s", e)
                await self.serial.send(protocol.status_msg("", ""))

        elif msg_type == "backspace":
            await self._send_keystroke("backspace")

        elif msg_type == "interrupt":
            log.info("Flipper interrupt request — sending Ctrl+C")
            await self._send_ctrl_c()

        elif msg_type == "perm_resp":
            allowed = data.get("allow", False)
            always = data.get("always", False)
            esc = data.get("esc", False)
            log.info("Flipper perm_resp: allow=%s always=%s esc=%s future=%s",
                     allowed, always, esc, self._perm_future is not None)
            if self._perm_future and not self._perm_future.done():
                self._perm_future.set_result({"allow": allowed, "always": always})
            if esc:
                await self._send_keystroke("escape")

        elif msg_type == "pong":
            if not self._menu_sent:
                log.info("First pong — sending initial state")
                self._menu_sent = True
                commands = self._load_commands()
                if commands:
                    await self.serial.send(protocol.menu_msg(commands))
                if self._claude_connected:
                    await self.serial.send(protocol.state_msg(True))

    async def _handle_ipc_action(self, request: dict) -> dict:
        action = request.get("action", "")

        if action == "notify":
            sound = request.get("sound", "alert")
            vibro = request.get("vibro", True)
            text = request.get("text", "")
            subtext = request.get("subtext", "")
            await self.serial.send(protocol.notify_msg(sound, vibro, text, subtext))
            return {"status": "ok"}

        elif action == "display":
            text = request.get("text", "")[:21]
            sub = request.get("subtext", "")[:21]
            await self.serial.send(protocol.status_msg(text, sub))
            return {"status": "ok"}

        elif action == "claude_connect":
            self._claude_connected = True
            await self.serial.send(protocol.state_msg(True))
            return {"status": "ok"}

        elif action == "claude_disconnect":
            self._claude_connected = False
            await self.serial.send(protocol.state_msg(False))
            return {"status": "ok"}

        elif action == "dismiss_permission":
            if self._perm_future and not self._perm_future.done():
                log.info("Dismissing pending permission (approved elsewhere)")
                self._perm_future.set_result({"allow": True, "always": False})
            await self.serial.send(protocol.status_msg("", ""))
            return {"status": "ok"}

        elif action == "permission_request":
            tool = request.get("tool", "Tool")[:21]
            detail = request.get("detail", "")[:21]
            log.info("Permission request: %s %s", tool, detail)

            if self._perm_future and not self._perm_future.done():
                log.info("Permission busy, rejecting")
                return {"status": "busy", "allowed": False}

            if not self.serial.connected:
                log.info("No Flipper, falling back")
                return {"status": "no_flipper", "allowed": None}

            self._perm_future = asyncio.get_running_loop().create_future()
            await self.serial.send(protocol.perm_msg(tool, detail))

            if not self.serial.connected:
                log.info("Send failed, no Flipper")
                self._perm_future = None
                return {"status": "no_flipper"}

            try:
                log.info("Waiting for Flipper response (60s timeout)")
                result = await asyncio.wait_for(self._perm_future, timeout=60.0)
                if result is None:
                    log.info("Permission cancelled (Flipper reset)")
                    return {"status": "no_flipper"}
                log.info("Permission result: %s", result)
                return {
                    "status": "ok",
                    "allowed": result["allow"],
                    "always": result["always"],
                }
            except asyncio.TimeoutError:
                log.info("Permission timed out")
                return {"status": "timeout"}
            finally:
                self._perm_future = None

        return {"status": "unknown_action"}

    # Built-in slash commands
    BUILTIN_COMMANDS = [
        "/btw", "/clear", "/compact", "/model", "/effort", "/config",
        "/usage", "/doctor", "/help", "/init", "/login", "/logout",
        "/pr-comments", "/review", "/status",
    ]

    def _load_commands(self) -> list[str]:
        """Load built-in commands + custom commands from flipper-commands.txt files."""
        commands = list(self.BUILTIN_COMMANDS)
        for path in config.CUSTOM_COMMANDS_FILES:
            if not os.path.isfile(path):
                continue
            try:
                with open(path) as f:
                    for line in f:
                        line = line.strip()
                        if line and not line.startswith("#"):
                            commands.append(line[:23])
            except Exception as e:
                log.error("Error reading %s: %s", path, e)
        log.info("Loaded %d commands (%d built-in + %d custom)",
                 len(commands), len(self.BUILTIN_COMMANDS),
                 len(commands) - len(self.BUILTIN_COMMANDS))
        return commands

    async def _send_initial_state(self):
        """Wait for Flipper app to take over CDC, then send a ping.
        The actual state (menu, claude status) is sent on first pong or hello."""
        self._menu_sent = False
        await asyncio.sleep(2.0)
        if self.serial.connected:
            await self.serial.send(protocol.ping_msg())

    async def _dictation_sync_loop(self):
        """Poll dictation backend state and correct bridge/Flipper state on mismatch."""
        while True:
            await asyncio.sleep(config.DICTATION_POLL_INTERVAL)
            if not self._dictating:
                continue
            try:
                active = await asyncio.get_running_loop().run_in_executor(
                    None, self._dictation.is_active
                )
            except Exception as e:
                log.error("Dictation poll error: %s", e)
                continue
            if not active:
                log.info("Dictation ended outside bridge — syncing state")
                self._dictating = False
                await self.serial.send(
                    protocol.notify_msg("voice_stop_quiet", vibro=False, text="Dictation stopped")
                )

    async def _send_ctrl_c(self):
        await self._input.send_ctrl_c()

    async def _send_keystroke(self, key: str):
        await self._input.send_keystroke(key)

    async def _send_to_claude(self, text: str):
        await self._input.send_text(text)

    async def run(self):
        await self._dictation.discover()
        await self.ipc.start()

        if await self.serial.connect():
            self.serial._read_task = asyncio.create_task(self.serial.read_loop())
            self.serial._ping_task = asyncio.create_task(self.serial.ping_loop())
            await self._send_initial_state()

        asyncio.create_task(self.serial.reconnect_loop())
        asyncio.create_task(self._dictation_sync_loop())

        log.info("Daemon running. Press Ctrl+C to stop.")
        try:
            await asyncio.Event().wait()
        except asyncio.CancelledError:
            pass
        finally:
            self.serial.close()
            await self.ipc.stop()


