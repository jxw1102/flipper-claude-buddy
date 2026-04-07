"""Input backend abstraction — sends keystrokes to a registered runner target.

Backends
--------
AppleScriptInputBackend (default on macOS)
    Uses osascript / System Events to inject keystrokes.

Factory
-------
Call ``create_backend()`` to obtain the configured backend instance.
"""

import asyncio
import ctypes
import logging
import sys
from abc import ABC, abstractmethod
from ctypes.util import find_library
from dataclasses import dataclass

log = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Abstract interface
# ---------------------------------------------------------------------------

class InputBackend(ABC):
    """Interface every input backend must implement."""

    def set_target(self, target: dict[str, str] | None) -> None:
        """Optionally bind future input to a specific runner session."""

    @abstractmethod
    async def send_ctrl_c(self) -> None:
        """Send Ctrl+C to the current runner target."""

    @abstractmethod
    async def send_keystroke(self, key: str) -> None:
        """Send a named key ('return', 'escape', 'down', 'tab', 'backspace')."""

    @abstractmethod
    async def send_text(self, text: str) -> None:
        """Type *text* and press Return in the focused application."""

    @abstractmethod
    async def send_key_down(self, key: str) -> None:
        """Press and hold a named key without releasing it."""

    @abstractmethod
    async def send_key_up(self, key: str) -> None:
        """Release a previously held named key."""


# ---------------------------------------------------------------------------
# macOS AppleScript backend
# ---------------------------------------------------------------------------

def _key_code(key: str) -> int:
    codes = {
        "return":    36,
        "escape":    53,
        "down":     125,
        "space":     49,
        "tab":       48,
        "backspace": 51,
    }
    return codes.get(key, 36)


def _escape_applescript(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"')


def _clean_target_value(value: object) -> str:
    if value is None:
        return ""
    return str(value).strip()


@dataclass(slots=True)
class InputTarget:
    session_key: str = ""
    app_name: str = ""
    term_program: str = ""
    term_session_id: str = ""
    iterm_session_id: str = ""
    tty: str = ""

    @classmethod
    def from_payload(cls, payload: dict[str, str] | None) -> "InputTarget | None":
        if not payload:
            return None
        target = cls(
            session_key=_clean_target_value(payload.get("session_key")),
            app_name=_clean_target_value(payload.get("app_name")),
            term_program=_clean_target_value(payload.get("term_program")),
            term_session_id=_clean_target_value(payload.get("term_session_id")),
            iterm_session_id=_clean_target_value(payload.get("iterm_session_id")),
            tty=_clean_target_value(payload.get("tty")),
        )
        if not any(
            (
                target.app_name,
                target.term_program,
                target.term_session_id,
                target.iterm_session_id,
                target.tty,
            )
        ):
            return None
        return target

    def describe(self) -> str:
        parts = []
        if self.app_name:
            parts.append(f"app={self.app_name}")
        if self.tty:
            parts.append(f"tty={self.tty}")
        if self.term_session_id:
            parts.append(f"term_session_id={self.term_session_id}")
        if self.iterm_session_id:
            parts.append(f"iterm_session_id={self.iterm_session_id}")
        if not parts:
            return "default"
        return ", ".join(parts)


def _generic_focus_script(app_name: str) -> str:
    escaped_app = _escape_applescript(app_name)
    return (
        "set __flipperTargetFocused to false\n"
        "try\n"
        f'    tell application "{escaped_app}" to activate\n'
        "    set __flipperTargetFocused to true\n"
        "end try\n"
        "if __flipperTargetFocused then delay 0.05\n"
    )


def _terminal_focus_script(target: InputTarget) -> str:
    if not target.tty:
        return _generic_focus_script("Terminal")

    escaped_tty = _escape_applescript(target.tty)
    return (
        "set __flipperTargetFocused to false\n"
        "try\n"
        '    tell application "Terminal"\n'
        "        repeat with w in windows\n"
        "            repeat with t in tabs of w\n"
        f'                if tty of t is "{escaped_tty}" then\n'
        "                    set index of w to 1\n"
        "                    set selected of t to true\n"
        "                    activate\n"
        "                    set __flipperTargetFocused to true\n"
        "                    exit repeat\n"
        "                end if\n"
        "            end repeat\n"
        "            if __flipperTargetFocused then exit repeat\n"
        "        end repeat\n"
        "    end tell\n"
        "end try\n"
        "if not __flipperTargetFocused then\n"
        + "\n".join(f"    {line}" for line in _generic_focus_script("Terminal").splitlines())
        + "\nend if\n"
    )


def _focus_script(target: InputTarget | None) -> str:
    if not target:
        return ""
    if target.app_name == "Terminal":
        return _terminal_focus_script(target)
    if target.app_name:
        return _generic_focus_script(target.app_name)
    return ""


def _build_key_code_script(
    key_code: int,
    modifiers: str = "",
    target: InputTarget | None = None,
) -> str:
    lines = []
    focus = _focus_script(target)
    if focus:
        lines.append(focus.rstrip())
    lines.extend(
        [
            'tell application "System Events"',
            (
                f"    key code {key_code} using {{{modifiers}}}"
                if modifiers
                else f"    key code {key_code}"
            ),
            "end tell",
        ]
    )
    return "\n".join(lines)


def _build_send_text_script(text: str, target: InputTarget | None = None) -> str:
    lines = []
    focus = _focus_script(target)
    if focus:
        lines.append(focus.rstrip())
    lines.extend(
        [
            'tell application "System Events"',
            f'    keystroke "{_escape_applescript(text)}"',
            "    key code 36",
            "end tell",
        ]
    )
    return "\n".join(lines)


_application_services = None


def _load_application_services():
    global _application_services
    if _application_services is not None:
        return _application_services
    if sys.platform != "darwin":
        return None

    lib_path = find_library("ApplicationServices")
    if not lib_path:
        log.warning("ApplicationServices framework not found")
        return None

    lib = ctypes.CDLL(lib_path)
    lib.CGEventCreateKeyboardEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_bool]
    lib.CGEventCreateKeyboardEvent.restype = ctypes.c_void_p
    lib.CGEventPost.argtypes = [ctypes.c_uint32, ctypes.c_void_p]
    lib.CGEventPost.restype = None
    lib.CFRelease.argtypes = [ctypes.c_void_p]
    lib.CFRelease.restype = None
    _application_services = lib
    return lib


def _post_key_event(key: str, is_key_down: bool) -> bool:
    lib = _load_application_services()
    if lib is None:
        return False

    event = lib.CGEventCreateKeyboardEvent(None, _key_code(key), is_key_down)
    if not event:
        return False
    try:
        lib.CGEventPost(0, event)
        return True
    finally:
        lib.CFRelease(event)


async def _run_applescript(script: str, context: str) -> None:
    try:
        proc = await asyncio.create_subprocess_exec(
            "osascript", "-e", script,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await proc.communicate()
        if proc.returncode != 0:
            message = stderr.decode().strip() or stdout.decode().strip() or f"rc={proc.returncode}"
            log.warning("%s failed: %s", context, message)
    except Exception as e:
        log.error("%s error: %s", context, e)


class AppleScriptInputBackend(InputBackend):
    """macOS input backend using osascript / System Events."""

    def __init__(self) -> None:
        self._target: InputTarget | None = None
        self._held_keys: set[str] = set()

    def set_target(self, target: dict[str, str] | None) -> None:
        self._target = InputTarget.from_payload(target)
        log.info(
            "Input target set: %s",
            self._target.describe() if self._target else "frontmost application",
        )

    async def send_ctrl_c(self) -> None:
        await _run_applescript(
            _build_key_code_script(8, modifiers="control down", target=self._target),
            "Ctrl+C",
        )

    async def send_keystroke(self, key: str) -> None:
        await _run_applescript(
            _build_key_code_script(_key_code(key), target=self._target),
            f"keystroke({key})",
        )

    async def send_text(self, text: str) -> None:
        await _run_applescript(
            _build_send_text_script(text, target=self._target),
            "send_text",
        )

    async def send_key_down(self, key: str) -> None:
        if key in self._held_keys:
            return
        focus = _focus_script(self._target)
        if focus:
            await _run_applescript(focus, f"focus({key})")
        if not _post_key_event(key, True):
            log.warning("key_down(%s) failed", key)
            return
        self._held_keys.add(key)

    async def send_key_up(self, key: str) -> None:
        if key not in self._held_keys:
            return
        if not _post_key_event(key, False):
            log.warning("key_up(%s) failed", key)
            return
        self._held_keys.discard(key)


# ---------------------------------------------------------------------------
# Factory
# ---------------------------------------------------------------------------

def create_backend() -> InputBackend:
    import sys
    if sys.platform == "darwin":
        return AppleScriptInputBackend()
    raise NotImplementedError(
        f"No input backend available for platform {sys.platform!r}. "
        "Implement InputBackend and register it in input.create_backend()."
    )
