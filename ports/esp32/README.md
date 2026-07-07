# esp32 port

ESP-IDF display interfaces for MicroPython `esp32` port / CircuitPython `espressif` port.

## Native modules

| C source | Python import | SoC | pydisplay backend |
|----------|---------------|-----|-------------------|
| `mod_rgbframebuffer.c` | `rgbframebuffer.RGBFrameBuffer` | RGB LCD (`SOC_LCD_RGB_SUPPORTED`) | **FBDisplay** |
| `mod_i80bus.c` | `i80bus.I80Bus` | S3 (`SOC_LCD_I80_SUPPORTED`) | bus driver |
| `mod_mipidsi.c` | `mipidsi.Bus` / `mipidsi.Display` | P4 (`SOC_MIPI_DSI_SUPPORTED`) | **FBDisplay** |
| `rgbmatrix_pm.c` + common `rgbmatrix` | `rgbmatrix.RGBMatrix` | S3 (Protomatter + LCD_CAM) | **FBDisplay** |

On SoCs without the matching peripheral, modules import but constructors raise `NotImplementedError`.

Pin arguments accept `machine.Pin` objects, integers, or port pin-name strings (via `displayif_pin_resolve`).

`RGBFrameBuffer` mirrors CP `dotclockframebuffer.DotClockFramebuffer`. Supports RGB-666 pin tuples (`red`/`green`/`blue`) and 16-pin RGB565 (`data=`) layouts via constructor — one driver, not separate backends.

Do not use `rgb565` as a module name (color helper collision). No `present()` / panel API.

## Build

`micropython.mk`, `micropython.cmake`, and `circuitpython.mk` in this directory are included when building the esp32 / espressif port.
