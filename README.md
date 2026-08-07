# displayif

Native display **interface** modules for pydisplay. Portable code in `src/ports/common/`; SoC-specific code under `src/ports/<mp-port>/`.

pydisplay MP board configs that raise `NotImplementedError` on import need firmware built with the matching displayif module. Native C modules register directly — **no Python re-export layer** in this repo.

**CircuitPython** has its own display stack (`dotclockframebuffer`, `mipidsi`, `picodvi`, …). displayif does not ship CP bindings — use pydisplay `cp_`* board configs with CP firmware.

**Status:** Accelerated interfaces on esp32, mimxrt, samd, and rp2. See [docs/PORT_MATRIX.md](docs/PORT_MATRIX.md).

Use this repo when a pydisplay board config needs a native display interface that is not available in the stock MicroPython port. In practice, you usually start with the target board’s port and the relevant display backend (for example `mipidsi`, `dotclockframebuffer`, or `picodvi`), then build the firmware with the matching `USER_C_MODULES` path. If you are debugging a bring-up problem, begin with [docs/SOFT_RESET_AND_BRINGUP.md](docs/SOFT_RESET_AND_BRINGUP.md) and the port notes in [docs/PORT_MATRIX.md](docs/PORT_MATRIX.md).

**Agents:** start at [AGENTS.md](AGENTS.md). Soft-reset / idempotent lifecycle
(**implemented**): [docs/IDEMPOTENT_LIFECYCLE.md](docs/IDEMPOTENT_LIFECYCLE.md). Bring-up /
failure modes (P4 `mipidsi`, Qualia `dotclockframebuffer.DotClockFramebuffer`):
[docs/SOFT_RESET_AND_BRINGUP.md](docs/SOFT_RESET_AND_BRINGUP.md).

## Native modules


| Module              | Port tree                                                                                       | pydisplay backend |
| ------------------- | ----------------------------------------------------------------------------------------------- | ----------------- |
| `spibus` / `i2cbus` | `common`                                                                                        | **BusDisplay**    |
| `dotclockframebuffer` | `esp32` (RGB LCD), `mimxrt` (1062 eLCDIF)                                              | **FBDisplay**     |
| `i80bus`            | `esp32` (S3), `rp2` (PIO+DMA), `mimxrt` (1062 FlexIO), `samd` (SAMD51 GPIO)                     | **BusDisplay**    |
| `qspibus`           | `esp32` (S3 esp_lcd SPI quad_mode); stubs elsewhere                                             | **BusDisplay**    |
| `mipidsi`           | `esp32` (P4), `mimxrt` (1176)                                                                   | **FBDisplay**     |
| `picodvi`           | `rp2` (RP2040 PIO / RP2350 HSTX)                                                                | **FBDisplay**     |
| `rgbmatrix`         | `esp32` (S3) / `mimxrt` (1062) / `samd` (SAMD51) / `rp2`                                        | **FBDisplay**     |
| stubs               | `samd` / `rp2` / non-1062 mimxrt (`dotclockframebuffer.DotClockFramebuffer`, `mipidsi`); non-1176 mimxrt (`mipidsi`); non-S3 `qspibus` | ctor raises |


Parallel dot-clock RGB uses **`dotclockframebuffer.DotClockFramebuffer`** (same module name as CircuitPython) — no `RGBDisplay`.

## ESP32 large framebuffers

RGB and DSI framebuffers prefer **PSRAM** (`MALLOC_CAP_SPIRAM`). Ensure `CONFIG_SPIRAM` is enabled and sized in your board `sdkconfig` before building — see [docs/PORT_MATRIX.md](docs/PORT_MATRIX.md#esp32-psram--sdkconfig-large-framebuffers).

## 🚀 Build

Clone as a sibling of `micropython/`:

```
workspace/
  displayif/      ← this repo
  micropython/
```

**Make ports** (mimxrt, samd, …): `USER_C_MODULES` is the **workspace parent** (directory that contains `displayif/` and any other `*/micropython.mk` siblings):

```bash
cd micropython/ports/mimxrt && make USER_C_MODULES=../../.. BOARD=TEENSY41
cd micropython/ports/samd && make USER_C_MODULES=../../.. BOARD=ADAFRUIT_METRO_M4_EXPRESS
```

**CMake ports** (esp32, rp2): `USER_C_MODULES` points at **this repo** (or `displayif/micropython.cmake`). CMake does not scan siblings the way Make does:

```bash
cd micropython/ports/esp32
make submodules BOARD=ESP32_GENERIC_S3
make BOARD=ESP32_GENERIC_S3 USER_C_MODULES=../../../displayif

cd micropython/ports/rp2
make BOARD=RPI_PICO USER_C_MODULES=../../../displayif
```

To build this module **plus** other usermods on a CMake port, pass a semicolon-separated list (no aggregator file required):

```bash
make BOARD=ESP32_GENERIC_S3 \
  USER_C_MODULES="/abs/path/to/displayif;/abs/path/to/lv_micropython_cmod"
```

See the [cmods workspace](https://github.com/PyDevices/cmods) for an easier way to build this repo with other user C modules.

## Related

- [docs/PORT_MATRIX.md](docs/PORT_MATRIX.md) — port matrix, hardware validation, RP2350 DSI notes
- [docs/ports/esp32.md](docs/ports/esp32.md) — Qualia DotClock + P4 mipidsi behavioral notes
- [PyDevices/pydisplay](https://github.com/PyDevices/pydisplay)

