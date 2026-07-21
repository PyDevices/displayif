# esp32 port

ESP-IDF display interfaces for MicroPython `esp32` port / CircuitPython `espressif` port.

## Native modules

| C source | Python import | SoC | pydisplay backend |
|----------|---------------|-----|-------------------|
| `mod_dotclockframebuffer.c` | `displayif.DotClockFramebuffer` | RGB LCD (`SOC_LCD_RGB_SUPPORTED`) | **FBDisplay** |
| `mod_i80bus.c` | `i80bus.I80Bus` | S3 (`SOC_LCD_I80_SUPPORTED`) | bus driver |
| `mod_mipidsi.c` | `mipidsi.Bus` / `mipidsi.Display` | P4 (`SOC_MIPI_DSI_SUPPORTED`) | **FBDisplay** |
| `rgbmatrix_pm.c` + common `rgbmatrix` | `rgbmatrix.RGBMatrix` | S3 (Protomatter + LCD_CAM) | **FBDisplay** |

On SoCs without the matching peripheral, modules import but constructors raise `NotImplementedError`.

Pin arguments accept `machine.Pin` objects, integers, or port pin-name strings (via `displayif_pin_resolve`).

### `displayif.DotClockFramebuffer` (RGB LCD / Qualia)

Python import is `displayif.DotClockFramebuffer` (CP uses `dotclockframebuffer.DotClockFramebuffer`). Supports
RGB-666 pin tuples (`red`/`green`/`blue`) and 16-pin RGB565 (`data=`) layouts.

Behavioral contract (proven on Qualia S3 + TL040HDS20):

| Topic | Behavior |
|-------|----------|
| Scanout | Continuous DMA (`refresh_on_demand=0`); panel-owned FB |
| `refresh()` | Cache writeback only (`esp_cache_msync`) |
| Large panels | Bounce buffer `20 * h_res` px + dirty-row msync |
| Buffer protocol | typecode `'B'` (MP has no `memoryview.cast`) |
| Fast paths | Native `blit` / `fill_rect` (expose via custom `attr`) |
| Lifecycle | Idempotent `deinit` / `__del__` / ctor + soft-reset teardown |

See [SOFT_RESET_AND_BRINGUP.md](../../SOFT_RESET_AND_BRINGUP.md#reference-displayifdotclockframebuffer-on-qualia-esp32-s3).

### `mipidsi` (ESP32-P4 DSI)

`mipidsi.Bus` + `mipidsi.Display` for `SOC_MIPI_DSI_SUPPORTED`. Proven on
Waveshare / Espressif P4 4B touch LCD with LVGL soft-reset re-import.

| Topic | Behavior |
|-------|----------|
| `refresh()` | Full-FB `esp_cache_msync` + `esp_lcd_panel_draw_bitmap` |
| Fast path | Native `Display.blit` + dirty-row msync (Python row slices WDT) |
| Buffer protocol | typecode `'B'` |
| Lifecycle | BSS host mirror; `esp_lcd_del_*` + LDO + SPIRAM free on soft-reset |
| Methods | Custom `attr` exports `refresh` / `blit` / `deinit` / `__del__` |

See [SOFT_RESET_AND_BRINGUP.md](../../SOFT_RESET_AND_BRINGUP.md) and
[IDEMPOTENT_LIFECYCLE.md](../../IDEMPOTENT_LIFECYCLE.md).

Large framebuffers use **PSRAM** when available — see [HANDOFF.md](../../HANDOFF.md#esp32-psram--sdkconfig-large-framebuffers).

## Build

`micropython.mk` and `micropython.cmake` in this directory are included when building the esp32 port.
