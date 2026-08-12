# rp2 port

Raspberry Pi RP2040/RP2350 display interfaces for MicroPython `rp2` port.

## Native modules

| Source | Python import | SoC | `displaydev` backend |
|--------|---------------|-----|-------------------|
| `src/ports/common/spi/mod_spibus.c` | `spibus.SPIBus` | RP2040 / RP2350 | **BusDisplay** |
| `src/ports/common/i2c/mod_i2cbus.c` | `i2cbus.I2CBus` | RP2040 / RP2350 | **BusDisplay** |
| `mod_i80bus.c` | `i80bus.I80Bus` | RP2040 / RP2350 | bus driver |
| `rgbmatrix_pm.c` + common `rgbmatrix` | `rgbmatrix.RGBMatrix` | RP2040 / RP2350 | **FBDisplay** |
| `mod_picodvi.c` + `picodvi_rp2040.c` | `picodvi.Framebuffer` | RP2040 | **FBDisplay** |
| `mod_picodvi.c` + `picodvi_rp2350.c` | `picodvi.Framebuffer` | RP2350 | **FBDisplay** |
| `src/ports/common/notimpl/mod_dotclockframebuffer.c` | `dotclockframebuffer.DotClockFramebuffer` | stub | **FBDisplay** (N/A) |
| `src/ports/common/notimpl/mod_mipidsi.c` | `mipidsi.Bus` / `Display` | stub | **FBDisplay** (N/A) |

`SPIBus` uses CircuitPython `FourWire` names for display-control pins
(`command`, `chip_select`, `reset`) and keeps MicroPython SPI extras
(`id`, `sck`/`mosi`/`miso`, `bits`, `lsb_first`, `soft`). It builds its own
`machine.SPI` / `SoftSPI` rather than taking a prebuilt `spi_bus` object.

### `i2cbus`

`I2CBus(i2c_bus, *, device_address, reset=None)` — CircuitPython
`I2CDisplayBus` kwargs (positional bus, required `device_address`, optional
`reset` with `.reset()`).

### `picodvi`

**Implemented** for both RP2040 and RP2350:

- **RP2040** — vendored [PicoDVI libdvi](../../src/ports/rp2/picodvi/libdvi/) (PIO TMDS serialiser, core1 scanline callback).
- **RP2350** — HSTX hardware serialiser backend (`picodvi_rp2350.c`).

`picodvi.Framebuffer` exposes buffer protocol + `refresh()` for **FBDisplay**.

### `i80bus`

Uses a **PIO state machine** (8 consecutive data pins, WR on side-set) with **DMA** for bulk `send()` transfers. Constructor kwargs match CircuitPython `ParallelBus`: exactly one of `data0` or `data_pins`, plus `command`, `chip_select`, `write`, optional `read` / `reset`, and `frequency` (default 30 MHz). `frequency` is wired into the PIO clock divider. Optional `read` is accepted for CP signature parity but unused on this port (write-only path); **mimxrt** wires `read` into FlexIO `RDPinIndex`.

### `rgbmatrix`

Uses the **Protomatter** backend (PWM slice 7 wrap IRQ + GPIO set/clr). Pin arguments accept `machine.Pin` objects, integers, or board pin names.

Stub modules (`dotclockframebuffer`, `mipidsi`) import on all boards but raise `NotImplementedError` from the constructor. **RP2350 has no MIPI DSI host** — use `picodvi` (HSTX/DVI) or SPI/I80 buses instead. CircuitPython `mipidsi` is not available on Pico 2; it targets SoCs like ESP32-P4.

## micropython-hardware board configs

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
