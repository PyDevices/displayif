# mimxrt port

NXP i.MX RT display interfaces for MicroPython `mimxrt` port.

## Native modules

| Source | Python import | SoC | pydisplay backend |
|--------|---------------|-----|-------------------|
| `src/ports/common/spi/mod_spibus.c` | `spibus.SPIBus` | all mimxrt | **BusDisplay** |
| `src/ports/common/i2c/mod_i2cbus.c` | `i2cbus.I2CBus` | all mimxrt | **BusDisplay** |
| `rgbmatrix_pm.c` + common `rgbmatrix` | `rgbmatrix.RGBMatrix` | MIMXRT1062 (`TEENSY40`, `TEENSY41`, `MIMXRT1060_EVK`) | **FBDisplay** |
| `mod_dotclockframebuffer_elcdif.c` | `dotclockframebuffer.DotClockFramebuffer` | MIMXRT1062 (`MIMXRT1060_EVK`, Teensy 4.x) | **FBDisplay** |
| `mod_mipidsi.c` + `mimxrt1176_dsi_display.c` | `mipidsi.Bus` / `Display` | MIMXRT1176 (`MIMXRT1170_EVK`) | **FBDisplay** |
| `mod_i80bus.c` | `i80bus.I80Bus` | MIMXRT1062 (`TEENSY40`, `TEENSY41`, `MIMXRT1060_EVK`) | bus driver |
| `src/ports/common/notimpl/mod_i80bus.c` | `i80bus.I80Bus` | stub (non-1062) | bus driver (N/A) |
| `src/ports/common/notimpl/mod_dotclockframebuffer.c` | `dotclockframebuffer.DotClockFramebuffer` | stub (non-1062) | **FBDisplay** (N/A) |
| `src/ports/common/notimpl/mod_mipidsi.c` | `mipidsi.Bus` / `Display` | stub (non-1176) | **FBDisplay** (N/A) |

### `dotclockframebuffer.DotClockFramebuffer` (eLCDIF)

On **MIMXRT1062**, `dotclockframebuffer.DotClockFramebuffer` uses the NXP SDK **eLCDIF** block for
dot-clock RGB scanout. Targets the **MIMXRT1060-EVK** with the RK043FN02H-CT
shield (J49). Requires CircuitPython-style `red`/`green`/`blue` (5/6/5) pin
tuples; EVK IOMUX is fixed to LCDIF D0..D15 in B0..B4, G0..G5, R0..R4 order.

Lifecycle: idempotent `deinit` / `__del__` / ctor + soft-reset teardown (stops
eLCDIF; see [IDEMPOTENT_LIFECYCLE.md](../IDEMPOTENT_LIFECYCLE.md)). For the
proven ESP32 continuous-scanout / bounce-buffer / native-blit contract (Qualia),
see [esp32.md](esp32.md) — match those Python-facing
behaviors when extending this backend.

### `mipidsi` (RT1176)

On **MIMXRT1176**, `mipidsi` drives the SoC MIPI DSI host (NXP `mipi_dsi_split`)
with an **LCDIFV2** video bridge to the DPI path. Primary hardware target:
**MIMXRT1170-EVK** + Waveshare 5" DSI on J84 — panel **50H-800480-IPS** with a
**TC358762** DSI-to-RGB bridge. Panel init is supplied via the pydisplay board
config `init_sequence` bytes.

`Display` accepts the same CircuitPython optional kwargs as esp32
(`virtual_channel`, `rotation`, `brightness`, `native_frames_per_second`,
`backlight_on_high`). `virtual_channel` must be `0` on this port (NXP path
does not multiplex VC yet). Panel **reset** and **backlight** GPIO are owned
by board_config (not `Display`); `brightness` / `backlight_on_high` are
accepted for CP signature parity and stored as metadata.

Lifecycle: wires `displayif_mimxrt1176_dsi_*_deinit/stop` into the shared
soft-reset registry. ESP32-P4 `mipidsi` is the soft-reset + blit reference —
see [SOFT_RESET_AND_BRINGUP.md](../SOFT_RESET_AND_BRINGUP.md).

### `i80bus` (FlexIO MCULCD)

On **MIMXRT1062**, `i80bus` uses the NXP SDK **FlexIO MCULCD** driver in Intel **8080** mode with an **8-bit** data bus. Constructor kwargs match CircuitPython `ParallelBus` names (`command`, `chip_select`, `write`, `data_pins`, `frequency`; default 30 MHz) with `.send(command, data=None)` and `.deinit()`.

**Pin constraints (minimal driver):**

- **`data_pins`** and **`write`** must be pads on `GPIO_B0_xx` / `GPIO_B1_xx` with **FLEXIO2** alternate function (ALT4), in **8 consecutive** FlexIO2 indices (e.g. `GPIO_B0_04`–`GPIO_B0_11` = FLEXIO2 D04–D11).
- **`command`** and **`chip_select`** are ordinary **GPIO** outputs via `machine.Pin` / `displayif_pin` helpers (any free GPIO).
- Only **one** `I80Bus` instance per board (single FLEXIO2 peripheral).
- **IOMUX** for FlexIO pins is applied at construction; see pydisplay `teensy41_flexio_ili9341` for an example pin map.

On **MIMXRT1060-EVK**, the LCDIF RGB pins on `GPIO_B0_04`–`GPIO_B0_11` overlap the typical FlexIO2 data mapping — do not use the RK043 RGB shield pins simultaneously with i80bus. Teensy 4.x boards can wire an external 8080 display to FLEXIO2-capable pads per the schematic.

Default `frequency` is 30 MHz (CP ParallelBus default; byte rate on this port). Bulk transfers (≥64 bytes) use `FLEXIO_MCULCD_TransferBlocking` with `dataOnly=true`.

Example pydisplay config: `busdisplay/i80/teensy41_flexio_ili9341`.

### Stubs

Non-1062 mimxrt boards get stub `dotclockframebuffer`; non-1176 boards get stub `mipidsi`. Stub modules import but raise `NotImplementedError` from the constructor.

## pydisplay board configs

| Config | Module | Hardware |
|--------|--------|----------|
| `fbdisplay/mimxrt1060_evk_rk043_rgb` | `dotclockframebuffer` | MIMXRT1060-EVK + RK043 shield (J49) |
| `fbdisplay/mimxrt1170_evk_waveshare_5dsi` | `mipidsi` | MIMXRT1170-EVK + Waveshare 50H-800480-IPS (TC358762 bridge) on J84 |
| `fbdisplay/matrixportal_m4_64x32` | `rgbmatrix` | Metro M4 (SAMD — see samd port) |
| `fbdisplay/rgb_matrix_featherwing_teensy41_64x32` | `rgbmatrix` | Teensy 4.1 + FeatherWing |
| `busdisplay/i80/teensy41_flexio_ili9341` | `i80bus` | Teensy 4.1 + external ILI9341 (FlexIO2) |

CircuitPython board configs for the same hardware live under pydisplay `cp_*` (use CP native modules, not displayif).

On MIMXRT1062 boards, `rgbmatrix` uses the **Protomatter** backend (PIT timer ISR + GPIO set/clear registers). Other mimxrt chips still get `rgbmatrix` with GPIO bitbang `refresh()`; `tile>1` requires Protomatter.

Pin arguments accept `machine.Pin` objects, integers, or port pin-name strings.

## Build

Make port — `USER_C_MODULES` is the workspace parent (sibling layout with `displayif/`):

```bash
cd micropython/ports/mimxrt
make USER_C_MODULES=../../.. BOARD=TEENSY41          # MIMXRT1062 — eLCDIF dotclockframebuffer.DotClockFramebuffer
make USER_C_MODULES=../../.. BOARD=TEENSY40
make USER_C_MODULES=../../.. BOARD=MIMXRT1060_EVK
make USER_C_MODULES=../../.. BOARD=MIMXRT1170_EVK    # MIMXRT1176 — mipidsi
```

## Future work

- Hardware validation: Teensy 4.1 + external 8080 panel on FlexIO2 wiring
