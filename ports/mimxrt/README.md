# mimxrt port

NXP i.MX RT display interfaces for MicroPython `mimxrt` port / CircuitPython `mimxrt` port.

## Native modules

| Source | Python import | SoC | pydisplay backend |
|--------|---------------|-----|-------------------|
| `ports/common/spi/mod_spibus.c` | `spibus.SPIBus` | all mimxrt | **BusDisplay** |
| `ports/common/i2c/mod_i2cbus.c` | `i2cbus.I2CBus` | all mimxrt | **BusDisplay** |
| `rgbmatrix_pm.c` + common `rgbmatrix` | `rgbmatrix.RGBMatrix` | MIMXRT1062 (`TEENSY40`, `TEENSY41`, `MIMXRT1060_EVK`) | **FBDisplay** |

On MIMXRT1062 boards, `rgbmatrix` uses the **Protomatter** backend (PIT timer ISR + GPIO set/clear registers). Other mimxrt chips (RT1011, RT1170, …) still get `rgbmatrix` with GPIO bitbang `refresh()`; `tile>1` requires Protomatter.

Pin arguments accept `machine.Pin` objects, integers, or port pin-name strings.

## Build

```bash
./build_mp.sh --port mimxrt --board TEENSY41
./build_mp.sh --port mimxrt --board TEENSY40
```

## Future accelerated backends

LCDIF, ELCDIF, and FlexIO panel drivers are not implemented yet — add here when pydisplay board configs need them.
