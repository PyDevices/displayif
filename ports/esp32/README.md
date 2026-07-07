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

`RGBFrameBuffer` mirrors CircuitPython `dotclockframebuffer.DotClockFramebuffer`. Supports RGB-666 pin tuples (`red`/`green`/`blue`) and 16-pin RGB565 (`data=`) layouts.

Large framebuffers use **PSRAM** when available — see [HANDOFF.md](../../HANDOFF.md#esp32-psram--sdkconfig-large-framebuffers).

## Build

`micropython.mk` and `micropython.cmake` in this directory are included when building the esp32 port.
