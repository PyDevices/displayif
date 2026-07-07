# mimxrt port

NXP i.MX RT display interfaces for MicroPython `mimxrt` port / CircuitPython `mimxrt` port.

## Native modules

| Source | Python import | SoC | pydisplay backend |
|--------|---------------|-----|-------------------|
| `ports/common/spi/mod_spibus.c` | `spibus.SPIBus` | all mimxrt | **BusDisplay** |
| `ports/common/i2c/mod_i2cbus.c` | `i2cbus.I2CBus` | all mimxrt | **BusDisplay** |
| `rgbmatrix_pm.c` + common `rgbmatrix` | `rgbmatrix.RGBMatrix` | MIMXRT1062 (`TEENSY40`, `TEENSY41`, `MIMXRT1060_EVK`) | **FBDisplay** |
| `ports/common/notimpl/mod_rgbframebuffer.c` | `rgbframebuffer.RGBFrameBuffer` | stub | **FBDisplay** (N/A) |
| `ports/common/notimpl/mod_i80bus.c` | `i80bus.I80Bus` | stub | bus driver (N/A) |
| `ports/common/notimpl/mod_mipidsi.c` | `mipidsi.Bus` / `Display` | stub (RT1176 target) | **FBDisplay** (N/A) |

## pydisplay board configs

| Config | Module | Hardware |
|--------|--------|----------|
| `fbdisplay/mimxrt1060_evk_rk043_rgb` | `rgbframebuffer` (stub) | MIMXRT1060-EVK + RK043 shield (J49) |
| `fbdisplay/mimxrt1170_evk_waveshare_5dsi` | `mipidsi` (stub) | MIMXRT1170-EVK + Waveshare 5" DSI on J84 |
| `fbdisplay/matrixportal_m4_64x32` | `rgbmatrix` | Metro M4 (SAMD — see samd port) |
| `fbdisplay/rgb_matrix_featherwing_teensy41_64x32` | `rgbmatrix` | Teensy 4.1 + FeatherWing |

CircuitPython board IDs: [imxrt1060_evk](https://circuitpython.org/board/imxrt1060_evk/), [nxp_mimxrt1170_evk](https://circuitpython.org/board/nxp_mimxrt1170_evk/).

On MIMXRT1062 boards, `rgbmatrix` uses the **Protomatter** backend (PIT timer ISR + GPIO set/clear registers). Other mimxrt chips (RT1011, RT1170, …) still get `rgbmatrix` with GPIO bitbang `refresh()`; `tile>1` requires Protomatter.

Stub modules import on all boards but raise `NotImplementedError` from the constructor.

Pin arguments accept `machine.Pin` objects, integers, or port pin-name strings.

## Build

```bash
./build_mp.sh --port mimxrt --board TEENSY41
./build_mp.sh --port mimxrt --board TEENSY40
```

## Future accelerated backends

NXP SDK **FlexIO** (I80 parallel), **LCDIF/eLCDIF** (`rgbframebuffer`), and **MIPI DSI** (`mipidsi` on RT1176) are not implemented yet — add here when pydisplay board configs need them.
