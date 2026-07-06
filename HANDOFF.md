# displayif — agent handoff (July 2026)

Handoff for continuing **pydevices/displayif**: native MicroPython drivers for framebuffer-class display interfaces that pydisplay cannot implement in pure Python. Written after pydisplay **v0.0.7** (display ecosystem PR #46) and a successful micropython-lib publish.

**Audience:** a future session in desktop Cursor (or any dev environment) picking up displayif work.

---

## Mission

Implement SoC-backed display interfaces so pydisplay `board_config.py` files can run on MicroPython the same way they do on CircuitPython.

| Module (planned) | Interface | pydisplay backend | Priority |
|------------------|-----------|-------------------|----------|
| `displayif.rgb565` | RGB parallel timed (RGB565) | `displaysys.rgbdisplay.RGBDisplay` | **P0** — unblocks LilyGO T-RGB |
| `displayif.rgb666` / `rgbframebuffer` | RGB parallel (RGB666) | `displaysys.fbdisplay.FBDisplay` | **P0** — unblocks Qualia TL040HDS20 |
| `displayif.rgbmatrix` | HUB75 LED matrix | `displaysys.fbdisplay.FBDisplay` | **P1** — MatrixPortal MP pairs |
| NeoPixel grid mapper | WS2812 / DotStar matrix | `displaysys.pixeldisplay.PixelDisplay` | **P2** — may stay in pydisplay or move here |

**Ignore existing repo contents** if present — pydisplay is the source of truth for wiring, pin maps, and API contracts. Generate displayif implementations *from* pydisplay board configs and drivers.

---

## Relationship to pydisplay

```
pydisplay                          displayif (this repo)
─────────                          ─────────────────────
board_configs/fbdisplay/t-rgb_480  →  RGB565Panel (ESP32-S3 LCD peripheral)
drivers/display/st7701.py          →  init only (already in pydisplay)
drivers/io_expander/xl9535.py      →  stays in pydisplay (I2C GPIO for T-RGB)
displaysys/rgbdisplay.py           →  consumes panel.present(x,y,w,h,buffer)

board_configs/fbdisplay/qualia_*  →  RGBFrameBuffer (ESP32-S3 RGB666 timings)
displaysys/fbdisplay.py            →  consumes fb.refresh() + memoryview

board_configs/fbdisplay/matrixportal_* → rgbmatrix.RGBMatrix equivalent
```

### Panel protocol (RGBDisplay)

`displaysys.rgbdisplay.RGBDisplay` duck-types the panel object:

```python
panel.present(x, y, width, height, buffer)   # preferred
panel.bitmap(x, y, width, height, buffer)    # legacy alias
panel.backlight_on() / panel.backlight_off() # optional
panel.deinit()                               # optional
```

Buffer is **RGB565**, row-major, `width * height * 2` bytes. See `PyDevices/pydisplay/src/lib/displaysys/rgbdisplay.py`.

### Import contract in board configs

`board_configs/fbdisplay/t-rgb_480/board_config.py` tries:

```python
from displayif.rgb565 import RGB565Panel   # preferred
# fallback: from rgb565 import RGB565Panel
```

MIP / frozen builds should expose `displayif.rgb565` (or document the flat import).

---

## P0 — RGB565Panel (LilyGO T-RGB)

**Reference config:** `PyDevices/pydisplay/board_configs/fbdisplay/t-rgb_480/board_config.py`

**Already done in pydisplay (do not duplicate):**

- ST7701 register init: `drivers/display/st7701.py` — `run_init(lcd_pins)` bit-bangs 3-wire SPI via XL9535
- XL9535 expander: `drivers/io_expander/xl9535.py`
- `RGBDisplay` RAM buffer + `show()` → `panel.present(0, 0, w, h, buffer)`

**Pin map (from board config):**

| Signal | GPIO |
|--------|------|
| RGB data (16) | 7, 6, 5, 3, 2, 14, 13, 12, 11, 10, 9, 21, 18, 17, 16, 15 |
| HSYNC | 47 |
| VSYNC | 41 |
| DE | 45 |
| PCLK | 42 |
| Backlight | 46 |

Panel: **480×480**, RGB565.

**Implementation notes:**

- Target **ESP32-S3** LCD RGB peripheral (or ESP-IDF panel API wrapped for MicroPython).
- LilyGO reference: `lilygo-micropython` T-RGB board tree (ST7701 init sequence already vendored into pydisplay `st7701.py`).
- Do **not** depend on LilyGO `lcd` module — pydisplay explicitly avoids it.
- First milestone: solid fill + full-frame `present()` from `RGBDisplay.show()`.
- Second: partial updates if hardware supports DMA from PSRAM buffer.

**Smoke test:**

1. Build firmware with displayif + pydisplay frozen or on filesystem.
2. Install `board_configs/fbdisplay/t-rgb_480` via MIP.
3. Run `displaysys_simpletest.py` or `pydisplay_demo.py`.

---

## P0 — RGBFrameBuffer (Qualia / RGB666)

**Reference config:** `PyDevices/pydisplay/board_configs/fbdisplay/qualia_tl040hds20/board_config.py`

**CircuitPython equivalent:** `dotclockframebuffer` + `qualia` ecosystem.

**API expected by FBDisplay:**

```python
fb = RGBFrameBuffer(de=..., vsync=..., hsync=..., dclk=...,
                    red=(...), green=(...), blue=(...),
                    frequency=..., width=720, height=720, ...)
mv = memoryview(fb)          # typecode "H" — unsigned short per pixel
fb.refresh()                 # scan out to panel
```

Timings and pin tuples are in the board config. FT6x36 touch stays in pydisplay (`drivers/touch/ft6x36.py`).

**CP sibling:** `board_configs/fbdisplay/cp_qualia_tl040hds20/`

---

## P1 — rgbmatrix (HUB75)

**Blocked MP configs** (raise `NotImplementedError` until cmod exists):

- `board_configs/fbdisplay/matrixportal_s3_64x64/board_config.py`
- `board_configs/fbdisplay/matrixportal_m4_64x32/board_config.py`
- `board_configs/fbdisplay/rgb_matrix_featherwing_64x32/board_config.py`

**Working CP references:**

- `board_configs/fbdisplay/cp_matrixportal_s3_64x64/board_config.py`
- `board_configs/fbdisplay/cp_matrixportal_m4_64x32/board_config.py`
- `board_configs/fbdisplay/cp_rgb_matrix_featherwing_64x32/board_config.py`

Mirror CircuitPython `rgbmatrix.RGBMatrix` constructor args from CP configs. `FBDisplay` wraps the matrix framebuffer the same way as Qualia.

---

## Repo layout (recommended)

Follow sibling cmod repos (`PyDevices/graphics`, `PyDevices/usdl2`):

```
displayif/
├── README.md
├── HANDOFF.md              # this file
├── micropython.mk          # USER_C_MODULES discovery
├── manifest.py             # frozen Python bindings if any
├── displayif/
│   ├── rgb565.c            # ESP32-S3 RGB565 panel
│   ├── rgb666.c            # Qualia RGB666
│   └── rgbmatrix.c         # HUB75 (later)
└── tests/
    └── test_rgb565_smoke.py
```

**Build workspace:** clone into `PyDevices/cmods` sibling directory (see `cmods/README.md`). `USER_C_MODULES` picks up `*/micropython.mk`.

```bash
cd ~/github/cmods
git clone https://github.com/PyDevices/displayif.git
git clone https://github.com/micropython/micropython.git micropython
./build_mp.sh --port esp32 --board ESP32_GENERIC_S3
```

---

## MIP / micropython-lib integration (later)

After the cmod works on hardware:

1. Add `packages/displayif_rgb565.json` (or per-module manifests) in **pydisplay** `packages/`.
2. Optionally publish native bindings through micropython-lib if pure-Python stubs are needed.
3. Bump pydisplay release tag; publish workflow syncs manifests.

pydisplay **v0.0.7** already publishes `rgbdisplay` Python package to TestPyPI/MIP; displayif is the missing native piece.

---

## Key pydisplay links

| Resource | URL |
|----------|-----|
| Display interfaces matrix | https://github.com/PyDevices/pydisplay/blob/main/docs/hardware/display-interfaces.md |
| pydevices roadmap | https://github.com/PyDevices/pydisplay/blob/main/docs/hardware/pydevices-roadmap.md |
| T-RGB board config | https://github.com/PyDevices/pydisplay/blob/main/board_configs/fbdisplay/t-rgb_480/board_config.py |
| RGBDisplay backend | https://github.com/PyDevices/pydisplay/blob/main/src/lib/displaysys/rgbdisplay.py |
| ST7701 init driver | https://github.com/PyDevices/pydisplay/blob/main/drivers/display/st7701.py |
| pydisplay continuation handoff | https://github.com/PyDevices/pydisplay/blob/main/docs/handoff/display-ecosystem.md |

---

## Suggested first PR (displayif)

1. Scaffold `micropython.mk` + minimal `RGB565Panel` type on ESP32-S3.
2. Implement `present()`, `backlight_on/off`, `deinit`.
3. Document build in `README.md`.
4. Verify against pydisplay `t-rgb_480` board config on physical T-RGB hardware.

Do **not** start with rgbmatrix or NeoPixel — T-RGB RGB565 is the narrowest vertical slice with a complete pydisplay board config and tests (`tests/test_rgbdisplay.py`, `tests/test_st7701.py` in pydisplay).

---

## Open questions

- **Repo name vs import path:** board configs use `displayif.rgb565`; repo may be `PyDevices/displayif` cloned as sibling `displayif/` under cmods.
- **RGB666 naming:** board config imports `rgbframebuffer.RGBFrameBuffer` today — align module name with CP (`rgbframebuffer`) or namespace under `displayif`.
- **PSRAM:** T-RGB 480×480 RGB565 buffer is ~450 KB — confirm allocation strategy with pydisplay `alloc_buffer()`.

---

*Generated 2026-07-06 after pydisplay display-ecosystem work and micropython-lib v0.0.7 publish.*
