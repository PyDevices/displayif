# displayif — agent handoff (July 2026)

Handoff for **pydevices/displayif**: native display **interface** cmods for pydisplay. Portable code in `ports/common/`; SoC-specific code under `ports/<mp-port>/` (names from `micropython/ports/`).

Several pydisplay MicroPython `board_config.py` files currently raise `NotImplementedError` until the matching displayif module exists — **that is what this repo builds**.

**Workspace:** clone as a sibling under [PyDevices/cmods](https://github.com/PyDevices/cmods).

**No Python re-export layer in this repo.** Native C modules register directly (e.g. `from rgbframebuffer import RGBFrameBuffer`). pydisplay board configs import those modules; see [pydisplay changes](#pydisplay-changes-unify-on-fbdisplay--rgbframebuffer) below.

---

## Mission

| Location | Native module | pydisplay backend | Phase |
|----------|---------------|-------------------|-------|
| `ports/common` | `spibus` | **BusDisplay** | **1 — shipped** |
| `ports/esp32` | `rgbframebuffer` | **FBDisplay** | **2 — buffer; scanout WIP** |
| `ports/esp32` / `mimxrt` | `rgbmatrix` | **FBDisplay** | 3+ |
| `ports/mimxrt` | TBD | TBD | later |

All parallel dot-clock RGB panels (RGB-666 **and** 16-pin RGB565 wiring) use **`rgbframebuffer.RGBFrameBuffer`** + **`FBDisplay`**. There is no separate `RGBDisplay` / `present()` path.

---

## pydisplay changes (unify on FBDisplay + rgbframebuffer)

These edits belong in **pydisplay**, not displayif. Track here until done.

### 1. Remove `RGBDisplay`

| Item | Action |
|------|--------|
| `src/lib/displaysys/rgbdisplay.py` | Delete |
| `web/pyscript/src/lib/displaysys/rgbdisplay.py` | Delete |
| `packages/rgbdisplay.json` | Delete |
| `tests/test_rgbdisplay.py` | Delete |
| `scripts/publish_micropython_lib.sh` | Remove `displaysys-rgbdisplay` entry |
| `scripts/install_refresh_manifests.sh` | Remove `packages/rgbdisplay.json` |
| `scripts/README.md` | Remove `rgbdisplay` from manual packages list |

### 2. Update docs

| File | Change |
|------|--------|
| `docs/hardware/display-interfaces.md` | Parallel RGB row: CP `dotclockframebuffer` → MP `rgbframebuffer`; backend **FBDisplay** only (drop RGBDisplay column) |
| `docs/hardware/pydevices-roadmap.md` | Already lists `rgbframebuffer`; remove any `rgbdisplay` / RGB565-panel split |
| `docs/handoff/display-ecosystem.md` | Remove `rgbdisplay.py`, `present()` protocol, `RGBDisplay` naming notes |
| `docs/hardware/driver-inventory.md` | ST7701 / parallel RGB: `rgbframebuffer` + `FBDisplay`, not `RGBDisplay` |
| `docs/concepts/displays.md` | Confirm dotclock parallel RGB uses FBDisplay only |

### 3. Board configs

| Config | Change |
|--------|--------|
| `board_configs/fbdisplay/qualia_tl040hds20/` | Already correct: `RGBFrameBuffer` + `FBDisplay` |
| `board_configs/fbdisplay/t-rgb_480/` | Rewrite when displayif lands: drop `RGBDisplay` / `RGB565Panel` / `displayif.rgb565`; use `rgbframebuffer.RGBFrameBuffer` + `FBDisplay` (pin map and init sequence per hardware — fix known errors separately) |
| `board_configs/fbdisplay/t-rgb_480/package.json` | Replace `rgbdisplay.json` dep with `rgbframebuffer` package when manifest exists |
| Parallel RGB configs (future) | Same pattern as Qualia |

**Target board_config pattern** (matches CP Qualia):

```python
from rgbframebuffer import RGBFrameBuffer
from displaysys.fbdisplay import FBDisplay

fb = RGBFrameBuffer(de=..., vsync=..., hsync=..., dclk=...,
                    red=..., green=..., blue=...,   # RGB-666 pin layout
                    # or data=...                     # 16-pin RGB565 layout
                    frequency=..., width=..., height=..., ...)
display_drv = FBDisplay(fb)
```

Chip init (ST7701, IO expander, backlight GPIO) stays in pydisplay drivers / board config — not in displayif.

### 4. Naming rules (pydisplay + displayif)

- Do **not** use `rgb565` as a module or class name (collides with `rgb565(r, g, b)` color helper).
- Native module: **`rgbframebuffer`**, class **`RGBFrameBuffer`** — mirrors CP `dotclockframebuffer.DotClockFramebuffer`.
- Pixel format (666 vs 565) is a **constructor / pin-layout** choice, not a separate displaysys backend.

### 5. `FBDisplay` contract (unchanged)

displayif `RGBFrameBuffer` must implement what `FBDisplay` already expects:

- `.width`, `.height`
- `memoryview(fb)` — draw buffer (typecode `"H"` for 16-bit pixels where applicable)
- `.refresh()` — scan out to panel

No `present()`, `bitmap()`, or `RGBPanel` API.

### 6. New pydisplay MIP package (when displayif ships)

Add `packages/rgbframebuffer.json` pointing at board configs that need it (Qualia, parallel RGB boards). Remove `rgbdisplay.json`.

---

## pydisplay board configs waiting on displayif

| pydisplay MP board config | Native module | CP reference |
|---------------------------|---------------|--------------|
| `fbdisplay/matrixportal_s3_64x64` | `rgbmatrix` | `cp_matrixportal_s3_64x64` |
| `fbdisplay/matrixportal_m4_64x32` | `rgbmatrix` | `cp_matrixportal_m4_64x32` |
| `fbdisplay/rgb_matrix_featherwing_64x32` | `rgbmatrix` | `cp_rgb_matrix_featherwing_64x32` |
| `fbdisplay/qualia_tl040hds20` | `rgbframebuffer` | `cp_qualia_tl040hds20` |
| parallel RGB `fbdisplay/*` | `rgbframebuffer` | CP `dotclockframebuffer` configs |

---

## Filesystem layout

```
displayif/
├── micropython.mk / micropython.cmake / circuitpython.mk
├── include/displayif/
├── ports/
│   ├── common/          # spi/ + micropython.mk / .cmake / circuitpython.mk
│   ├── esp32/           # rgbframebuffer.c, rgbmatrix.c (planned)
│   └── mimxrt/
└── tests/
```

Each `ports/<name>/` directory has the same build filenames (`micropython.mk`, `micropython.cmake`, `circuitpython.mk`). Root makefiles detect the active port and include `ports/common/` plus the matching port tree.

---

## displayif native API — `rgbframebuffer`

CP equivalent: `dotclockframebuffer.DotClockFramebuffer`.

```python
fb = RGBFrameBuffer(
    de=Pin(...), vsync=Pin(...), hsync=Pin(...), dclk=Pin(...),
    # RGB-666 (Qualia-style):
    red=(...), green=(...), blue=(...),
    # OR 16-pin RGB565 parallel:
    # data=(...),
    frequency=16_000_000,
    width=720, height=720,
    hsync_pulse_width=..., hsync_front_porch=..., hsync_back_porch=...,
    vsync_pulse_width=..., vsync_front_porch=..., vsync_back_porch=...,
    hsync_idle_low=False, vsync_idle_low=False,
    de_idle_high=False, pclk_active_high=False, pclk_idle_high=False,
    overscan_left=0,  # optional
)
mv = memoryview(fb)
fb.refresh()
```

Framebuffer memory is allocated by the C driver (PSRAM on ESP32). pydisplay draws via `FBDisplay(fb)`; it does not call `alloc_buffer()` for scanout.

---

## Build (cmods)

```bash
./build_mp.sh --port rp2 --board RPI_PICO2_W
./build_mp.sh --port esp32 --board ESP32_GENERIC_S3
./build_mp.sh --port mimxrt --board TEENSY40
```

No `manifest.py` frozen package required unless we later add pure-Python helpers (not planned).

---

## Suggested work sequence

1. Scaffold (done)
2. pydisplay board configs on `rgbframebuffer` + `FBDisplay` (done)
3. `ports/common/spi/mod_spibus.c` + smoke test (done)
4. `ports/esp32/mod_rgbframebuffer.c` buffer protocol (done); ESP-IDF `refresh()` scanout
5. Qualia + parallel-RGB hardware validation
6. `rgbmatrix` on esp32 / mimxrt
7. CP patch script (`apply_cp_displayif_patches.sh`)

---

## Open questions

- SPI native module name (`spi` vs `displayif_spi` vs match viper `spibus` surface)
- PSRAM / `sdkconfig` sizing for large framebuffers on ESP32
- CP module registration pattern

---

*Updated 2026-07-06 — FBDisplay + rgbframebuffer only; pydisplay change list.*
