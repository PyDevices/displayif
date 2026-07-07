"""Smoke test for rgbmatrix import, tile height, and buffer protocol."""

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
    tile=1,
)

assert matrix.width == 4
assert matrix.height == 4
mv = memoryview(matrix)
assert len(mv) == 4 * 4
assert mv.format == "H"

# Two 4-row panels stacked (tile=2) => height 8 with one addr line.
tiled = RGBMatrix(
    width=4,
    bit_depth=1,
    rgb_pins=(0, 1, 2, 3, 4, 5),
    addr_pins=(6,),
    clock_pin=7,
    latch_pin=8,
    output_enable_pin=9,
    doublebuffer=False,
    serpentine=False,
    tile=2,
)
assert tiled.height == 8

try:
    matrix.refresh()
except OSError:
    print("refresh skipped (no matrix hardware)")
else:
    print("refresh ok")

print("rgbmatrix ok on", sys.platform)
