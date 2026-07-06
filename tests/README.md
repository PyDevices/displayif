# displayif tests

| Script | Port | Phase |
|--------|------|-------|
| `test_spi_smoke.py` | any SPI-capable | 1 |
| `test_rgbframebuffer_smoke.py` | `esp32` + Qualia or parallel-RGB board config | 2 |
| `test_rgbmatrix_smoke.py` | `esp32` or `mimxrt` + MatrixPortal board config | 3 |

Board configs and `FBDisplay` wiring live in pydisplay; tests import native modules directly.
