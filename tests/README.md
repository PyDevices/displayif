# displayif tests

| Script | Port | Notes |
|--------|------|-------|
| `test_spi_smoke.py` | MCU with SPI (`rp2`, `esp32`, …) | native `spibus` |
| `test_rgbframebuffer_smoke.py` | `esp32` (RGB LCD SoC) | buffer protocol; `refresh()` uses esp_lcd scanout |
| `test_i80bus_smoke.py` | `esp32-S3` | import `I80Bus` |
| `test_mipidsi_smoke.py` | `esp32p4` | import `Bus` / `Display`; full panel needs Waveshare 4B hardware |
| `test_rgbmatrix_smoke.py` | MCU with `rgbmatrix` (Protomatter on S3 / 1062 / SAMD51 / rp2) | buffer + tile height |
| `test_i2cbus_smoke.py` | MCU with I2C | native `i2cbus` |

```bash
# After cmods build with displayif (flash uf2 to board for rp2):
./micropython/ports/rp2/build-RPI_PICO/firmware.uf2
./micropython/ports/mimxrt/build-TEENSY41/micropython displayif/tests/test_rgbmatrix_smoke.py
./micropython/ports/esp32/build-*/micropython displayif/tests/test_rgbframebuffer_smoke.py
```

Do not install pydisplay viper `spibus` in the same firmware as displayif `spibus`.
