"""Smoke test for native spibus (displayif ports/common/spi)."""

import sys

print("displayif spibus smoke test")

import spibus

bus = spibus.SPIBus(
    id=0,
    baudrate=1_000_000,
    sck=18,
    mosi=19,
    miso=-1,
    dc=9,
    cs=10,
    reset=6,
)

assert bus.send is not None
bus.send(0x00)
bus.send(None, b"\x00\x00")
bus.reset()
bus.deinit()

print("spibus ok on", sys.platform)
