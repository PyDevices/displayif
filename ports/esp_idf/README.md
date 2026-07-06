# ESP-IDF display drivers

Targets: **ESP32-S3**, **ESP32-P4**, and future Espressif SoCs with RGB LCD peripherals.

## Planned modules

| C source | Python | pydisplay board config |
|----------|--------|------------------------|
| `rgb565_panel.c` | `displayif.rgb565.RGB565Panel` | `board_configs/fbdisplay/t-rgb_480` |
| `rgb_framebuffer.c` | `displayif.rgbframebuffer.RGBFrameBuffer` | `board_configs/fbdisplay/qualia_tl040hds20` |

ST7701 register init remains in pydisplay `drivers/display/st7701.py`.

## Build

Gated by `mk/esp_idf.mk` (Make) and `mk/esp_idf.cmake` (CMake). Only included when `PORT` is `esp32`.

## IDF notes

Document minimum ESP-IDF version and any `sdkconfig` requirements here as drivers land.
