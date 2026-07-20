# displayif

Native display **interface** modules for pydisplay. Portable code in `ports/common/`; SoC-specific code under `ports/<mp-port>/`.

pydisplay MP board configs that raise `NotImplementedError` on import need firmware built with the matching displayif module. Native C modules register directly — **no Python re-export layer** in this repo.

**CircuitPython** has its own display stack (`dotclockframebuffer`, `mipidsi`, `picodvi`, …). displayif does not ship CP bindings — use pydisplay `cp_`* board configs with CP firmware.

**Status:** Accelerated interfaces on esp32, mimxrt, samd, and rp2. See [HANDOFF.md](HANDOFF.md).

**Agents:** start at [AGENTS.md](AGENTS.md). Soft-reset contract:
[IDEMPOTENT_LIFECYCLE.md](IDEMPOTENT_LIFECYCLE.md). Bring-up / failure modes:
[SOFT_RESET_AND_BRINGUP.md](SOFT_RESET_AND_BRINGUP.md).

## Native modules


| Module              | Port tree                                                                                       | pydisplay backend |
| ------------------- | ----------------------------------------------------------------------------------------------- | ----------------- |
| `spibus` / `i2cbus` | `common`                                                                                        | **BusDisplay**    |
| `rgbframebuffer`    | `esp32` (RGB LCD), `mimxrt` (1062 eLCDIF)                                                       | **FBDisplay**     |
| `i80bus`            | `esp32` (S3), `rp2` (PIO+DMA), `mimxrt` (1062 FlexIO), `samd` (SAMD51 GPIO)                     | **BusDisplay**    |
| `mipidsi`           | `esp32` (P4), `mimxrt` (1176)                                                                   | **FBDisplay**     |
| `picodvi`           | `rp2` (RP2040 PIO / RP2350 HSTX)                                                                | **FBDisplay**     |
| `rgbmatrix`         | `esp32` (S3) / `mimxrt` (1062) / `samd` (SAMD51) / `rp2`                                        | **FBDisplay**     |
| stubs               | `samd` (`rgbframebuffer`, `mipidsi`); `rp2` (`rgbframebuffer`, `mipidsi`); non-1062/1176 mimxrt | ctor raises       |


Parallel dot-clock RGB uses `rgbframebuffer.RGBFrameBuffer` — no `RGBDisplay`.

## ESP32 large framebuffers

RGB and DSI framebuffers prefer **PSRAM** (`MALLOC_CAP_SPIRAM`). Ensure `CONFIG_SPIRAM` is enabled and sized in your board `sdkconfig` before building — see [HANDOFF.md](HANDOFF.md#esp32-psram--sdkconfig-large-framebuffers).

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

([cmods](https://github.com/PyDevices/cmods) is an optional convenience workspace with `./build_mp.sh`; it is not required.)

## Related

- [HANDOFF.md](HANDOFF.md) — port matrix, hardware validation, RP2350 DSI notes
- [PyDevices/pydisplay](https://github.com/PyDevices/pydisplay)
- [PyDevices/cmods](https://github.com/PyDevices/cmods) — optional build-shortcut workspace; see `[MP_EXAMPLE.md](https://github.com/PyDevices/cmods/blob/main/MP_EXAMPLE.md)` for ESP32-P4 bring-up

