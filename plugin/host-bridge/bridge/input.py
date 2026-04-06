"""Input backend abstraction — sends keystrokes to the focused application.

Backends
--------
AppleScriptInputBackend (default on macOS)
    Uses osascript / System Events to inject keystrokes.

Factory
-------
Call ``create_backend()`` to obtain the configured backend instance.
"""

import asyncio
import logging
from abc import ABC, abstractmethod

log = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Abstract interface
# ---------------------------------------------------------------------------

class InputBackend(ABC):
    """Interface every input backend must implement."""

    @abstractmethod
    async def send_ctrl_c(self) -> None:
        """Send Ctrl+C to the focused application."""

    @abstractmethod
    async def send_keystroke(self, key: str) -> None:
        """Send a named key ('return', 'escape', 'down', 'tab', 'backspace')."""

    @abstractmethod
    async def send_text(self, text: str) -> None:
        """Type *text* and press Return in the focused application."""


# ---------------------------------------------------------------------------
# macOS AppleScript backend
# ---------------------------------------------------------------------------

def _key_code(key: str) -> int:
    codes = {
        "return":    36,
        "escape":    53,
        "down":     125,
        "tab":       48,
        "backspace": 51,
    }
    return codes.get(key, 36)


def _escape_applescript(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"')


async def _run_applescript(script: str, context: str) -> None:
    try:
        proc = await asyncio.create_subprocess_exec(
            "osascript", "-e", script,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        await proc.communicate()
    except Exception as e:
        log.error("%s error: %s", context, e)


class AppleScriptInputBackend(InputBackend):
    """macOS input backend using osascript / System Events."""

    async def send_ctrl_c(self) -> None:
        await _run_applescript(
            'tell application "System Events"\n'
            '    key code 8 using {control down}\n'
            'end tell',
            "Ctrl+C",
        )

    async def send_keystroke(self, key: str) -> None:
        await _run_applescript(
            f'tell application "System Events"\n'
            f'    key code {_key_code(key)}\n'
            f'end tell',
            f"keystroke({key})",
        )

    async def send_text(self, text: str) -> None:
        await _run_applescript(
            f'tell application "System Events"\n'
            f'    keystroke "{_escape_applescript(text)}"\n'
            f'    key code 36\n'
            f'end tell',
            "send_text",
        )


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
