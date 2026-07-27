# esp32 port

ESP-IDF display interfaces for MicroPython `esp32` port / CircuitPython `espressif` port.

## Native modules

| C source | Python import | SoC | pydisplay backend |
|----------|---------------|-----|-------------------|
| `mod_dotclockframebuffer.c` | `dotclockframebuffer.DotClockFramebuffer` | RGB LCD (`SOC_LCD_RGB_SUPPORTED`) | **FBDisplay** |
| `mod_i80bus.c` | `i80bus.I80Bus` | S3 (`SOC_LCD_I80_SUPPORTED`) | bus driver |
| `mod_qspibus.c` | `qspibus.QSPIBus` | S3 (`esp_lcd` SPI `quad_mode`) | bus driver |
| `mod_mipidsi.c` | `mipidsi.Bus` / `mipidsi.Display` | P4 (`SOC_MIPI_DSI_SUPPORTED`) | **FBDisplay** |
| `rgbmatrix_pm.c` + common `rgbmatrix` | `rgbmatrix.RGBMatrix` | S3 (Protomatter + LCD_CAM) | **FBDisplay** |

`I80Bus` kwargs match CircuitPython `ParallelBus`: exactly one of `data0` or
`data_pins`, plus `command`, `chip_select`, `write`, optional `read` / `reset`,
and `frequency` (default 30 MHz). `data0` expands to eight consecutive GPIOs.

On SoCs without the matching peripheral, modules import but constructors raise `NotImplementedError`.

Pin arguments accept `machine.Pin` objects, integers, or port pin-name strings (via `displayif_pin_resolve`).

### `qspibus.QSPIBus` (ESP32-S3)

Keyword-only constructor matches CircuitPython `qspibus.QSPIBus`:

```python
qspibus.QSPIBus(
    *,
    clock, data0, data1, data2, data3, cs,
    dcx=None, reset=None, frequency=80_000_000,
)
```

Uses ESP-IDF `spi_bus_initialize` + `esp_lcd_new_panel_io_spi` with `quad_mode`, dual DMA bounce buffers, and encoded QSPI command words (`0x02` / `0x32`). Methods: `send(command, data)`, `write_command`, `write_data`, `reset`, `deinit` / `__del__`. Soft-reset tears down panel IO, SPI host, DMA buffers, and the transfer semaphore before GC.

### `dotclockframebuffer.DotClockFramebuffer` (RGB LCD / Qualia)

Python import is `dotclockframebuffer.DotClockFramebuffer` (same as CircuitPython).
Required RGB565 pin tuples (`red`/`green`/`blue` = 5/6/5), matching CircuitPython
(wire order B0..B4, G0..G5, R0..R4).

Behavioral contract (proven on Qualia S3 + TL040HDS20):

| Topic | Behavior |
|-------|----------|
| Scanout | Continuous DMA (`refresh_on_demand=0`); panel-owned FB |
| `refresh()` | Cache writeback only (`esp_cache_msync`) |
| Large panels | Bounce buffer `20 * h_res` px + dirty-row msync |
| Buffer protocol | typecode `'B'` (MP has no `memoryview.cast`) |
| Fast paths | Native `blit` / `fill_rect` (expose via custom `attr`) |
| Lifecycle | Idempotent `deinit` / `__del__` / ctor + soft-reset teardown |

See [SOFT_RESET_AND_BRINGUP.md](../SOFT_RESET_AND_BRINGUP.md#reference-dotclockframebufferdotclockframebuffer-on-qualia-esp32-s3).

### `mipidsi` (ESP32-P4 DSI)

`mipidsi.Bus` + `mipidsi.Display` for `SOC_MIPI_DSI_SUPPORTED`. Proven on
Waveshare / Espressif P4 4B touch LCD with LVGL soft-reset re-import.

`Bus(frequency=500_000_000, num_lanes=2, …)` matches CircuitPython defaults
(`ldo_*` remain displayif extras).

`Display(bus, init_sequence, *, …)` takes positional `bus` and `init_sequence`
like CircuitPython. Optional kwargs: `virtual_channel=0` (wired into esp_lcd
DBI/DPI), `rotation=0` (90° steps; metadata), `brightness=1.0`,
`native_frames_per_second=60`, `backlight_pin=None`, `backlight_on_high=True`.
`color_depth` defaults to 16. Panel **reset** and **backlight** GPIO are owned
by board_config; `backlight_pin` / `brightness` / `backlight_on_high` are
accepted for CP signature parity and stored as metadata (not driven here).

| Topic | Behavior |
|-------|----------|
| `refresh()` | Full-FB `esp_cache_msync` + `esp_lcd_panel_draw_bitmap` |
| Fast path | Native `Display.blit` + dirty-row msync (Python row slices WDT) |
| Buffer protocol | typecode `'B'` |
| Lifecycle | BSS host mirror; `esp_lcd_del_*` + LDO + SPIRAM free on soft-reset |
| Methods | Custom `attr` exports `refresh` / `blit` / `deinit` / `__del__` |

See [SOFT_RESET_AND_BRINGUP.md](../SOFT_RESET_AND_BRINGUP.md) and
[IDEMPOTENT_LIFECYCLE.md](../IDEMPOTENT_LIFECYCLE.md).

Large framebuffers use **PSRAM** when available — see [PORT_MATRIX.md](../PORT_MATRIX.md#esp32-psram--sdkconfig-large-framebuffers).

## Build

`micropython.mk` and `micropython.cmake` under `src/ports/esp32/` are included when building the esp32 port.
