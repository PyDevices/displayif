"""Smoke test for picodvi import and Framebuffer (rp2 PIO / HSTX).

Uses the smallest supported mode (320x240 @ 4bpp on RP2040). Full DVI output
needs Pimoroni Pico DV base or Pico 2 DVI sock hardware.
"""

import sys

print("displayif picodvi smoke test")

from picodvi import Framebuffer

if sys.platform != "rp2":
    try:
        Framebuffer(width=4, height=4)
    except NotImplementedError:
        print("picodvi not available on this SoC (expected off rp2)")
        raise SystemExit(0)
    raise AssertionError("expected NotImplementedError off rp2")

from machine import Pin

# GP6–GP13 differential pairs (Pimoroni Pico DV Demo Base pinout).
try:
    fb = Framebuffer(
        width=320,
        height=240,
        color_depth=4,
        clk_dp=Pin(7),
        clk_dn=Pin(6),
        red_dp=Pin(9),
        red_dn=Pin(8),
        green_dp=Pin(11),
        green_dn=Pin(10),
        blue_dp=Pin(13),
        blue_dn=Pin(12),
    )
except NotImplementedError as exc:
    print("picodvi not supported:", exc)
    raise SystemExit(0)

assert fb.width == 320
assert fb.height == 240
mv = memoryview(fb)
assert len(mv) > 0
assert mv.format in ("B", "H")

try:
    fb.refresh()
except (NotImplementedError, OSError, RuntimeError) as exc:
    print("refresh deferred or hardware unavailable:", type(exc).__name__)
else:
    print("refresh ok")

del fb

print("picodvi ok on", sys.platform)
