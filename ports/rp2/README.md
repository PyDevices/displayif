# rp2 port

Raspberry Pi RP2040/RP2350 display interfaces for MicroPython `rp2` port.

## Native modules

| Source | Python import | SoC | pydisplay backend |
|--------|---------------|-----|-------------------|
| `ports/common/spi/mod_spibus.c` | `spibus.SPIBus` | RP2040 / RP2350 | **BusDisplay** |
| `ports/common/i2c/mod_i2cbus.c` | `i2cbus.I2CBus` | RP2040 / RP2350 | **BusDisplay** |
| `rgbmatrix_pm.c` + common `rgbmatrix` | `rgbmatrix.RGBMatrix` | RP2040 / RP2350 | **FBDisplay** |

`rgbmatrix` uses the **Protomatter** backend (PWM slice 7 wrap IRQ + GPIO set/clr). Pin arguments accept `machine.Pin` objects, integers, or board pin names.

There is no native `i80bus` on RP2040 — pydisplay's `ili9341_i80_rp2040` config still needs a PIO/DMA I80 driver (not implemented here).

## Build

```bash
./build_mp.sh --port rp2 --board RPI_PICO
./build_mp.sh --port rp2 --board RPI_PICO2_W
```
