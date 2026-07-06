# SoC port trees

Each subdirectory under `ports/` holds drivers that **only compile** on a specific SDK or MCU family.

| Directory | SDK / port | MP `PORT` | CP port |
|-----------|------------|-----------|---------|
| `esp_idf/` | ESP-IDF (`esp_lcd`, RGB panel API) | `esp32` | `espressif` |
| `imxrt/` | NXP MCUXpresso / register headers | `mimxrt` | `mimxrt` |

Add a new `ports/<family>/` when:

- the driver needs vendor SDK headers, **and**
- it cannot live in `drivers/` (universal / `machine.SPI` only).

Chip variants (S3 vs P4) stay in one tree with `SOC_*` / `IDF_TARGET` guards — do not fork per chip.
