# displayif tests

| Script | Port | Notes |
|--------|------|-------|
| `test_spi_smoke.py` | MCU with SPI (`rp2`, `esp32`, …) | native `spibus` |
| `test_dotclockframebuffer_smoke.py` | `esp32` (RGB LCD SoC) | `displayif.DotClockFramebuffer`; buffer protocol; esp_lcd scanout |
| `test_mimxrt_dotclockframebuffer_smoke.py` | `mimxrt` (MIMXRT1062) | small buffer; `refresh()` uses eLCDIF scanout |
| `test_i80bus_smoke.py` | `esp32-S3`, `rp2`, `mimxrt` (1062), `samd` (SAMD51) | import `I80Bus` |
| `test_mipidsi_smoke.py` | `esp32p4` | import `Bus` / `Display`; full panel needs Waveshare 4B hardware |
| `test_mimxrt_mipidsi_smoke.py` | `mimxrt` (MIMXRT1176) | import `Bus` / `Display`; full panel needs MIMXRT1170-EVK + Waveshare 5\" DSI |
| `test_picodvi_smoke.py` | `rp2` | import `Framebuffer`; smallest mode (320x240); full DVI needs Pico DV hardware |
| `test_rgbmatrix_smoke.py` | MCU with `rgbmatrix` (Protomatter on S3 / 1062 / SAMD51 / rp2) | buffer + tile height |
| `test_i2cbus_smoke.py` | MCU with I2C | native `i2cbus` |
| `test_lifecycle_api.py` | any port with displayif linked | import-only: types expose `deinit` / `__del__`; hardware soft-reset/idempotent ctor needs board (contract in `IDEMPOTENT_LIFECYCLE.md`; P4 `mipidsi` + Qualia `displayif.DotClockFramebuffer` bring-up notes in `SOFT_RESET_AND_BRINGUP.md`) |

```bash
# After a build with displayif linked in (flash uf2 to board for rp2):
./micropython/ports/rp2/build-RPI_PICO/firmware.uf2
./micropython/ports/rp2/build-RPI_PICO/micropython displayif/tests/test_picodvi_smoke.py
./micropython/ports/mimxrt/build-TEENSY41/micropython displayif/tests/test_rgbmatrix_smoke.py
./micropython/ports/mimxrt/build-TEENSY41/micropython displayif/tests/test_mimxrt_dotclockframebuffer_smoke.py
./micropython/ports/mimxrt/build-MIMXRT1170_EVK/micropython displayif/tests/test_mimxrt_mipidsi_smoke.py
./micropython/ports/esp32/build-*/micropython displayif/tests/test_dotclockframebuffer_smoke.py
```

Do not install pydisplay viper `spibus` in the same firmware as displayif `spibus`.
