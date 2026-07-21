# rp2 port

Raspberry Pi RP2040/RP2350 display interfaces for MicroPython `rp2` port.

## Native modules

| Source | Python import | SoC | pydisplay backend |
|--------|---------------|-----|-------------------|
| `ports/common/spi/mod_spibus.c` | `spibus.SPIBus` | RP2040 / RP2350 | **BusDisplay** |
| `ports/common/i2c/mod_i2cbus.c` | `i2cbus.I2CBus` | RP2040 / RP2350 | **BusDisplay** |
| `mod_i80bus.c` | `i80bus.I80Bus` | RP2040 / RP2350 | bus driver |
| `rgbmatrix_pm.c` + common `rgbmatrix` | `rgbmatrix.RGBMatrix` | RP2040 / RP2350 | **FBDisplay** |
| `mod_picodvi.c` + `picodvi_rp2040.c` | `picodvi.Framebuffer` | RP2040 | **FBDisplay** |
| `mod_picodvi.c` + `picodvi_rp2350.c` | `picodvi.Framebuffer` | RP2350 | **FBDisplay** |
| `ports/common/notimpl/mod_dotclockframebuffer.c` | `displayif.DotClockFramebuffer` | stub | **FBDisplay** (N/A) |
| `ports/common/notimpl/mod_mipidsi.c` | `mipidsi.Bus` / `Display` | stub | **FBDisplay** (N/A) |

### `picodvi`

**Implemented** for both RP2040 and RP2350:

- **RP2040** — vendored [PicoDVI libdvi](picodvi/libdvi/) (PIO TMDS serialiser, core1 scanline callback).
- **RP2350** — HSTX hardware serialiser backend (`picodvi_rp2350.c`).

`picodvi.Framebuffer` exposes buffer protocol + `refresh()` for **FBDisplay**.

### `i80bus`

Uses a **PIO state machine** (8 consecutive data pins, WR on side-set) with **DMA** for bulk `send()` transfers. Matches the pydisplay `I80Bus(dc, cs, wr, data, freq)` contract.

### `rgbmatrix`

Uses the **Protomatter** backend (PWM slice 7 wrap IRQ + GPIO set/clr). Pin arguments accept `machine.Pin` objects, integers, or board pin names.

Stub modules (`displayif`, `mipidsi`) import on all boards but raise `NotImplementedError` from the constructor. **RP2350 has no MIPI DSI host** — use `picodvi` (HSTX/DVI) or SPI/I80 buses instead. CircuitPython `mipidsi` is not available on Pico 2; it targets SoCs like ESP32-P4.

## pydisplay board configs

| Config | Module |
|--------|--------|
| `busdisplay/i80/ili9341_i80_rp2040` | `i80bus` |
| `fbdisplay/pimoroni_pico_dv_base_640x480` | `picodvi` |
| `fbdisplay/pico2_dvi_sock_640x480` | `picodvi` |
| `fbdisplay/feather_rp2040_rgb_matrix_64x32` | `rgbmatrix` |
| `fbdisplay/*` (matrix) | `rgbmatrix` |

## Build

CMake port — point `USER_C_MODULES` at the `displayif` repo (sibling of `micropython/`):

```bash
cd micropython/ports/rp2
make BOARD=RPI_PICO USER_C_MODULES=../../../displayif       # RP2040 — picodvi (PIO/libdvi)
make BOARD=RPI_PICO2_W USER_C_MODULES=../../../displayif    # RP2350 — picodvi (HSTX)
```

MicroPython's rp2 port needs `picotool` for UF2 output. Fetch a prebuilt binary from [pico-sdk-tools](https://github.com/raspberrypi/pico-sdk-tools/releases/tag/v2.1.1-0) if needed, or override with `picotool_DIR` / `PICOTOOL_FETCH_FROM_GIT_PATH`.
