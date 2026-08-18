# displayif — port matrix

Native display **interface** modules for PyDevices `displaydev`. Portable code in `src/ports/common/`; SoC-specific code under `src/ports/<mp-port>/` (names from `micropython/ports/`).

**Start here for agents:** [AGENTS.md](../AGENTS.md) → [idempotent-lifecycle.md](idempotent-lifecycle.md) → [soft-reset-and-bring-up.md](soft-reset-and-bring-up.md) (soft-reset wraps, symptom table, bring-up methods).

Several `pydevices` MicroPython `board_config.py` files raise `NotImplementedError` until the matching displayif module exists — **that is what this repo builds**.

**Workspace:** clone as a sibling of `micropython/`.

**No Python re-export layer in this repo.** Native C modules register directly.
Parallel RGB on MicroPython is **`import dotclockframebuffer` → `dotclockframebuffer.DotClockFramebuffer`**,
matching CircuitPython’s module name. Other interfaces keep their own module
names (`mipidsi`, `i80bus`, `picodvi`, …).

**CircuitPython:** displayif does **not** ship CP bindings. CircuitPython already provides `dotclockframebuffer`, `mipidsi`, `paralleldisplaybus`, `picodvi`, etc. — use those for CP board configs under `pydevices/board_configs/cp/`.

---

## Mission

| Location | Native module | `displaydev` backend | Status |
|----------|---------------|-------------------|--------|
| `src/ports/common` | `spibus` / `i2cbus` | **BusDisplay** | shipped |
| `src/ports/esp32` | `dotclockframebuffer` | **FBDisplay** | esp_lcd RGB scanout |
| `src/ports/esp32` | `i80bus` | bus driver | esp_lcd I80 (S3) |
| `src/ports/esp32` | `qspibus` | bus driver | esp_lcd SPI quad_mode (S3) |
| `src/ports/esp32` | `mipidsi` | **FBDisplay** | ESP32-P4 MIPI DSI |
| `src/ports/esp32` / `mimxrt` / `samd` / `rp2` | `rgbmatrix` | **FBDisplay** | Protomatter backends |
| `src/ports/rp2` | `i80bus` | bus driver | PIO+DMA |
| `src/ports/rp2` | `picodvi` | **FBDisplay** | RP2040 PIO / RP2350 HSTX |
| `src/ports/mimxrt` | `dotclockframebuffer` | **FBDisplay** | eLCDIF on MIMXRT1062 |
| `src/ports/mimxrt` | `mipidsi` | **FBDisplay** | MIPI DSI on MIMXRT1176 |
| `src/ports/mimxrt` | `i80bus` | bus driver | FlexIO MCULCD 8080 on MIMXRT1062 |
| `src/ports/samd` | `i80bus` | bus driver | GPIO bit-bang (SAMD51) via `common/i80bus` |
| `src/ports/samd` | stubs | `dotclockframebuffer`, `mipidsi` | import ok; ctor raises |

All parallel dot-clock RGB panels use **`dotclockframebuffer.DotClockFramebuffer`** + **`FBDisplay`**.
There is no separate `RGBDisplay` / `present()` path. CircuitPython board configs use the same import name.

---

## Per-port native module notes

Behavioral/constructor detail beyond the Mission table above. `esp32` has its own page —
[docs/ports/esp32.md](ports/esp32.md).

### mimxrt

`dotclockframebuffer.DotClockFramebuffer` (**MIMXRT1062**, eLCDIF): dot-clock RGB scanout.
Targets the **MIMXRT1060-EVK** with the RK043FN02H-CT shield (J49). Requires CircuitPython-style
`red`/`green`/`blue` (5/6/5) pin tuples; EVK IOMUX is fixed to LCDIF D0..D15 in B0..B4, G0..G5,
R0..R4 order. Lifecycle: idempotent `deinit` / `__del__` / ctor + soft-reset teardown (stops
eLCDIF; see [idempotent-lifecycle.md](idempotent-lifecycle.md)). For the proven ESP32
continuous-scanout / bounce-buffer / native-blit contract (Qualia), see
[ports/esp32.md](ports/esp32.md) — match those Python-facing behaviors when extending this
backend. CP timing/polarity kwargs `overscan_left` and `pclk_idle_high` are accepted but are
no-ops on this port (both are wired on esp32's esp_lcd RGB path).

`mipidsi` (**MIMXRT1176**): drives the SoC MIPI DSI host (NXP `mipi_dsi_split`) with an
**LCDIFV2** video bridge to the DPI path. Primary hardware target: **MIMXRT1170-EVK** + Waveshare
5" DSI on J84 — panel **50H-800480-IPS** with a **TC358762** DSI-to-RGB bridge. Panel init is
supplied via the pydevices board config `init_sequence` bytes. `Display` optional kwargs match
esp32 displayif: `virtual_channel=0` (must be `0` — NXP path does not multiplex VC yet) and
`color_depth=16`. Panel **reset** and **backlight** GPIO are owned by board_config, not `Display`.
Lifecycle: wires `displayif_mimxrt1176_dsi_*_deinit/stop` into the shared soft-reset registry.
ESP32-P4 `mipidsi` is the soft-reset + blit reference — see
[soft-reset-and-bring-up.md](soft-reset-and-bring-up.md).

`i80bus` (**MIMXRT1062**, FlexIO MCULCD): Intel 8080 mode, 8-bit data bus. Constructor kwargs
match CircuitPython `ParallelBus` (exactly one of `data0` / `data_pins`, plus `command`,
`chip_select`, `write`, optional `read` / `reset`, `frequency` default 30 MHz). Pin constraints:
`data0`/`data_pins` and `write` must be pads on `GPIO_B0_xx` / `GPIO_B1_xx` with FLEXIO2
alternate function (ALT4), in 8 consecutive FlexIO2 indices; `command`, `chip_select`, `reset`
are ordinary GPIO outputs. Only one `I80Bus` instance per board (single FLEXIO2 peripheral). On
**MIMXRT1060-EVK**, the LCDIF RGB pins on `GPIO_B0_04`–`GPIO_B0_11` overlap the typical FlexIO2
data mapping — do not use the RK043 RGB shield pins simultaneously with i80bus. `frequency` is
wired here (as on esp32/rp2 — ignored on samd). Bulk transfers (≥64 bytes) use
`FLEXIO_MCULCD_TransferBlocking` with `dataOnly=true`. Example pydevices config:
`busdisplay/i80/teensy41_flexio_ili9341`.

Non-1062 mimxrt boards get a stub `dotclockframebuffer`; non-1176 boards get a stub `mipidsi`.
Stub modules import but raise `NotImplementedError` from the constructor.

**Build** (make port, `USER_C_MODULES` = workspace parent):

```bash
cd micropython/ports/mimxrt
make USER_C_MODULES=../../.. BOARD=TEENSY41          # MIMXRT1062 — eLCDIF dotclockframebuffer
make USER_C_MODULES=../../.. BOARD=MIMXRT1170_EVK    # MIMXRT1176 — mipidsi
```

**Future work:** hardware validation of Teensy 4.1 + external 8080 panel on FlexIO2 wiring.

### rp2

`spibus.SPIBus` builds its own `machine.SPI` / `SoftSPI` rather than taking a prebuilt `spi_bus`
object, and keeps MicroPython SPI extras (`id`, `sck`/`mosi`/`miso`, `bits`, `lsb_first`, `soft`)
alongside CircuitPython `FourWire` pin names (`command`, `chip_select`, `reset`).

`picodvi.Framebuffer` is **implemented** for both chips: **RP2040** uses the vendored
[PicoDVI libdvi](https://github.com/Wren6991/PicoDVI) (BSD-3-Clause; PIO TMDS serialiser, core1
scanline callback), **RP2350** uses the HSTX hardware serialiser (`picodvi_rp2350.c`). Exposes
buffer protocol + `refresh()` for **FBDisplay**.

`i80bus` uses a **PIO state machine** (8 consecutive data pins, WR on side-set) with **DMA** for
bulk `send()` transfers. Kwargs match CircuitPython `ParallelBus`; `frequency` (default 30 MHz)
is wired into the PIO clock divider. Optional `read` is accepted for CP signature parity but
unused (write-only path); **mimxrt** wires `read` into FlexIO `RDPinIndex`.

`rgbmatrix` uses the **Protomatter** backend (PWM slice 7 wrap IRQ + GPIO set/clr).

Stub modules (`dotclockframebuffer`, `mipidsi`) import on all boards but raise
`NotImplementedError` from the constructor — **RP2350 has no MIPI DSI host** (see
[RP2350 / MIPI DSI](#rp2350--mipi-dsi) above); use `picodvi` or SPI/I80 buses instead.

**Build** (CMake port, `USER_C_MODULES` points at this repo):

```bash
cd micropython/ports/rp2
make BOARD=RPI_PICO USER_C_MODULES=../../../displayif       # RP2040 — picodvi (PIO/libdvi)
make BOARD=RPI_PICO2_W USER_C_MODULES=../../../displayif    # RP2350 — picodvi (HSTX)
```

MicroPython's rp2 port needs `picotool` for UF2 output — fetch a prebuilt binary from
[pico-sdk-tools](https://github.com/raspberrypi/pico-sdk-tools/releases/tag/v2.1.1-0) if needed,
or override with `picotool_DIR` / `PICOTOOL_FETCH_FROM_GIT_PATH`.

### samd

`i80bus` (**SAMD51**, GPIO bit-bang): uses PORT `OUTSET`/`OUTCLR` register writes via the shared
`src/ports/common/i80bus/gpio_bitbang.c` backend (same algorithm as pydevices's viper `I80Bus`).
Supports 8 data pins via `data0` (eight consecutive) or `data_pins` (sequential or LUT layout).
No dedicated 8080 peripheral — throughput is lower than esp32-S3, rp2 PIO, or mimxrt FlexIO.
`frequency` is accepted for CP signature parity but unused (timing is not cycle-counted yet);
esp32/rp2/mimxrt wire `frequency` into the host. `read` is likewise accepted but unused
(write-only path) — same as esp32/rp2. SAMD21 builds keep the import-only stub.

`rgbmatrix` (SAMD51) uses the **Protomatter** backend (TC3 overflow ISR + PORT OUTSET/OUTCLR);
`samd_irq_hook.c` patches the TC3 vector before scanout starts.

**Build** (make port, `USER_C_MODULES` = workspace parent):

```bash
cd micropython/ports/samd
make USER_C_MODULES=../../.. BOARD=ADAFRUIT_METRO_M4_EXPRESS
make USER_C_MODULES=../../.. BOARD=ADAFRUIT_FEATHER_M4_EXPRESS
```

---

## pydevices board configs (MicroPython)

These configs import displayif modules when firmware is built with the matching cmod:

| MicroPython board config | Native module |
|---------------------------|---------------|
| `fbdisplay/matrixportal_s3_64x64` | `rgbmatrix` |
| `fbdisplay/matrixportal_m4_64x32` | `rgbmatrix` |
| `fbdisplay/rgb_matrix_featherwing_64x32` | `rgbmatrix` |
| `fbdisplay/qualia_tl040hds20` | `dotclockframebuffer` |
| `fbdisplay/mimxrt1060_evk_rk043_rgb` | `dotclockframebuffer` |
| `fbdisplay/mimxrt1170_evk_waveshare_5dsi` | `mipidsi` |
| `fbdisplay/esp32-p4-wifi6-touch-lcd-4b` | `mipidsi` |
| `fbdisplay/pico2_dvi_sock_640x480` | `picodvi` |
| `fbdisplay/t-rgb_480` | `dotclockframebuffer` |
| parallel RGB `fbdisplay/*` (MP) | `dotclockframebuffer` |

CP siblings live under `board_configs/cp/fbdisplay/` and use CircuitPython native modules — not displayif.

---

## RP2350 / MIPI DSI

**RP2350 has no MIPI DSI host.** The chip provides **HSTX** (high-speed serial transmit) for DVI/TMDS — used by displayif `picodvi` and CircuitPython `picodvi`. CircuitPython’s `mipidsi` module is enabled only on SoCs with a DSI PHY (e.g. **ESP32-P4**, M5Stack Tab5) — not on Raspberry Pi Pico 2 / RP2350 boards.

TFT_eSPI and similar Arduino stacks on RP2040/RP2350 use **SPI** or **8-bit parallel (I80)** bit-bang/PIO, not MIPI DSI. For DSI panels you need a bridge chip (e.g. TC358762 on MIMXRT1170) or a SoC with native DSI (ESP32-P4).

---

## ESP32 PSRAM / sdkconfig (large framebuffers)

`dotclockframebuffer.DotClockFramebuffer` and `mipidsi` allocate framebuffers with `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` and set `fb_in_psram = 1` for esp_lcd RGB panels. If PSRAM is disabled or too small in `sdkconfig`, allocation falls back to internal RAM (may fail for 720×720+ panels).

**Before building esp32 displayif firmware**, verify in `menuconfig` / board `sdkconfig`:

- `CONFIG_SPIRAM` / `CONFIG_ESP32S3_SPIRAM_SUPPORT` enabled
- PSRAM size matches your module (e.g. 8 MB OPI on S3)
- `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` threshold — large display buffers should land in PSRAM

If panels stay black or init fails with OOM, increase PSRAM allocation or reduce resolution. displayif logs a warning when RGB framebuffer SPIRAM allocation falls back to internal heap.

---

## Filesystem layout

```
displayif/
├── micropython.mk / micropython.cmake
├── src/
│   ├── include/displayif/
│   └── ports/
│       ├── common/      # spi/, i2c/, rgbmatrix/, i80bus/gpio_bitbang.c
│       ├── esp32/       # dotclockframebuffer, i80bus, qspibus, mipidsi, rgbmatrix (S3)
│       ├── mimxrt/      # rgbmatrix; eLCDIF dotclockframebuffer; RT1176 mipidsi; FlexIO i80bus
│       ├── samd/        # rgbmatrix; GPIO i80bus (SAMD51); stubs
│       └── rp2/         # rgbmatrix; i80bus PIO+DMA; picodvi
├── docs/                # markdown (port notes under docs/ports/)
├── tests/               # unit tests only
└── tools/               # developer / smoke tests
```

Each `src/ports/<name>/` directory has `micropython.mk` and `micropython.cmake`. Root makefiles detect the active port and include `src/ports/common/` plus the matching port tree.

---

## Build

From a sibling `micropython/` checkout:

**Make ports** — `USER_C_MODULES` = workspace parent (contains `displayif/`):

```bash
cd micropython/ports/mimxrt && make USER_C_MODULES=../../.. BOARD=TEENSY41
cd micropython/ports/samd && make USER_C_MODULES=../../.. BOARD=ADAFRUIT_METRO_M4_EXPRESS
```

**CMake ports** — `USER_C_MODULES` = this repo (or a `;`-separated list of module paths):

```bash
cd micropython/ports/rp2 && make BOARD=RPI_PICO2_W USER_C_MODULES=../../../displayif
cd micropython/ports/esp32 && make BOARD=ESP32_GENERIC_S3 USER_C_MODULES=../../../displayif
```

See the [cmods workspace](https://github.com/PyDevices/cmods) for an easier way to build this repo with other user C modules.

No `manifest.py` frozen package required unless we later add pure-Python helpers (not planned).

---

## Suggested work sequence

1. Scaffold — done
2. pydevices board configs on `dotclockframebuffer.DotClockFramebuffer` + `FBDisplay` — done
3. `spibus` + smoke tests — done
4. esp32 `dotclockframebuffer`, `i80bus`, `mipidsi` — done
5. mimxrt eLCDIF `dotclockframebuffer`, RT1176 `mipidsi`, FlexIO `i80bus` — done
6. rp2 `picodvi`, PIO `i80bus` — done
7. samd GPIO `i80bus` via `common/i80bus/gpio_bitbang.c` — done
8. `rgbmatrix` Protomatter backends — done
9. **Hardware validation**
   - **Done:** ESP32-P4 `mipidsi` + LVGL soft-reset (`lv_test_timer`); Qualia S3
     `dotclockframebuffer.DotClockFramebuffer` + touch (`lv_test_timer`) — see
     [soft-reset-and-bring-up.md](soft-reset-and-bring-up.md)
   - **Pending:** RK043 (mimxrt eLCDIF), RT1170 DSI, Pico DVI full panel soak
10. Lifecycle / soft-reset registry for all host-owning backends — **done**
    ([idempotent-lifecycle.md](idempotent-lifecycle.md))
11. mimxrt i80bus: board-specific pydevices config, optional DMA bulk path — pending
12. `displaydev`: remove legacy `RGBDisplay` package — done in pydevices

---

*Updated 2026-07-26 — Python module `dotclockframebuffer.DotClockFramebuffer` (CP-aligned name); P4 + Qualia bring-up; soft-reset lifecycle; MicroPython-only.*
