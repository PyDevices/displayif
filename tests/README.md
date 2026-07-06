# displayif tests

| Script | Port | Phase |
|--------|------|-------|
| `test_spi_smoke.py` | any SPI-capable (`rp2`, `esp32`, `mimxrt`, …) | 1 |
| `test_rgbpanel_smoke.py` | `esp32` + parallel-RGB board config | 2 |
| `test_rgb666_smoke.py` | `esp32` + Qualia board config | 2 |
| `test_rgbmatrix_smoke.py` | `esp32` or `mimxrt` + MatrixPortal board config | 3 |

Run from cmods workspace after build:

```bash
./micropython/ports/rp2/build-RPI_PICO2_W/micropython displayif/tests/test_spi_smoke.py
```
