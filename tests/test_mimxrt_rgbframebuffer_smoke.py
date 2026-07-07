"""Smoke test for rgbframebuffer import and buffer protocol (mimxrt eLCDIF).

Small 4x4 buffer on MIMXRT1062 (Teensy 4.x / MIMXRT1060-EVK). Full RK043 panel
bring-up needs the MIMXRT1060-EVK + shield hardware.
"""

import sys

print("displayif mimxrt rgbframebuffer smoke test")

from rgbframebuffer import RGBFrameBuffer

if sys.platform != "mimxrt":
    try:
        RGBFrameBuffer(
            de=0,
            vsync=0,
            hsync=0,
            dclk=0,
            data=tuple(range(16)),
            frequency=1,
            width=4,
            height=4,
            hsync_pulse_width=1,
            hsync_front_porch=1,
            hsync_back_porch=1,
            vsync_pulse_width=1,
            vsync_front_porch=1,
            vsync_back_porch=1,
            hsync_idle_low=False,
            vsync_idle_low=False,
            de_idle_high=False,
            pclk_active_high=False,
            pclk_idle_high=False,
        )
    except NotImplementedError:
        print("rgbframebuffer not available on this SoC (expected off mimxrt)")
        raise SystemExit(0)
    raise AssertionError("expected NotImplementedError off mimxrt")

from machine import Pin

_data = tuple(Pin(i) for i in range(16))

try:
    fb = RGBFrameBuffer(
        de=Pin(0),
        vsync=Pin(1),
        hsync=Pin(2),
        dclk=Pin(3),
        data=_data,
        frequency=9_000_000,
        width=4,
        height=4,
        hsync_pulse_width=2,
        hsync_front_porch=1,
        hsync_back_porch=1,
        vsync_pulse_width=2,
        vsync_front_porch=1,
        vsync_back_porch=1,
        hsync_idle_low=True,
        vsync_idle_low=True,
        de_idle_high=False,
        pclk_active_high=False,
        pclk_idle_high=False,
    )
except NotImplementedError as exc:
    print("rgbframebuffer not available on this mimxrt SoC:", exc)
    raise SystemExit(0)

assert fb.width == 4
assert fb.height == 4
mv = memoryview(fb)
assert len(mv) == 4 * 4
assert mv.format == "H"

try:
    fb.refresh()
except (NotImplementedError, OSError) as exc:
    print("refresh deferred or hardware unavailable:", type(exc).__name__)
else:
    print("refresh ok")

print("rgbframebuffer ok on", sys.platform)
