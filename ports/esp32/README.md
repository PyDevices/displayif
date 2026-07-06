# esp32 port

ESP-IDF display interfaces for MicroPython `esp32` port / CircuitPython `espressif` port.

## Planned modules

| C source | Python | Interface |
|----------|--------|-----------|
| `rgb_panel.c` | `displayif.rgbpanel.RGBPanel` | Parallel timed RGB LCD |
| `rgb666.c` | `displayif.rgb666.RGBFrameBuffer` | RGB666 dotclock framebuffer |
| `rgbmatrix.c` | `displayif.rgbmatrix.RGBMatrix` | HUB75 (ESP32-S3 MatrixPortal, etc.) |

## Naming

- **`rgbpanel`** — parallel timed RGB LCD. Not `rgb565` (reserved for color packing helpers).
- **`rgb666`** — aligns with CircuitPython RGB666 / `dotclockframebuffer`.

## Build

`micropython.mk`, `micropython.cmake`, and `circuitpython.mk` in this directory are included when building the esp32 / espressif port.
