# esp32 port drivers

Targets **ESP32-S3**, **ESP32-P4**, and future Espressif SoCs (MicroPython `esp32` port / CircuitPython `espressif` port).

## Planned modules

| C source | Python | pydisplay board config |
|----------|--------|------------------------|
| `rgb_panel.c` | `displayif.rgbpanel.RGBPanel` | `board_configs/fbdisplay/t-rgb_480` |
| `rgb666.c` | `displayif.rgb666.RGBFrameBuffer` | `board_configs/fbdisplay/qualia_tl040hds20` |

ST7701 register init remains in pydisplay `drivers/display/st7701.py`.

## Naming

- **`rgbpanel`** — parallel timed RGB LCD (16-bit pixels). Not `rgb565` (that name is reserved for color packing helpers).
- **`rgb666`** — dotclock framebuffer; aligns with CircuitPython RGB666 / `dotclockframebuffer`.

## Build

Gated by `mk/esp32.mk` (Make) and `mk/esp32.cmake` (CMake). Only included when building the `esp32` port.

Document minimum ESP-IDF version and `sdkconfig` notes here as drivers land.
