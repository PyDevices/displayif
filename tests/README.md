# displayif tests

| Script | Port | Phase |
|--------|------|-------|
| `test_spi_smoke.py` | any SPI-capable (`rp2`, `esp32`, `mimxrt`, …) | 1 |
| `test_rgb565_smoke.py` | `esp32` + `t-rgb_480` board config | 2 |

Run from cmods workspace after build:

```bash
./micropython/ports/esp32/build-ESP32_GENERIC_S3/micropython displayif/tests/test_spi_smoke.py
```
