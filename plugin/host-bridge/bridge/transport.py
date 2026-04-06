"""Abstract transport interface for the host bridge."""

from abc import ABC, abstractmethod


class Transport(ABC):
    """Raw byte transport: connect, read lines, write bytes, close."""

    @abstractmethod
    async def connect(self) -> bool:
        """Open the connection. Returns True on success."""

    @abstractmethod
    async def readline(self) -> bytes:
        """Read until newline and return the complete line (including \\n)."""

    @abstractmethod
    async def write(self, data: bytes) -> None:
        """Enqueue data for sending."""

    @abstractmethod
    async def drain(self) -> None:
        """Flush any buffered outgoing data."""

    @abstractmethod
    def close(self) -> None:
        """Close the connection (best-effort, non-blocking)."""

    @property
    @abstractmethod
    def is_closing(self) -> bool:
        """True when the underlying connection has been lost or is closing."""

