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
