"""Entry point: python -m bridge  [--transport usb|ble]"""

import argparse
import asyncio
import logging
import os
import signal

from . import config
from .daemon import Daemon


def _make_transport(name: str):
    if name == "ble":
        from .transport_bt import BtTransport
        return BtTransport()
    if name == "usb":
        from .transport_usb import UsbTransport
        return UsbTransport()
    # "auto": try USB first, fall back to BLE
    from .transport_auto import AutoTransport
    return AutoTransport()


def main():
    parser = argparse.ArgumentParser(description="Flipper Claude Buddy bridge")
    parser.add_argument(
        "--transport",
        choices=["auto", "usb", "ble"],
        default=config.TRANSPORT,
        help="Communication transport (default: %(default)s)",
    )
    parser.add_argument(
        "--log-level",
        choices=["debug", "info", "warning", "error"],
        default=os.environ.get("FLIPPER_LOG_LEVEL", "info"),
        help="Log level (default: info, env: FLIPPER_LOG_LEVEL)",
    )
    args = parser.parse_args()

    logging.basicConfig(
        level=getattr(logging, args.log_level.upper()),
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )

    transport = _make_transport(args.transport)
    daemon = Daemon(transport)

    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, lambda: loop.stop())

    try:
        loop.run_until_complete(daemon.run())
    except KeyboardInterrupt:
        pass
    finally:
        loop.close()
        logging.info("Bridge stopped.")


if __name__ == "__main__":
    main()
