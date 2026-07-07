# displayif

Native display **interface** cmods for pydisplay. Portable code in `ports/common/`; SoC-specific code under `ports/<mp-port>/`.

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

- [HANDOFF.md](HANDOFF.md) — pydisplay migration checklist, native API
- [PyDevices/pydisplay](https://github.com/PyDevices/pydisplay)
- [PyDevices/cmods](https://github.com/PyDevices/cmods)
