# displayif — agent handoff (July 2026)

Handoff for **pydevices/displayif**: native display **interface** modules for pydisplay. Portable code in `ports/common/`; SoC-specific code under `ports/<mp-port>/` (names from `micropython/ports/`).

**Start here for agents:** [AGENTS.md](AGENTS.md) → [IDEMPOTENT_LIFECYCLE.md](IDEMPOTENT_LIFECYCLE.md) → [SOFT_RESET_AND_BRINGUP.md](SOFT_RESET_AND_BRINGUP.md) (soft-reset wraps, symptom table, bring-up methods).

Several pydisplay MicroPython `board_config.py` files currently raise `NotImplementedError` until the matching displayif module exists — **that is what this repo builds**.

**Workspace:** clone as a sibling of `micropython/` (e.g. via [PyDevices/cmods](https://github.com/PyDevices/cmods), which is an optional convenience wrapper, not a requirement).

**No Python re-export layer in this repo.** Native C modules register directly (e.g. `from rgbframebuffer import RGBFrameBuffer`). pydisplay board configs import those modules.

**CircuitPython:** displayif does **not** ship CP bindings. CircuitPython already provides `dotclockframebuffer`, `mipidsi`, `paralleldisplaybus`, `picodvi`, etc. — use those for CP board configs under `pydisplay/board_configs/fbdisplay/cp_*`.

---

## Mission

| Location | Native module | pydisplay backend | Status |
|----------|---------------|-------------------|--------|
| `ports/common` | `spibus` / `i2cbus` | **BusDisplay** | shipped |
| `ports/esp32` | `rgbframebuffer` | **FBDisplay** | esp_lcd RGB scanout |
| `ports/esp32` | `i80bus` | bus driver | esp_lcd I80 (S3) |
| `ports/esp32` | `mipidsi` | **FBDisplay** | ESP32-P4 MIPI DSI |
| `ports/esp32` / `mimxrt` / `samd` / `rp2` | `rgbmatrix` | **FBDisplay** | Protomatter backends |
| `ports/rp2` | `i80bus` | bus driver | PIO+DMA |
| `ports/rp2` | `picodvi` | **FBDisplay** | RP2040 PIO / RP2350 HSTX |
| `ports/mimxrt` | `rgbframebuffer` | **FBDisplay** | eLCDIF on MIMXRT1062 |
| `ports/mimxrt` | `mipidsi` | **FBDisplay** | MIPI DSI on MIMXRT1176 |
| `ports/mimxrt` | `i80bus` | bus driver | FlexIO MCULCD 8080 on MIMXRT1062 |
| `ports/samd` | `i80bus` | bus driver | GPIO bit-bang (SAMD51) via `common/i80bus` |
| `ports/samd` | stubs | `rgbframebuffer`, `mipidsi` | import ok; ctor raises |

All parallel dot-clock RGB panels use **`rgbframebuffer.RGBFrameBuffer`** + **`FBDisplay`**. There is no separate `RGBDisplay` / `present()` path.

---

## pydisplay board configs (MicroPython)

These configs import displayif modules when firmware is built with the matching cmod:

| pydisplay MP board config | Native module |
|---------------------------|---------------|
| `fbdisplay/matrixportal_s3_64x64` | `rgbmatrix` |
| `fbdisplay/matrixportal_m4_64x32` | `rgbmatrix` |
| `fbdisplay/rgb_matrix_featherwing_64x32` | `rgbmatrix` |
| `fbdisplay/qualia_tl040hds20` | `rgbframebuffer` |
| `fbdisplay/mimxrt1060_evk_rk043_rgb` | `rgbframebuffer` |
| `fbdisplay/mimxrt1170_evk_waveshare_5dsi` | `mipidsi` |
| `fbdisplay/esp32-p4-wifi6-touch-lcd-4b` | `mipidsi` |
| `fbdisplay/pico2_dvi_sock_640x480` | `picodvi` |
| `fbdisplay/t-rgb_480` | `rgbframebuffer` |
| parallel RGB `fbdisplay/*` | `rgbframebuffer` |

CP siblings live under `fbdisplay/cp_*` and use CircuitPython native modules — not displayif.

---

## RP2350 / MIPI DSI

**RP2350 has no MIPI DSI host.** The chip provides **HSTX** (high-speed serial transmit) for DVI/TMDS — used by displayif `picodvi` and CircuitPython `picodvi`. CircuitPython’s `mipidsi` module is enabled only on SoCs with a DSI PHY (e.g. **ESP32-P4**, M5Stack Tab5) — not on Raspberry Pi Pico 2 / RP2350 boards.

TFT_eSPI and similar Arduino stacks on RP2040/RP2350 use **SPI** or **8-bit parallel (I80)** bit-bang/PIO, not MIPI DSI. For DSI panels you need a bridge chip (e.g. TC358762 on MIMXRT1170) or a SoC with native DSI (ESP32-P4).

---

## ESP32 PSRAM / sdkconfig (large framebuffers)

`rgbframebuffer` and `mipidsi` allocate framebuffers with `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` and set `fb_in_psram = 1` for esp_lcd RGB panels. If PSRAM is disabled or too small in `sdkconfig`, allocation falls back to internal RAM (may fail for 720×720+ panels).

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
├── include/displayif/
├── ports/
│   ├── common/          # spi/, i2c/, rgbmatrix/, i80bus/gpio_bitbang.c
│   ├── esp32/           # rgbframebuffer, i80bus, mipidsi, rgbmatrix (S3)
│   ├── mimxrt/          # rgbmatrix; eLCDIF rgbframebuffer; RT1176 mipidsi; FlexIO i80bus
│   ├── samd/            # rgbmatrix; GPIO i80bus (SAMD51); stubs
│   └── rp2/             # rgbmatrix; i80bus PIO+DMA; picodvi
└── tests/
```

Each `ports/<name>/` directory has `micropython.mk` and `micropython.cmake`. Root makefiles detect the active port and include `ports/common/` plus the matching port tree.

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

([cmods](https://github.com/PyDevices/cmods) `./build_mp.sh` is optional — e.g. `./build_mp.sh --port rp2 --board RPI_PICO2_W`.)

No `manifest.py` frozen package required unless we later add pure-Python helpers (not planned).

---

## Suggested work sequence

1. Scaffold — done
2. pydisplay board configs on `rgbframebuffer` + `FBDisplay` — done
3. `spibus` + smoke tests — done
4. esp32 `rgbframebuffer`, `i80bus`, `mipidsi` — done
5. mimxrt eLCDIF `rgbframebuffer`, RT1176 `mipidsi`, FlexIO `i80bus` — done
6. rp2 `picodvi`, PIO `i80bus` — done
7. samd GPIO `i80bus` via `common/i80bus/gpio_bitbang.c` — done
8. `rgbmatrix` Protomatter backends — done
9. **Hardware validation** on real panels (Qualia, RK043, RT1170 DSI, P4 4B, Pico DVI) — in progress (P4 mipidsi + LVGL soft-reset bring-up: see [SOFT_RESET_AND_BRINGUP.md](SOFT_RESET_AND_BRINGUP.md))
10. mimxrt i80bus: board-specific pydisplay config, optional DMA bulk path — pending
11. pydisplay: remove legacy `RGBDisplay` package — see pydisplay repo

---

*Updated 2026-07-20 — soft-reset wraps / P4 bring-up notes; MicroPython-only.*
