# displayif — agent handoff (July 2026)

Handoff for **pydevices/displayif**: native display **interface** cmods for pydisplay. Portable code in `ports/common/`; SoC-specific code under `ports/<mp-port>/` (names from `micropython/ports/`).

Several pydisplay MicroPython `board_config.py` files currently raise `NotImplementedError` until the matching displayif module exists — **that is what this repo builds**.

**Workspace:** clone as a sibling under [PyDevices/cmods](https://github.com/PyDevices/cmods).

---

## Mission

| Location | Interface | Python import | pydisplay backend | Phase |
|----------|-----------|---------------|-------------------|-------|
| `ports/common` | SPI display bus | `displayif.spi` | **BusDisplay** | **1** |
| `ports/esp32` | Parallel RGB LCD | `displayif.rgbpanel` | **RGBDisplay** | 2 |
| `ports/esp32` | RGB666 dotclock | `displayif.rgb666` | **FBDisplay** | 2 |
| `ports/esp32` / `mimxrt` | HUB75 LED matrix | `displayif.rgbmatrix` | **FBDisplay** | 3+ |
| `ports/mimxrt` | LCDIF / ELCDIF / FlexIO | TBD | TBD | later |

**pydisplay** remains source of truth for pin maps, chip init sequences, and API contracts. Board configs import displayif modules once the cmod exists; pydisplay will update those imports as drivers land.

### Naming: not `rgb565`

Do **not** use `rgb565` as a module or class name — that collides with color packing (`rgb565(r, g, b)`). Use:

- **`rgbpanel`** / `RGBPanel` — parallel timed RGB LCD (16-bit scanout)
- **`rgb666`** / `RGBFrameBuffer` — dotclock framebuffer; matches CircuitPython RGB666 naming

---

## pydisplay board configs waiting on displayif

| pydisplay MP board config | displayif module | CP reference |
|---------------------------|------------------|--------------|
| `fbdisplay/matrixportal_s3_64x64` | `displayif.rgbmatrix` | `cp_matrixportal_s3_64x64` |
| `fbdisplay/matrixportal_m4_64x32` | `displayif.rgbmatrix` | `cp_matrixportal_m4_64x32` |
| `fbdisplay/rgb_matrix_featherwing_64x32` | `displayif.rgbmatrix` | `cp_rgb_matrix_featherwing_64x32` |
| `fbdisplay/qualia_tl040hds20` | `displayif.rgb666` | `cp_qualia_tl040hds20` |
| parallel RGB `fbdisplay/*` | `displayif.rgbpanel` | CP `dotclockframebuffer` configs |

Parallel-RGB MP board configs will be wired to `displayif.rgbpanel` when the driver exists (pydisplay edits those imports separately).

---

## Filesystem layout

Root build glue (cmods discovery):

```
displayif/
├── micropython.mk / micropython.cmake / circuitpython.mk / manifest.py
├── include/displayif/
├── ports/
│   ├── README.md
│   ├── common/
│   │   ├── micropython.mk / micropython.cmake / circuitpython.mk
│   │   └── spi/
│   ├── esp32/
│   │   ├── micropython.mk / micropython.cmake / circuitpython.mk
│   │   ├── rgb_panel.c
│   │   └── rgb666.c
│   └── mimxrt/
│       ├── micropython.mk / micropython.cmake / circuitpython.mk
│       └── …
├── py/displayif/
└── tests/
```

Each `ports/<name>/` tree carries the **same trio** of build files:

- `micropython.mk` — Make-based MP ports
- `micropython.cmake` — CMake MP ports (esp32, rp2, …)
- `circuitpython.mk` — CP ports

Root `micropython.mk` detects the active port and `include`s `ports/common/` plus the matching `ports/<port>/micropython.mk`.

### Layout rules

1. **Everything under `ports/`** — sources and per-port build glue together.
2. **`ports/common/`** — portable; only `machine.*` / CP bus APIs.
3. **`ports/<port>/`** — folder name = MicroPython port name (`esp32`, `mimxrt`, …).
4. CP may use a different port dir (`espressif`); root `circuitpython.mk` maps it to `ports/esp32/`.
5. Chip variants (S3 vs P4) stay in one port tree with `SOC_*` guards.

---

## Build (cmods)

```bash
./build_mp.sh --port rp2 --board RPI_PICO2_W
./build_mp.sh --port esp32 --board ESP32_GENERIC_S3
./build_mp.sh --port mimxrt --board TEENSY40
```

```python
# cmods/manifest.py
package("displayif", base_path="displayif/py", opt=3)
```

| Condition | Included | Sources |
|-----------|----------|---------|
| always | `ports/common/micropython.mk` | `ports/common/**` |
| `esp32` port | `ports/esp32/micropython.mk` | `ports/esp32/**` |
| `mimxrt` port | `ports/mimxrt/micropython.mk` | `ports/mimxrt/**` |

---

## Phase 1 — `displayif.spi` (`ports/common/spi/`)

Native SPI bus for pydisplay **BusDisplay**.

**Deliverables:** `mod_spi.c`, `spi_bus.c`, smoke test on any SPI-capable port.

---

## Phase 2 — `ports/esp32/`

| Module | Interface | pydisplay backend |
|--------|-----------|-------------------|
| `displayif.rgbpanel` | Parallel timed RGB LCD | **RGBDisplay** |
| `displayif.rgb666` | RGB666 dotclock framebuffer | **FBDisplay** |

### RGBPanel protocol (RGBDisplay)

```python
panel.present(x, y, width, height, buffer)
panel.bitmap(...)              # legacy alias
panel.backlight_on() / off()
panel.deinit()
```

Buffer: 16-bit RGB, row-major, `width * height * 2` bytes.

---

## Phase 3 — `displayif.rgbmatrix`

HUB75 scanout for MatrixPortal and FeatherWing board configs. Implementation port depends on hardware (likely `ports/esp32/` for S3, `ports/mimxrt/` for M4). Mirror CircuitPython `rgbmatrix.RGBMatrix` constructor surface from the `cp_matrixportal_*` / `cp_rgb_matrix_*` board configs.

---

## Phase 4+ — `ports/mimxrt/`

NXP-specific scanout when board configs require it.

---

## Import contract

```python
from displayif.spi import SPIDisplayBus
from displayif.rgbpanel import RGBPanel
from displayif.rgb666 import RGBFrameBuffer
from displayif.rgbmatrix import RGBMatrix   # when implemented
```

---

## Suggested commit sequence

1. Scaffold (done)
2. `ports/common/spi/` + smoke test
3. `ports/esp32/rgb_panel.c` + parallel-RGB board config on hardware
4. `ports/esp32/rgb666.c` + Qualia board config
5. `ports/esp32/` or `ports/mimxrt/` `rgbmatrix` + MatrixPortal configs
6. CP patch script

---

## Open questions

- SPI vs viper `spibus` — coexist or deprecate?
- PSRAM strategy for large parallel-RGB framebuffers with pydisplay `alloc_buffer()`
- CP module registration (`apply_cp_displayif_patches.sh`)

---

*Updated 2026-07-06 — per-port build files; displayif as interface cmod provider.*
