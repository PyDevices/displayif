# displayif tests

| Script | Port | Notes |
|--------|------|-------|
| `test_spi_smoke.py` | MCU with SPI (`esp32`, …) | native `spibus` |
| `test_rgbframebuffer_smoke.py` | `esp32` | buffer protocol; `refresh()` raises until scanout lands |
| `test_mipidsi_smoke.py` | `esp32p4` | import `Bus` / `Display`; full panel needs Waveshare 4B hardware |

```bash
# After cmods build with displayif:
./micropython/ports/rp2/build-*/micropython displayif/tests/test_spi_smoke.py
./micropython/ports/esp32/build-*/micropython displayif/tests/test_rgbframebuffer_smoke.py
```

Do not install pydisplay viper `spibus` in the same firmware as displayif `spibus`.
