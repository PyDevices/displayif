"""Smoke test for rgbframebuffer import and buffer protocol (esp32)."""

import sys

print("displayif rgbframebuffer smoke test")

from rgbframebuffer import RGBFrameBuffer

fb = RGBFrameBuffer(
    de=17,
    vsync=3,
    hsync=46,
    dclk=9,
    red=(1, 2),
    green=(21, 47),
    blue=(10, 11),
    frequency=16_000_000,
    width=4,
    height=4,
    hsync_pulse_width=2,
    hsync_front_porch=1,
    hsync_back_porch=1,
    vsync_pulse_width=2,
    vsync_front_porch=1,
    vsync_back_porch=1,
    hsync_idle_low=False,
    vsync_idle_low=False,
    de_idle_high=False,
    pclk_active_high=False,
    pclk_idle_high=False,
)

assert fb.width == 4
assert fb.height == 4
mv = memoryview(fb)
assert len(mv) == 4 * 4
assert mv.format == "H"

try:
    fb.refresh()
except NotImplementedError:
    print("refresh stub ok (scanout not implemented yet)")
else:
    print("refresh ok")

print("rgbframebuffer ok on", sys.platform)
