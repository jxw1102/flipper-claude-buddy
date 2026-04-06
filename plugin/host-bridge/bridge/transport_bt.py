"""
Bluetooth BLE transport — uses bleak to talk to the Flipper Zero BLE serial service.

Connection flow:
  1. Scan for a BLE device whose name starts with config.BT_DEVICE_NAME ("Flipper")
  2. Connect and look for the Flipper's Serial-over-BLE GATT service
  3. Use the known serial characteristic for both notify (RX) and write (TX)
  4. Subscribe to notifications on the serial characteristic
  5. Incoming notifications are buffered; readline() returns complete lines

Flipper BLE Serial UUIDs (from Momentum firmware):
  Service:  8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000
  TX char:  19ed82ae-ed21-4c9d-4145-228e61fe0000  (Flipper→host, notify)
  RX char:  19ed82ae-ed21-4c9d-4145-228e62fe0000  (host→Flipper, write)

Install dependency:  pip install bleak   (or: pip install ".[bt]")
"""

import asyncio
import logging

from . import config
from .transport import Transport

log = logging.getLogger(__name__)


FLIPPER_SERIAL_SERVICE_UUID = "8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000"
FLIPPER_SERIAL_TX_UUID = "19ed82ae-ed21-4c9d-4145-228e61fe0000"  # Flipper→host (notify)
FLIPPER_SERIAL_RX_UUID = "19ed82ae-ed21-4c9d-4145-228e62fe0000"  # host→Flipper (write)


class BtTransport(Transport):
    def __init__(self):
        self._client = None
        self._tx_uuid = FLIPPER_SERIAL_TX_UUID  # subscribe for notify
        self._rx_uuid = FLIPPER_SERIAL_RX_UUID  # write to
        self._rx_buf = bytearray()
        self._rx_event = asyncio.Event()
        self._closed = True

    # ── Transport interface ────────────────────────────────────────

    async def connect(self) -> bool:
        try:
            from bleak import BleakScanner, BleakClient
        except ImportError:
            log.error("BT transport requires bleak:  pip install bleak")
            return False

        log.info("BT: scanning for %r (timeout %.0fs)…",
                 config.BT_DEVICE_NAME, config.BT_SCAN_TIMEOUT)
        device = await BleakScanner.find_device_by_filter(
            lambda d, _: bool(d.name and d.name.startswith(config.BT_DEVICE_NAME)),
            timeout=config.BT_SCAN_TIMEOUT,
        )
        if device is None:
            log.warning("BT: Flipper not found — is Bluetooth enabled and advertising?")
            return False

        self._client = BleakClient(device, disconnected_callback=self._on_disconnect)
        try:
            await self._client.connect()
        except Exception as e:
            log.error("BT: connect to %s failed: %s", device.name, e)
            self._client = None
            return False

        # Verify the serial characteristics exist
        try:
            tx_char = self._client.services.get_characteristic(self._tx_uuid)
            rx_char = self._client.services.get_characteristic(self._rx_uuid)
        except Exception:
            tx_char = rx_char = None
        if tx_char is None or rx_char is None:
            log.error(
                "BT: serial characteristics not found on %s — "
                "is the Flipper running a compatible app?",
                device.name,
            )
            await self._client.disconnect()
            self._client = None
            return False

        log.info("BT: serial service found  TX=%s  RX=%s", self._tx_uuid, self._rx_uuid)
        log.info("BT: RX char properties: %s", rx_char.properties)
        mtu = getattr(self._client, "mtu_size", 23)
        log.info("BT: negotiated MTU=%d  (write chunk=%d)", mtu, max(1, min(mtu - 3, config.BT_WRITE_CHUNK)))

        await self._client.start_notify(self._tx_uuid, self._on_notify)
        self._closed = False
        self._rx_buf.clear()
        log.info("BT: connected to %s", device.name)
        return True

    async def readline(self) -> bytes:
        """Block until a complete \\n-terminated line arrives via BLE notify."""
        while True:
            if b"\n" in self._rx_buf:
                idx = self._rx_buf.index(b"\n")
                line = bytes(self._rx_buf[: idx + 1])
                del self._rx_buf[: idx + 1]
                return line
            if self._closed:
                return b""
            self._rx_event.clear()
            # Re-check after clear to avoid losing a notification that arrived
            # between the buffer check above and clearing the event.
            if b"\n" in self._rx_buf or self._closed:
                continue
            await self._rx_event.wait()

    async def write(self, data: bytes) -> None:
        """Write to the RX characteristic in MTU-safe chunks.

        Uses write-without-response (Write Command) so the ATT layer does not
        add a second ACK on top of the serial profile's own credit system.
        Chunk size is derived from the negotiated ATT MTU (CoreBluetooth
        negotiates this automatically; MTU minus 3 bytes overhead).
        """
        mtu = getattr(self._client, "mtu_size", 23)
        chunk = max(1, min(mtu - 3, config.BT_WRITE_CHUNK))
        for i in range(0, len(data), chunk):
            await self._client.write_gatt_char(
                self._rx_uuid, data[i : i + chunk], response=False
            )

    async def drain(self) -> None:
        pass  # BLE writes are already awaited

    def close(self) -> None:
        self._closed = True
        self._rx_event.set()  # unblock any waiting readline()
        if self._client:
            asyncio.ensure_future(self._client.disconnect())

    @property
    def is_closing(self) -> bool:
        if self._closed:
            return True
        return self._client is None or not self._client.is_connected

    # ── BLE callbacks ──────────────────────────────────────────────

    def _on_notify(self, _handle, data: bytearray) -> None:
        self._rx_buf.extend(data)
        self._rx_event.set()

    def _on_disconnect(self, _client) -> None:
        log.warning("BT: disconnected")
        self._closed = True
        self._rx_event.set()
