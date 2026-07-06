# displayif — agent handoff (July 2026)

Handoff for **pydevices/displayif**: native display interface drivers for pydisplay. Targets span **universal** buses (any port with `machine.SPI`), **ESP-IDF** SoCs (ESP32-S3, ESP32-P4, …), **i.MX RT** (NXP), and future MCU families.

**Workspace:** clone as a sibling under [PyDevices/cmods](https://github.com/PyDevices/cmods) next to `micropython/`, `graphics/`, etc.

**Audience:** a future session picking up displayif work in desktop Cursor or any dev environment.

---

## Mission

Implement display interfaces so pydisplay `board_config.py` files run on MicroPython (and eventually CircuitPython) the same way they do on CircuitPython — using native code where pure Python or viper (`spibus`) is not enough.

| Tier | Examples | Python import | pydisplay backend | Phase |
|------|----------|---------------|-------------------|-------|
| **Universal** | SPI display bus / pixel push | `displayif.spi` | **BusDisplay** (`SPIBus` complement) | **1 — start here** |
| **ESP-IDF** | RGB565 parallel, RGB666 dotclock | `displayif.rgb565`, `displayif.rgbframebuffer` | **RGBDisplay**, **FBDisplay** | 2 |
| **ESP-IDF** | HUB75 (if IDF-backed) | `displayif.rgbmatrix` | **FBDisplay** | 3+ |
| **i.MX RT** | LCDIF / ELCDIF / FlexIO panels | TBD (`displayif.*`) | TBD | later |
| **Other MCU** | SoC-specific blocks | under `ports/<family>/` | per board config | as needed |

**pydisplay remains source of truth** for pin maps, chip init sequences, and API contracts. Generate displayif implementations *from* pydisplay board configs and vendored drivers — do not fork parallel pin tables.

---

## Filesystem layout

Build glue lives at the **repo root** so cmods discovery works:

- MicroPython: `USER_C_MODULES=<cmods>` → `displayif/micropython.mk`
- CMake ports (ESP32, RP2): `cmods/micropython.cmake` → `displayif/micropython.cmake`
- CircuitPython: `displayif/circuitpython.mk` included from the port `Makefile` (patch script; see [CP integration](#circuitpython-integration))

```
displayif/                          # cloned: ~/github/cmods/displayif
├── README.md
├── HANDOFF.md                      # this file
├── micropython.mk                  # port/MCU gating → includes mk/*.mk
├── micropython.cmake               # CMake gating (esp32, rp2, …)
├── circuitpython.mk                # CP port gating
├── manifest.py                     # frozen py/displayif package (cmods manifest.py picks this up)
├── apply_cp_displayif_patches.sh   # patches CP port Makefile (graphics/usdl2 pattern)
│
├── mk/                             # build fragments (included by root .mk / .cmake)
│   ├── common.mk                   # universal drivers (SPI)
│   ├── esp_idf.mk                  # ESP32-S3, P4, …
│   ├── imxrt.mk                    # mimxrt port only
│   └── detect.mk                   # shared PORT_DIR / BOARD sniffing helpers
│
├── include/
│   └── displayif/
│       ├── common.h                # shared types, error codes
│       └── spi.h
│
├── drivers/                        # **portable** — compile on any capable MP/CP port
│   └── spi/
│       ├── mod_spi.c               # MicroPython module glue → displayif.spi
│       ├── spi_bus.c               # bus protocol (DCS write, windowed RAMWR)
│       ├── spi_bus_cp.c            # CircuitPython glue (if not shared)
│       └── displayif_spi_qstrdefs.h
│
├── ports/                          # **MCU-specific** — gated in mk/*.mk
│   ├── esp_idf/
│   │   ├── rgb565_panel.c        # ESP LCD RGB panel (S3, P4, …)
│   │   ├── rgb_framebuffer.c     # dotclock / RGB666
│   │   ├── esp_idf_rgb.h
│   │   └── README.md             # IDF version pins, sdkconfig notes
│   ├── imxrt/
│   │   ├── README.md             # NXP blocks per board (Teensy 4.x, M7 EVKs, …)
│   │   └── (drivers added per feature)
│   └── README.md                 # when to add a new ports/<family>/ tree
│
├── py/
│   └── displayif/                  # frozen package (manifest.py)
│       ├── __init__.py             # version, capability helpers
│       ├── spi.py                  # re-exports / fallbacks for native spi
│       ├── rgb565.py               # ImportError stub off ESP; re-export on ESP
│       └── rgbframebuffer.py
│
├── circuitpython_spike/            # shared-bindings / shared-module stubs (CP only)
│   ├── copy_manifest.txt
│   ├── shared-bindings/displayif/
│   └── shared-module/displayif/
│
└── tests/
    ├── test_spi_smoke.py           # Phase 1 — any SPI-capable port
    ├── test_rgb565_smoke.py        # ESP32-S3 + t-rgb_480 board config
    └── README.md
```

### Layout rules

1. **`drivers/`** — code that uses only MicroPython `machine.SPI` / `machine.Pin` (or CP bus APIs). No ESP-IDF or NXP SDK headers.
2. **`ports/<family>/`** — code that includes SoC SDK headers (`esp_lcd`, `fsl_elcdif`, …). One subdirectory per SDK family, not per chip variant.
3. **Chip variants** (S3 vs P4) are selected with `#if` / IDF `SOC_*` macros inside `ports/esp_idf/`, not separate top-level trees.
4. **Root `micropython.mk`** never lists source files directly — it `include`s `mk/common.mk` and zero or more `mk/<family>.mk` based on `PORT` / `BOARD`.
5. **New universal driver** → `drivers/<name>/` + one line in `mk/common.mk`.
6. **New SoC driver** → `ports/<family>/` + fragment in `mk/<family>.mk`.

---

## Build integration (cmods)

### MicroPython

```bash
cd ~/github/cmods
git clone https://github.com/PyDevices/displayif.git displayif
./build_mp.sh --port esp32 --board ESP32_GENERIC_S3    # common SPI + esp_idf RGB
./build_mp.sh --port mimxrt --board TEENSY40           # common SPI only (until imxrt drivers land)
./build_mp.sh --port rp2 --board RPI_PICO2_W           # common SPI only
```

Add to `cmods/manifest.py` when freezing Python wrappers:

```python
package("displayif", base_path="displayif/py", opt=3)
```

### `micropython.mk` gating (sketch)

Root file sets `DISPLAYIF_MOD_DIR := $(USERMOD_DIR)` then includes fragments:

| Condition | Includes | Objects |
|-----------|----------|---------|
| always (when mod enabled) | `mk/common.mk` | `drivers/spi/*.c` |
| `PORT` = `esp32` | `mk/esp_idf.mk` | `ports/esp_idf/*.c` |
| `PORT` = `mimxrt` | `mk/imxrt.mk` | `ports/imxrt/*.c` (when present) |

Use the same `PORT_DIR_ABS` / `findstring /ports/esp32` pattern as [usdl2/micropython.mk](https://github.com/PyDevices/usdl2/blob/main/micropython.mk).

### `micropython.cmake` (ESP32 / RP2)

Mirror the Make gating: `add_library(displayif_common INTERFACE)` always; `displayif_esp_idf` only when `IDF_TARGET` is `esp32s3`, `esp32p4`, etc. Link into `usermod` the same way as [lv_micropython_cmod/micropython.cmake](https://github.com/PyDevices/lv_micropython_cmod/blob/main/micropython.cmake).

### CircuitPython integration

CP does **not** scan `USER_C_MODULES`. Follow the **graphics** / **usdl2** model:

1. `circuitpython.mk` at repo root (port-gated sources + `CFLAGS`).
2. `apply_cp_displayif_patches.sh` adds `include $(DISPLAYIF_MOD_DIR)/circuitpython.mk` to the target port `Makefile` and copies `circuitpython_spike/` bindings into the CP tree.
3. Optionally teach `lv_circuitpython_mod/build_cp.sh` to call `apply_cp_displayif_patches.sh` (same hook as `run_usdl2_patches`).

---

## Phase 1 — universal SPI (`displayif.spi`)

**Goal:** native SPI display bus usable from pydisplay **BusDisplay** board configs without viper `spibus`.

**Reference pydisplay:**

- `board_configs/busdisplay/spi/` — pin maps and chip drivers
- `src/lib/displaysys/busdisplay.py` — expects a bus with `send()`, `init()`, etc.
- Existing pure-Python `spibus` (viper) — displayif SPI should match or exceed its protocol surface

**Deliverables:**

1. `drivers/spi/mod_spi.c` + `spi_bus.c`
2. `mk/common.mk` wired from root `micropython.mk` / `circuitpython.mk`
3. `py/displayif/spi.py` + root `manifest.py`
4. `tests/test_spi_smoke.py` (loopback or ILI9341-class board config)

**Non-goals for Phase 1:** RGB parallel, HUB75, ESP-IDF LCD peripheral.

---

## Phase 2 — ESP-IDF targets

After SPI is solid on ESP32 builds, add `ports/esp_idf/`:

| Module | Board config | Notes |
|--------|--------------|-------|
| `displayif.rgb565` | `board_configs/fbdisplay/t-rgb_480` | ST7701 init stays in pydisplay `st7701.py` |
| `displayif.rgbframebuffer` | `board_configs/fbdisplay/qualia_tl040hds20` | RGB666 dotclock |

### Panel protocol (RGBDisplay) — unchanged from pydisplay

```python
panel.present(x, y, width, height, buffer)   # preferred
panel.bitmap(x, y, width, height, buffer)    # legacy alias
panel.backlight_on() / panel.backlight_off() # optional
panel.deinit()                               # optional
```

Buffer: **RGB565**, row-major, `width * height * 2` bytes.

### T-RGB pin map (from board config)

| Signal | GPIO |
|--------|------|
| RGB data (16) | 7, 6, 5, 3, 2, 14, 13, 12, 11, 10, 9, 21, 18, 17, 16, 15 |
| HSYNC | 47 |
| VSYNC | 41 |
| DE | 45 |
| PCLK | 42 |
| Backlight | 46 |

Panel: **480×480**, RGB565.

---

## Phase 3+ — i.MX RT and others

Add `ports/imxrt/` when a pydisplay board config needs NXP LCD blocks (FlexIO, ELCDIF, …). Gate exclusively in `mk/imxrt.mk` so ESP and RP2 builds never compile NXP headers.

Document each new family in `ports/<family>/README.md` with:

- Supported boards / MP `BOARD` names
- SDK or register headers assumed
- pydisplay board configs that consume the driver

---

## Relationship to pydisplay

```
pydisplay                          displayif
─────────                          ─────────
board_configs/busdisplay/spi/*     →  displayif.spi (Phase 1)
board_configs/fbdisplay/t-rgb_480 → displayif.rgb565 (Phase 2, esp_idf)
drivers/display/st7701.py          →  init only (stays in pydisplay)
board_configs/fbdisplay/qualia_*   →  displayif.rgbframebuffer (Phase 2)
board_configs/fbdisplay/matrixportal_* → displayif.rgbmatrix (later)
```

### Import contract (board configs)

```python
from displayif.spi import SPIDisplayBus          # Phase 1
from displayif.rgb565 import RGB565Panel         # Phase 2 (ESP-IDF)
from displayif.rgbframebuffer import RGBFrameBuffer
```

Flat fallbacks (`from rgb565 import …`) may remain for MIP convenience.

---

## Suggested PR sequence

1. **Scaffold** — layout above, root `micropython.mk` / `manifest.py`, empty `mk/common.mk`, CI smoke on `unix` or `rp2` (compile-only).
2. **SPI vertical slice** — `displayif.spi` + pydisplay SPI board config smoke test.
3. **ESP-IDF RGB565** — `ports/esp_idf/rgb565_panel.c` + `t-rgb_480` on hardware.
4. **ESP-IDF RGB666** — Qualia board config.
5. **i.MX RT** — first `ports/imxrt/` driver when a board config exists.
6. **CP patches** — `apply_cp_displayif_patches.sh` + unix/espressif smoke.

---

## Open questions

- **SPI vs spibus:** deprecate viper `spibus` in pydisplay once `displayif.spi` covers the same boards, or keep both?
- **RGB666 module name:** align with CP `rgbframebuffer` vs namespace `displayif.rgbframebuffer`.
- **PSRAM:** T-RGB 480×480 RGB565 ≈ 450 KB — confirm allocation with pydisplay `alloc_buffer()`.
- **CP module surface:** one `displayif` package vs separate top-level modules like `usdl2`.

---

## Key pydisplay links

| Resource | URL |
|----------|-----|
| Display interfaces matrix | https://github.com/PyDevices/pydisplay/blob/main/docs/hardware/display-interfaces.md |
| T-RGB board config | https://github.com/PyDevices/pydisplay/blob/main/board_configs/fbdisplay/t-rgb_480/board_config.py |
| RGBDisplay backend | https://github.com/PyDevices/pydisplay/blob/main/src/lib/displaysys/rgbdisplay.py |
| cmods workspace | https://github.com/PyDevices/cmods |

---

*Updated 2026-07-06 — multi-target layout (universal SPI first, then ESP-IDF, i.MX RT, others).*
