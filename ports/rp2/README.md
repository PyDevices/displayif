# rp2 port

Raspberry Pi RP2040/RP2350 display interfaces for MicroPython `rp2` port.

## Native modules

| Source | Python import | SoC | pydisplay backend |
|--------|---------------|-----|-------------------|
| `ports/common/spi/mod_spibus.c` | `spibus.SPIBus` | RP2040 / RP2350 | **BusDisplay** |
| `ports/common/i2c/mod_i2cbus.c` | `i2cbus.I2CBus` | RP2040 / RP2350 | **BusDisplay** |
| `mod_i80bus.c` | `i80bus.I80Bus` | RP2040 / RP2350 | bus driver |
| `rgbmatrix_pm.c` + common `rgbmatrix` | `rgbmatrix.RGBMatrix` | RP2040 / RP2350 | **FBDisplay** |
| `ports/common/notimpl/mod_rgbframebuffer.c` | `rgbframebuffer.RGBFrameBuffer` | stub | **FBDisplay** (N/A) |
| `ports/common/notimpl/mod_mipidsi.c` | `mipidsi.Bus` / `Display` | stub | **FBDisplay** (N/A) |
| `ports/common/notimpl/mod_picodvi.c` | `picodvi.Framebuffer` | stub | **FBDisplay** (PIO/HSTX TBD) |

`i80bus` uses a **PIO state machine** (8 consecutive data pins, WR on side-set) with **DMA** for bulk `send()` transfers. Matches the pydisplay `I80Bus(dc, cs, wr, data, freq)` contract.

`rgbmatrix` uses the **Protomatter** backend (PWM slice 7 wrap IRQ + GPIO set/clr). Pin arguments accept `machine.Pin` objects, integers, or board pin names.

Stub modules (`rgbframebuffer`, `mipidsi`, `picodvi`) import on all boards but raise `NotImplementedError` from the constructor — same pattern as esp32 on unsupported SoCs.

## pydisplay board configs

| Config | Module |
|--------|--------|
| `busdisplay/i80/ili9341_i80_rp2040` | `i80bus` |
| `fbdisplay/pimoroni_pico_dv_base_640x480` | `picodvi` (stub) |
| `fbdisplay/pico2_dvi_sock_640x480` | `picodvi` (stub) |
| `fbdisplay/feather_rp2040_rgb_matrix_64x32` | `rgbmatrix` |
| `fbdisplay/*` (matrix) | `rgbmatrix` |

## Build

```bash
./build_mp.sh --port rp2 --board RPI_PICO
./build_mp.sh --port rp2 --board RPI_PICO2_W
```

`build_mp.sh` fetches a prebuilt `picotool` from [pico-sdk-tools](https://github.com/raspberrypi/pico-sdk-tools/releases/tag/v2.1.1-0) when needed (CircuitPython avoids picotool entirely and uses `uf2conv.py`; MicroPython's cmake-based rp2 port still needs picotool for UF2 output). Override with `picotool_DIR` or `PICOTOOL_FETCH_FROM_GIT_PATH` if you already have one installed.
