# esp32 port

ESP-IDF display interfaces for MicroPython `esp32` port / CircuitPython `espressif` port.

## Planned native modules

| C source | Python import | pydisplay backend |
|----------|---------------|-------------------|
| `rgbframebuffer.c` | `rgbframebuffer.RGBFrameBuffer` | **FBDisplay** |
| `rgbmatrix.c` | `rgbmatrix.RGBMatrix` | **FBDisplay** |

`RGBFrameBuffer` mirrors CP `dotclockframebuffer.DotClockFramebuffer`. Supports RGB-666 pin tuples (`red`/`green`/`blue`) and 16-pin RGB565 (`data=`) layouts via constructor — one driver, not separate backends.

Do not use `rgb565` as a module name (color helper collision). No `present()` / panel API.

## Build

`micropython.mk`, `micropython.cmake`, and `circuitpython.mk` in this directory are included when building the esp32 / espressif port.
