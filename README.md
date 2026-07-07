# displayif

Native display **interface** cmods for **MicroPython** / pydisplay. Portable code in `ports/common/`; SoC-specific code under `ports/<mp-port>/`.

pydisplay MP board configs that raise `NotImplementedError` on import need firmware built with the matching displayif module. Native C modules register directly — **no Python re-export layer** in this repo.

**CircuitPython** has its own display stack (`dotclockframebuffer`, `mipidsi`, `picodvi`, …). displayif does not ship CP bindings — use pydisplay `cp_*` board configs with CP firmware.

**Status:** Accelerated interfaces on esp32, mimxrt, samd, and rp2. See [HANDOFF.md](HANDOFF.md).

## Native modules

| Module | Port tree | pydisplay backend |
|--------|-----------|-------------------|
| `spibus` / `i2cbus` | `common` | **BusDisplay** |
| `rgbframebuffer` | `esp32` (RGB LCD), `mimxrt` (1062 eLCDIF) | **FBDisplay** |
| `i80bus` | `esp32` (S3), `rp2` (PIO+DMA), `mimxrt` (1062 FlexIO), `samd` (SAMD51 GPIO) | bus driver |
| `mipidsi` | `esp32` (P4), `mimxrt` (1176) | **FBDisplay** |
| `picodvi` | `rp2` (RP2040 PIO / RP2350 HSTX) | **FBDisplay** |
| `rgbmatrix` | `esp32` (S3) / `mimxrt` (1062) / `samd` (SAMD51) / `rp2` | **FBDisplay** |
| stubs | `samd` (`rgbframebuffer`, `mipidsi`); `rp2` (`rgbframebuffer`, `mipidsi`); non-1062/1176 mimxrt | ctor raises |

Parallel dot-clock RGB uses `rgbframebuffer.RGBFrameBuffer` — no `RGBDisplay`.

## ESP32 large framebuffers

RGB and DSI framebuffers prefer **PSRAM** (`MALLOC_CAP_SPIRAM`). Ensure `CONFIG_SPIRAM` is enabled and sized in your board `sdkconfig` before building — see [HANDOFF.md](HANDOFF.md#esp32-psram--sdkconfig-large-framebuffers).

## Build (cmods workspace)

```bash
cd ~/github/cmods
git clone https://github.com/PyDevices/displayif.git displayif
./build_mp.sh --port esp32 --board ESP32_GENERIC_S3
./build_mp.sh --port mimxrt --board TEENSY41
./build_mp.sh --port samd --board ADAFRUIT_METRO_M4_EXPRESS
./build_mp.sh --port rp2 --board RPI_PICO
```

## Related

- [HANDOFF.md](HANDOFF.md) — port matrix, hardware validation, RP2350 DSI notes
- [PyDevices/pydisplay](https://github.com/PyDevices/pydisplay)
- [PyDevices/cmods](https://github.com/PyDevices/cmods) — [`MP_EXAMPLE.md`](https://github.com/PyDevices/cmods/blob/main/MP_EXAMPLE.md) ESP32-P4 bring-up
