# Protomatter (vendored)

`core.c`, `core.h`, and `arch/` in this directory are vendored from
[adafruit/Adafruit_Protomatter](https://github.com/adafruit/Adafruit_Protomatter),
Adafruit's portable HUB75 RGB-matrix driving core. `mod_rgbmatrix.c` and
`protomatter_mp.c` (one directory up) are displayif's own MicroPython
binding on top of it — those are not vendored and may be edited freely.
**Do not edit the files in this directory** (`core.c`, `core.h`, `arch/*.h`);
patch at the binding layer or re-vendor instead.

## License

BSD, per `core.c`'s own header:

> BSD license, all text here must be included in any redistribution.

## Upstream revision

`core.c` and `core.h` are byte-identical to
[tag `1.7.1`](https://github.com/adafruit/Adafruit_Protomatter/releases/tag/1.7.1)
(commit `629d5007f0dfeba244ae81759ce2ba568224cd8a`) once whitespace and
formatting are normalized — the only differences are pointer-spacing
(`Type* x` vs `Type *x`), brace style, and include order, with zero logic
changes. This was confirmed by fetching `src/core.c` and `src/core.h` at
that tag from the GitHub API and diffing token-for-token against the files
here.

`arch/*.h` mostly matches the same tag the same way (`esp32*.h`, `nrf52.h`,
`samd21.h`, `stm32.h`), but several files carry genuine local changes on
top of the 1.7.1 baseline, made to integrate with displayif's MicroPython
usermod build and CircuitPython patch layer:

- `arch/rp2040.h` — includes `../../hardware_pwm/include/hardware/pwm.h`
  instead of `hardware/pwm.h` (displayif's PIO/hardware layout), and adds a
  `DISPLAYIF_RGBMATRIX_USE_PROTOMATTER`-guarded include for the CIRCUITPY
  branch.
- `arch/teensy4.h` — narrows the CPU guard to `__IMXRT1062__` only (drops
  `CPU_MIMXRT1062` / `CPU_MIMXRT1064`, which are Arduino/Teensyduino-only
  macros not present in this build).
- `arch/samd51.h`, `arch/samd-common.h`, `arch/esp32-s3.h`, `arch/arch.h` —
  differ beyond formatting; not yet diffed line-by-line against 1.7.1.

Exact import date and whether these arch changes were applied at vendoring
time vs. accumulated since are unrecorded. Re-vendoring cleanly at a pinned
tag (with the local arch/*.h deltas captured as an explicit patch set
instead of baked-in edits) is tracked work, not done here.

See [`../../../../../UPSTREAM`](../../../../../UPSTREAM) for how this fits with
the rest of displayif's pinned dependencies.
