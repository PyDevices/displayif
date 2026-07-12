# displayif

Native display **interface** modules for pydisplay. Portable code in `ports/common/`; SoC-specific code under `ports/<mp-port>/`.

pydisplay MP board configs that raise `NotImplementedError` are waiting on modules built here. Native C modules register directly — **no Python re-export layer** in this repo.

**Status:** Phase 1 `spibus` shipped. Accelerated interfaces verified on esp32, mimxrt, samd, and rp2. See [HANDOFF.md](HANDOFF.md).

## Native modules

| Module | Port tree | pydisplay backend |
|--------|-----------|-------------------|
| `spibus` / `i2cbus` | `common` | **BusDisplay** |
| `rgbframebuffer` | `esp32` (RGB LCD SoCs), `mimxrt` (MIMXRT1062 eLCDIF) | **FBDisplay** |
| `i80bus` | `esp32` (S3 I80), `rp2` (PIO+DMA) | bus driver |
| `mipidsi` | `esp32` (P4 MIPI DSI), `mimxrt` (MIMXRT1176) | **FBDisplay** |
| `picodvi` | `rp2` (RP2040 PIO / RP2350 HSTX) | **FBDisplay** |
| `rgbmatrix` | `esp32` (S3) / `mimxrt` (1062) / `samd` (SAMD51) / `rp2` | **FBDisplay** |
| `rgbframebuffer` / `mipidsi` / `i80bus` stubs | `mimxrt` (`i80bus` only), `samd`; `rgbframebuffer`+`mipidsi` on `rp2` | import ok; ctor raises |

Parallel dot-clock RGB (RGB-666 and 16-pin RGB565) both use `rgbframebuffer.RGBFrameBuffer` — no `RGBDisplay`.

## Build

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

- [HANDOFF.md](HANDOFF.md) — pydisplay migration checklist, native API
- [PyDevices/pydisplay](https://github.com/PyDevices/pydisplay)
- [PyDevices/cmods](https://github.com/PyDevices/cmods) — optional build-shortcut workspace
