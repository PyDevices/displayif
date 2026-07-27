# samd port

Microchip SAMD display interfaces for MicroPython `samd` port.

## Native modules

| Source | Python import | SoC | pydisplay backend |
|--------|---------------|-----|-------------------|
| `src/ports/common/spi/mod_spibus.c` | `spibus.SPIBus` | all samd | **BusDisplay** |
| `src/ports/common/i2c/mod_i2cbus.c` | `i2cbus.I2CBus` | all samd | **BusDisplay** |
| `rgbmatrix_pm.c` + common `rgbmatrix` | `rgbmatrix.RGBMatrix` | SAMD51 | **FBDisplay** |
| `mod_i80bus.c` + `common/i80bus/gpio_bitbang.c` | `i80bus.I80Bus` | SAMD51 | bus driver |
| `src/ports/common/notimpl/mod_dotclockframebuffer.c` | `dotclockframebuffer.DotClockFramebuffer` | stub | **FBDisplay** (N/A) |
| `src/ports/common/notimpl/mod_mipidsi.c` | `mipidsi.Bus` / `Display` | stub | **FBDisplay** (N/A) |

### `i80bus` (GPIO bit-bang)

On **SAMD51**, `i80bus` uses PORT `OUTSET`/`OUTCLR` register writes via the shared `common/i80bus/gpio_bitbang.c` backend (same algorithm as pydisplay’s viper `I80Bus`). Supports **8 data pins** via `data0` (eight consecutive) or `data_pins` (sequential or LUT layout). ParallelBus kwargs: optional `read` / `reset`, default `frequency` 30 MHz.

- No dedicated 8080 peripheral — throughput is lower than esp32-S3, rp2 PIO, or mimxrt FlexIO.
- `frequency` is accepted for CircuitPython API compatibility; timing is not cycle-counted yet.
- SAMD21 builds keep the import-only stub.

On SAMD51, `rgbmatrix` uses the **Protomatter** backend (TC3 overflow ISR + PORT OUTSET/OUTCLR). `samd_irq_hook.c` patches the TC3 vector before scanout starts.

## Build

Make port — `USER_C_MODULES` is the workspace parent (sibling layout with `displayif/`):

```bash
cd micropython/ports/samd
make USER_C_MODULES=../../.. BOARD=ADAFRUIT_METRO_M4_EXPRESS
make USER_C_MODULES=../../.. BOARD=ADAFRUIT_FEATHER_M4_EXPRESS
```

## pydisplay board configs

| Config | Notes |
|--------|-------|
| `fbdisplay/matrixportal_m4_64x32` | HUB75 on Matrix Portal M4 |
| `fbdisplay/rgb_matrix_featherwing_64x32` | FeatherWing (integer pins) |

CP siblings use CircuitPython `rgbmatrix` — not displayif.
