# samd port

Microchip SAMD display interfaces for MicroPython `samd` port.

## Native modules

| Source | Python import | SoC | pydisplay backend |
|--------|---------------|-----|-------------------|
| `ports/common/spi/mod_spibus.c` | `spibus.SPIBus` | all samd | **BusDisplay** |
| `ports/common/i2c/mod_i2cbus.c` | `i2cbus.I2CBus` | all samd | **BusDisplay** |
| `rgbmatrix_pm.c` + common `rgbmatrix` | `rgbmatrix.RGBMatrix` | SAMD51 (`ADAFRUIT_METRO_M4_EXPRESS`, Feather/ItsyBitsy M4, …) | **FBDisplay** |

On SAMD51 boards, `rgbmatrix` uses the **Protomatter** backend (TC3 overflow ISR + PORT OUTSET/OUTCLR). MicroPython's vector table does not wire TC3 by default; `samd_irq_hook.c` copies the table to RAM and patches the TC3 slot before scanout starts.

Pin arguments accept `machine.Pin` objects, integers, or port pin-name strings (`Pin("PB00")`, etc.).

## Build

```bash
./build_mp.sh --port samd --board ADAFRUIT_METRO_M4_EXPRESS
./build_mp.sh --port samd --board ADAFRUIT_FEATHER_M4_EXPRESS
```

Matrix Portal M4 has no dedicated MicroPython board definition; Metro M4 Express is the stand-in (same SAMD51J19A).

## pydisplay board configs

| Config | Notes |
|--------|-------|
| `fbdisplay/matrixportal_m4_64x32` | `Pin("PB00")` … HUB75 on Matrix Portal M4 |
| `fbdisplay/rgb_matrix_featherwing_64x32` | integer pins (Feather ESP32-S3 variant in repo) |
