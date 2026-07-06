# ports/

All interface code lives under `ports/`, named after **MicroPython port** directories (`esp32`, `mimxrt`, `rp2`, …) or `common` for portable code.

| Directory | When included | MP port | CP port dir |
|-----------|---------------|---------|-------------|
| `common/` | always | all | all (when patched) |
| `esp32/` | esp32 builds | `esp32` | `espressif` |
| `mimxrt/` | mimxrt builds | `mimxrt` | `mimxrt` |

## Rules

1. **`ports/common/`** — uses only `machine.SPI`, `machine.Pin`, or equivalent portable APIs. No ESP-IDF or NXP SDK headers.
2. **`ports/<port>/`** — SoC SDK code (`esp_lcd`, `fsl_elcdif`, …). One tree per MP port name.
3. Chip variants (S3 vs P4) use `SOC_*` / `IDF_TARGET` guards inside `ports/esp32/`, not separate folders.
4. New portable interface → `ports/common/<name>/` + line in `mk/common.mk`.
5. New port-specific interface → `ports/<port>/` + fragment in `mk/<port>.mk`.

## CP vs MP port names

Folder names always match **MicroPython** `ports/<name>/`. CircuitPython may use a different port directory (e.g. `espressif`); `mk/detect_cp.mk` maps that to `ports/esp32/` sources.
