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
| `ports/common/notimpl/mod_mipidsi.c` | `mipidsi.Bus` / `Display` | stub | **FBDisplay** (N/A) |

On MIMXRT1062 boards, `rgbmatrix` uses the **Protomatter** backend (PIT timer ISR + GPIO set/clear registers). Other mimxrt chips (RT1011, RT1170, …) still get `rgbmatrix` with GPIO bitbang `refresh()`; `tile>1` requires Protomatter.

Stub modules import on all boards but raise `NotImplementedError` from the constructor.

Pin arguments accept `machine.Pin` objects, integers, or port pin-name strings.

## Build

```bash
./build_mp.sh --port mimxrt --board TEENSY41
./build_mp.sh --port mimxrt --board TEENSY40
```

## Future accelerated backends

NXP SDK **FlexIO** (I80 parallel) and **LCDIF/eLCDIF** (RGB framebuffer) drivers are not implemented yet — add here when pydisplay board configs need them.
