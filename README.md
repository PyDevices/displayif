# displayif

**Status:** source-integrated component — built into firmware from source as a user C module, not installed as a package. By design it has no versioned releases and is **not** in PyDevices' publishing set (unlike `pydevices` / `pydevices-desktop`). Maturity: **Alpha**. Issues: [PyDevices/displayif/issues](https://github.com/PyDevices/displayif/issues).

Native display **interface** modules for PyDevices `displaydev`. Portable code in `src/ports/common/`; SoC-specific code under `src/ports/<mp-port>/`.

New here? Read the [newcomer's guide](docs/newcomers.md) for the board-to-
backend map, firmware integration boundary, and lifecycle rules.

MicroPython board configs in `pydevices` that raise `NotImplementedError` on import need firmware built with the matching displayif module. Native C modules register directly — **no Python re-export layer** in this repo.

**CircuitPython** already has MCU display interfaces (`dotclockframebuffer`, `mipidsi`, `picodvi`, …) — use `pydevices/board_configs/cp/` with stock CP firmware for those. **Exception:** desktop `usdl2` (unix) is built from this repo via `./apply_cp_patches.sh` + CircuitPython unix.

**Status:** Accelerated interfaces on esp32, mimxrt, samd, and rp2. See [docs/port-matrix.md](docs/port-matrix.md).

Use this repo when a `pydevices` board config needs a native display interface that is not available in the stock MicroPython port. In practice, you usually start with the target board’s port and the relevant display backend (for example `mipidsi`, `dotclockframebuffer`, or `picodvi`), then build the firmware with this module in it (see [Build](#-build)). If you are debugging a bring-up problem, begin with [docs/soft-reset-and-bring-up.md](docs/soft-reset-and-bring-up.md) and the port notes in [docs/port-matrix.md](docs/port-matrix.md).

**Agents:** start at [AGENTS.md](AGENTS.md). Soft-reset / idempotent lifecycle
(**implemented**): [docs/idempotent-lifecycle.md](docs/idempotent-lifecycle.md). Bring-up /
failure modes (P4 `mipidsi`, Qualia `dotclockframebuffer.DotClockFramebuffer`):
[docs/soft-reset-and-bring-up.md](docs/soft-reset-and-bring-up.md).

## Native modules


| Module              | Port tree                                                                                       | `displaydev` backend |
| ------------------- | ----------------------------------------------------------------------------------------------- | ----------------- |
| `spibus` / `i2cbus` | `common`                                                                                        | **BusDisplay**    |
| `dotclockframebuffer` | `esp32` (RGB LCD), `mimxrt` (1062 eLCDIF)                                              | **FBDisplay**     |
| `i80bus`            | `esp32` (S3), `rp2` (PIO+DMA), `mimxrt` (1062 FlexIO), `samd` (SAMD51 GPIO)                     | **BusDisplay**    |
| `qspibus`           | `esp32` (S3 esp_lcd SPI quad_mode); stubs elsewhere                                             | **BusDisplay**    |
| `mipidsi`           | `esp32` (P4), `mimxrt` (1176)                                                                   | **FBDisplay**     |
| `picodvi`           | `rp2` (RP2040 PIO / RP2350 HSTX)                                                                | **FBDisplay**     |
| `rgbmatrix`         | `esp32` (S3) / `mimxrt` (1062) / `samd` (SAMD51) / `rp2`                                        | **FBDisplay**     |
| `usdl2`             | `desktop` (MicroPython `unix` / `windows`; CircuitPython unix via `apply_cp_patches.sh`)       | **SDLDisplay**    |
| `jpegio`            | every port (`src/jpegio/`, platform-neutral; CircuitPython has it natively)                     | any — decodes JPEG to RGB565 for `blit_rect`; beside `lvgl-micropython` it is also LVGL's JPEG decoder ([docs](src/jpegio/README.md)) |
| stubs               | `samd` / `rp2` / non-1062 mimxrt (`dotclockframebuffer.DotClockFramebuffer`, `mipidsi`); non-1176 mimxrt (`mipidsi`); non-S3 `qspibus` | ctor raises |


Parallel dot-clock RGB uses **`dotclockframebuffer.DotClockFramebuffer`** (same module name as CircuitPython) — no `RGBDisplay`.

## ESP32 large framebuffers

RGB and DSI framebuffers prefer **PSRAM** (`MALLOC_CAP_SPIRAM`). Ensure `CONFIG_SPIRAM` is enabled and sized in your board `sdkconfig` before building — see [docs/port-matrix.md](docs/port-matrix.md#esp32-psram--sdkconfig-large-framebuffers).

## 🚀 Build

Tested against MicroPython v1.29.0, the CircuitPython 10.2.1 oracle, and SDL2 >= 2.0
(desktop `usdl2`) — see [UPSTREAM](UPSTREAM) for exact pins and how to verify them locally.

On MicroPython 1.29 or later, clone this repository anywhere and add one line
to the manifest your build already uses:

```python
include("/path/to/displayif/manifest.py")
```

On unix that manifest is `ports/unix/variants/standard/manifest.py`; on esp32
and rp2 it is usually `ports/<port>/boards/manifest.py`, unless your board
brings its own. Then build as usual:

```bash
# esp32, after sourcing ESP-IDF's export.sh
cd micropython/ports/esp32 && make BOARD=ESP32_GENERIC_S3
# or unix
cd micropython/ports/unix && make submodules && make
```

The manifest freezes no Python; it only names the C module, and the build glue
picks the modules your port supports (see [docs/port-matrix.md](docs/port-matrix.md)).
On unix that is `usdl2` and `jpegio`; the bus and framebuffer modules are
MCU-only. Tested on the unix port against MicroPython v1.29.0.

If you would rather not edit the MicroPython tree, write a manifest of your own
and pass it as `FROZEN_MANIFEST=`. That replaces the port's default, so include
the default too (`include("$(PORT_DIR)/variants/standard/manifest.py")` on
unix, `include("$(PORT_DIR)/boards/manifest.py")` on esp32) or you lose
`asyncio` and the port's other frozen modules.

**Older than 1.29?** Manifests there have no `c_module()`; use
`USER_C_MODULES` instead. On esp32 and rp2 point it at this repository:

```bash
make BOARD=ESP32_GENERIC_S3 USER_C_MODULES=/abs/path/to/displayif
make BOARD=ESP32_GENERIC_S3 \
  USER_C_MODULES="/abs/path/to/displayif;/abs/path/to/lvgl-micropython"
```

On Make ports (unix, windows, mimxrt, samd) point it at the directory that
*contains* this repository, which builds every module in that directory:

```bash
cd micropython/ports/samd && make USER_C_MODULES=../../.. BOARD=ADAFRUIT_METRO_M4_EXPRESS
```

**Desktop SDL (`usdl2`)** builds automatically on the `unix` and `windows`
ports. Unix needs `libsdl2-dev`. Windows (MinGW) needs an unpacked
[SDL2 MinGW development ZIP](https://github.com/libsdl-org/SDL/releases) and
`SDL2_DEV` pointing at its root (see [`tools/sdl2_dev_env.sh`](tools/sdl2_dev_env.sh)).

For several PyDevices modules at once,
[micropython-pydevices](https://github.com/PyDevices/micropython-pydevices)
keeps ready-made manifests, variants and boards.

CircuitPython unix: `./apply_cp_patches.sh --apply --port unix --variant coverage`, then build the unix port.

### First run (unix `usdl2` smoke)

Once the unix port above is built, prove it imports and can drive SDL2:

```bash
./micropython/ports/unix/build-standard/micropython -c "
import usdl2
assert usdl2.SDL_Init(usdl2.SDL_INIT_VIDEO) == 0, usdl2.SDL_GetError()
win = usdl2.SDL_CreateWindow(
    'displayif smoke', usdl2.SDL_WINDOWPOS_UNDEFINED, usdl2.SDL_WINDOWPOS_UNDEFINED,
    320, 240, usdl2.SDL_WINDOW_SHOWN)
print('usdl2 OK:', win)
usdl2.SDL_DestroyWindow(win)
usdl2.SDL_Quit()"
```

A live SDL window appears on a desktop session. Headless CI runs the same
import/init smoke with `SDL_VIDEODRIVER=dummy` so it works without a display
server — see [.github/workflows/clean-build.yml](.github/workflows/clean-build.yml).

Then run the lifecycle API test under the same interpreter:

```bash
./micropython/ports/unix/build-standard/micropython tests/test_lifecycle_api.py
```

## Third-party

`src/ports/common/rgbmatrix/protomatter/` vendors
[adafruit/Adafruit_Protomatter](https://github.com/adafruit/Adafruit_Protomatter)
(BSD). See [UPSTREAM](UPSTREAM) and
[the protomatter/ README](src/ports/common/rgbmatrix/protomatter/README.md)
for the identified upstream revision and license text.

`src/jpegio/tjpgd/` vendors ChaN's TJpgDec R0.03 (with patch1, via
CircuitPython's `lib/tjpgd`) under its own permissive notice; see
[src/jpegio/README.md](src/jpegio/README.md#notice--tjpgdec).

## Related

- [docs/port-matrix.md](docs/port-matrix.md) — port matrix, hardware validation, RP2350 DSI notes
- [docs/ports/esp32.md](docs/ports/esp32.md) — Qualia DotClock + P4 mipidsi behavioral notes
- [PyDevices/pydevices](https://github.com/PyDevices/pydevices) — canonical drivers and board configs
- [PyDevices/pydevices-examples](https://github.com/PyDevices/pydevices-examples) — examples and gallery
