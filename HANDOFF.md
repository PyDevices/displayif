# displayif — agent handoff (July 2026)

Handoff for **pydevices/displayif**: native display interface drivers for pydisplay. Portable interfaces live in `ports/common/`; SoC-specific code under `ports/<mp-port>/` (names from `micropython/ports/`).

**Workspace:** clone as a sibling under [PyDevices/cmods](https://github.com/PyDevices/cmods).

---

## Mission

| Location | Examples | Python import | pydisplay backend | Phase |
|----------|----------|---------------|-------------------|-------|
| `ports/common` | SPI display bus | `displayif.spi` | **BusDisplay** | **1** |
| `ports/esp32` | Parallel RGB LCD, RGB666 dotclock | `displayif.rgbpanel`, `displayif.rgb666` | **RGBDisplay**, **FBDisplay** | 2 |
| `ports/esp32` | HUB75 (if IDF-backed) | `displayif.rgbmatrix` | **FBDisplay** | 3+ |
| `ports/mimxrt` | LCDIF / ELCDIF / FlexIO | TBD | TBD | later |

**pydisplay** remains source of truth for pin maps, chip init, and API contracts.

### Naming: not `rgb565`

Do **not** use `rgb565` as a module or class name — that collides with color packing (`rgb565(r, g, b)`). Use:

- **`rgbpanel`** / `RGBPanel` — parallel timed RGB LCD (16-bit scanout; T-RGB)
- **`rgb666`** / `RGBFrameBuffer` — dotclock framebuffer (Qualia); matches CircuitPython RGB666 naming

---

## Filesystem layout

Build glue at **repo root** (cmods discovery):

```
displayif/
├── micropython.mk / micropython.cmake / circuitpython.mk / manifest.py
├── mk/
│   ├── detect.mk / detect.cmake / detect_cp.mk
│   ├── common.mk              # → ports/common/
│   ├── esp32.mk               # → ports/esp32/
│   └── mimxrt.mk              # → ports/mimxrt/
├── include/displayif/
├── ports/
│   ├── README.md
│   ├── common/                # portable — every port
│   │   └── spi/
│   │       ├── mod_spi.c
│   │       └── spi_bus.c
│   ├── esp32/                 # ESP-IDF (S3, P4, …)
│   │   ├── rgb_panel.c
│   │   └── rgb666.c
│   └── mimxrt/
├── py/displayif/
│   ├── spi.py
│   ├── rgbpanel.py
│   └── rgb666.py
└── tests/
```

### Layout rules

1. **Everything under `ports/`** — no separate `drivers/` tree.
2. **`ports/common/`** — portable; only `machine.*` / CP bus APIs.
3. **`ports/<port>/`** — folder name = MicroPython port name (`esp32`, `mimxrt`, …).
4. CP may use a different port dir (`espressif`); `detect_cp.mk` maps it to `ports/esp32/`.
5. Chip variants (S3 vs P4) stay in one port tree with `SOC_*` guards.

---

## Build (cmods)

```bash
./build_mp.sh --port rp2 --board RPI_PICO2_W       # common only
./build_mp.sh --port esp32 --board ESP32_GENERIC_S3  # common + esp32
./build_mp.sh --port mimxrt --board TEENSY40
```

```python
# cmods/manifest.py
package("displayif", base_path="displayif/py", opt=3)
```

| Condition | mk fragment | Sources |
|-----------|-------------|---------|
| always | `common.mk` | `ports/common/**` |
| `esp32` port | `esp32.mk` | `ports/esp32/**` |
| `mimxrt` port | `mimxrt.mk` | `ports/mimxrt/**` |

---

## Phase 1 — `displayif.spi` (`ports/common/spi/`)

Native SPI bus for pydisplay **BusDisplay** — replaces viper `spibus` where a C implementation is preferred.

**Deliverables:** `mod_spi.c`, `spi_bus.c`, smoke test on rp2 or esp32.

---

## Phase 2 — `ports/esp32/`

| Module | Board config | Notes |
|--------|--------------|-------|
| `displayif.rgbpanel` | `t-rgb_480` | ST7701 init in pydisplay `st7701.py` |
| `displayif.rgb666` | `qualia_tl040hds20` | dotclock / RGB666 |

### RGBPanel protocol (RGBDisplay)

```python
panel.present(x, y, width, height, buffer)
panel.bitmap(...)              # legacy alias
panel.backlight_on() / off()
panel.deinit()
```

Buffer: 16-bit RGB, row-major, `width * height * 2` bytes.

### T-RGB pins (480×480)

| Signal | GPIO |
|--------|------|
| RGB data (16) | 7, 6, 5, 3, 2, 14, 13, 12, 11, 10, 9, 21, 18, 17, 16, 15 |
| HSYNC | 47 |
| VSYNC | 41 |
| DE | 45 |
| PCLK | 42 |
| Backlight | 46 |

---

## Phase 3+ — `ports/mimxrt/`

NXP-specific scanout when board configs require it. Gated only in `mk/mimxrt.mk`.

---

## pydisplay mapping

```
board_configs/busdisplay/spi/*     →  displayif.spi
board_configs/fbdisplay/t-rgb_480  →  displayif.rgbpanel.RGBPanel
board_configs/fbdisplay/qualia_*   →  displayif.rgb666.RGBFrameBuffer
```

### Import contract

```python
from displayif.spi import SPIDisplayBus
from displayif.rgbpanel import RGBPanel
from displayif.rgb666 import RGBFrameBuffer
```

pydisplay board configs will need updating from the old `displayif.rgb565` / `RGB565Panel` names.

---

## Suggested commit sequence

1. Scaffold (done)
2. `ports/common/spi/` + smoke test
3. `ports/esp32/rgb_panel.c` + T-RGB hardware
4. `ports/esp32/rgb666.c` + Qualia
5. `ports/mimxrt/` when needed
6. CP patch script

---

## Open questions

- SPI vs viper `spibus` — coexist or deprecate?
- PSRAM for T-RGB full frame (~450 KB) with pydisplay `alloc_buffer()`
- CP module registration pattern (`apply_cp_displayif_patches.sh`)

---

*Updated 2026-07-06 — ports/common + MP port names; rgbpanel / rgb666 naming.*
