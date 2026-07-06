"""Smoke test for rgbmatrix import and buffer protocol (MCU ports)."""

import sys

print("displayif rgbmatrix smoke test")

from rgbmatrix import RGBMatrix

matrix = RGBMatrix(
    width=4,
    height=4,
    bit_depth=1,
    rgb_pins=(0, 1, 2, 3, 4, 5),
    addr_pins=(6,),
    clock_pin=7,
    latch_pin=8,
    output_enable_pin=9,
    doublebuffer=False,
    serpentine=False,
)

assert matrix.width == 4
assert matrix.height == 4
mv = memoryview(matrix)
assert len(mv) == 4 * 4
assert mv.format == "H"

try:
    matrix.refresh()
except OSError:
    print("refresh skipped (no matrix hardware)")
else:
    print("refresh ok")

print("rgbmatrix ok on", sys.platform)
