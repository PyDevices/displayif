# jpegio — baseline JPEG decoder (CircuitPython API, TJpgDec R0.03)

`import jpegio` gives MicroPython the same `JpegDecoder` CircuitPython ships
natively, so one script decodes on both — CircuitPython uses its own, a
MicroPython firmware built with displayif uses this one. It exists so a
`displaydev` firmware can show a camera's MJPEG frames (or a JPEG on flash)
without carrying LVGL for its decoder (org `docs/jpegio-vision.md`).

Platform-neutral C: built on every port from the root `micropython.mk` /
`micropython.cmake`. Output is native-order RGB565, block by block, straight
from TJpgDec — no full-frame buffer inside the module.

Beside the `lvgl-micropython` usermod it is also LVGL's JPEG decoder: the
same TJpgDec, registered through LVGL's public `lv_image_decoder_create`
API, so `lv.image` shows JPEGs (webcam MJPEG frames included) with one
decoder in the firmware — see [LVGL image decoder](#lvgl-image-decoder).

```python
import jpegio
decoder = jpegio.JpegDecoder()          # allocates its ~3.7 KB work area once
width, height = decoder.open("/sd/photo.jpg")
buf = bytearray(width * height * 2)     # RGB565, tight, row-major
decoder.decode(buf)
display_drv.blit_rect(buf, 0, 0, width, height)

# camera: hand each MCU block straight to the display, no frame buffer
width, height = decoder.open(frame)     # bytes/bytearray/memoryview of one MJPEG frame
decoder.decode(lambda x, y, w, h, mv: display_drv.blit_rect(mv, x, y, w, h), 1)
```

## API

### `jpegio.JpegDecoder()`

No arguments. Holds TJpgDec's decoder state and its `TJPGD_WORKSPACE_SIZE`
(3500-byte, CircuitPython's number) work area inside the object, allocated
once — create one decoder and reuse it, as CP's docs advise.

### `open(source) -> (width, height)`

Positional-only, like CP. `source` is one of:

- `str` — a path; opened `"rb"` by the module and closed after `decode()`
  (or when `open()` fails, or on the next `open()`).
- a bytes-like object (`bytes`, `bytearray`, `memoryview`, anything with the
  buffer protocol) — read in place through the buffer protocol; the source is
  not copied. Keep it alive and unchanged until `decode()` returns.
- a binary stream: any object with MicroPython's native stream protocol
  (`open(...)` files, `io.BytesIO`, sockets). Unseekable streams are fine;
  skipped segments are read through, never seeked. Text streams,
  pure-Python objects that merely define `read()`, and anything else
  (`None`, an int, a list) are not accepted (TypeError).

Parses the headers (`jd_prepare`) and returns the image size. `width` and
`height` are also readable as properties and track the last `open()`:
valid after a successful one (still valid after its `decode()`, so
`blit_rect(buf, 0, 0, decoder.width, decoder.height)` works), RuntimeError
`width needs a successful open()` before any `open()` or after one that
failed — a failed `open()` never leaves the previous image's size behind.

**What is sniffed: nothing beyond SOI.** The stream goes to `jd_prepare`,
which scans for `FF D8` and then takes the segments in whatever order they
arrive. There is deliberately no JFIF/APP0 check: UVC MJPEG frames from a
webcam are `SOI → DQT → SOF0 → DHT → … → DRI → SOS`, not JFIF-first, and an
`is_jpg()`-style sniff rejects every one of them. Restart markers (DRI) are
handled by TJpgDec.

### `decode(target, scale=0, x=0, y=0, *, stride=None) -> None`

Decodes the image opened by the last `open()`. One-shot, like CP: the stream
is consumed, so `open()` again before the next `decode()` (RuntimeError
`decode() without open()` otherwise).

- `scale` 0..3 — downscale by 1, 1/2, 1/4, 1/8 (TJpgDec's `JD_USE_SCALE`).
  The decoded image is `(width >> scale) x (height >> scale)`; 1/8 is the
  cheap one (DC-only, no IDCT). Other values: ValueError.
- `x`, `y` (0..65535, the range of the image size itself) — where the
  decoded image's top-left corner lands, in pixels, in both target modes.
  Anything else: ValueError.
- `target` — either a **buffer** or a **callable**.

**Buffer target.** A writable buffer-protocol object (`bytearray`,
`memoryview`, a `framebuf.FrameBuffer`-compatible object) of native-order
RGB565 pixels, row-major. Rows are `stride` pixels apart (default: the
decoded width, i.e. a tight buffer of exactly
`(width >> scale) * (height >> scale) * 2` bytes). Before decoding, the
module checks `x + decoded_width <= stride` and that the last pixel written
fits in the buffer; otherwise ValueError naming the numbers, e.g.
`target too small: 320x240 at (0, 0) with stride 320 needs 153600 bytes,
buffer has 1024`. The fit is decided without computing
`(y + decoded_height - 1) * stride`, so a `stride` / `y` pair whose product
wraps `size_t` (32 bits on the MCU ports) is refused too (`... the last
pixel's offset overflows size_t`), never let through. Because the default
`stride` is the decoded width, `x > 0` always needs `stride` — the row
width of the target in pixels — and the error says so (`x + decoded width
(5 + 320) exceeds the default stride 320 (the decoded width): pass
stride=...`); `y > 0` needs only a buffer with enough rows. A rejected call
leaves the opened image in place, so `decode()` can be retried with a
better target without another `open()`. Each TJpgDec output block is
copied row by row into place.

**Callable target.** Called once per TJpgDec output block, in raster order:

```python
callback(x, y, w, h, mv)
```

`x, y` are the block's position (with `decode()`'s `x`, `y` added), `w, h`
its size, `mv` a read-only `memoryview('H')` of `w * h` native-order RGB565
pixels — `len(mv) == w * h`, `mv[0]` is the first pixel as an int, and
`bytes(mv)` / any buffer-protocol consumer (`display_drv.blit_rect`) sees
`w * h * 2` bytes. `mv` views TJpgDec's own output buffer and is valid only
until the callback returns; a reference kept past that reads a zero-length
view (the module truncates it after each decode). One memoryview object is
created per decoder and reused for every block, so the callback path
allocates nothing per block. Blocks are MCU-sized (8x8, 16x8 or 16x16 pixels
before scaling) and clipped at the right/bottom edge; together they cover
the decoded image exactly once. An exception raised inside the callback
propagates out of `decode()`; the decoder closes its source and needs a new
`open()`. `stride` is not accepted with a callable (TypeError).

**No byte swapping.** Pixels are native-order RGB565 (on little-endian
targets `0xF800` red is the bytes `00 F8`). Displays that want big-endian
RGB565 get it from `display_drv`, which byte-swaps itself; CP's
`RGB565_SWAPPED` output is CP's convention, not this module's.

## Errors

TJpgDec's `JRESULT` codes map to exceptions whose message starts with the
TJpgDec name, so a caller can tell them apart without module constants:

| `JRESULT` | Exception | When |
|-----------|-----------|------|
| `JDR_INP` | `ValueError` | input ended before SOI, or the JPEG is truncated (an I/O error on a stream raises `OSError(errno)` instead) |
| `JDR_FMT1` | `ValueError` | unsupported or malformed JPEG: corrupt entropy data, a missing DQT/DHT — **this is what a DHT-less MJPEG frame produces** |
| `JDR_FMT2` | `ValueError` | right format but not supported (unused by R0.03) |
| `JDR_FMT3` | `ValueError` | not baseline: **progressive** (SOF2), lossless, arithmetic, CMYK, or 4:1:1 / 4:4:0 chroma subsampling |
| `JDR_MEM1` | `MemoryError` | the image's tables do not fit the 3500-byte work area |
| `JDR_MEM2` | `MemoryError` | a segment is larger than TJpgDec's 512-byte input buffer (`JD_SZBUF`) |
| `JDR_PAR` | `ValueError` | parameter error (scale is validated before TJpgDec sees it) |
| `JDR_INTR` | `RuntimeError` | output function interrupted the decode (the module's output functions never do) |

A DHT-less frame (some cameras omit the standard Huffman tables and expect
the decoder to supply them) fails `open()` with `JDR_FMT1`; whether the
module should inject the default tables is an open decision recorded in the
vision doc, not something done silently here.

## Differences from CircuitPython

One, deliberate: `decode()` takes a buffer or a callable instead of a
`displayio.Bitmap`, and consequently has **no** `x1`/`y1`/`x2`/`y2` crop
window and no `skip_source_index` / `skip_dest_index`. Those are
`bitmaptools.blit` palette semantics that only make sense against a
`displayio.Bitmap`; the `stride` keyword and the callable target are the
replacements. Everything else — constructor, `open()` (positional-only,
str/buffer/stream), the `(width, height)` return, `scale` 0..3, `x`/`y`,
one-shot decode — follows CP's `shared-bindings/jpegio`.

Also unlike CP, TJpgDec here emits native-order RGB565 (CP patches its copy
to byte-swap for `RGB565_SWAPPED`); see above. And errors are typed
(`ValueError` / `MemoryError` / `RuntimeError`) rather than CP's uniform
`RuntimeError`.

## Build

- `jpegio.c` — the module (`MP_REGISTER_MODULE(MP_QSTR_jpegio, ...)`).
- `tjpgd/` — vendored TJpgDec: `tjpgd.c`, `tjpgd.h`, `tjpgdcnf.h`. The
  firmware's only TJpgDec, always compiled.
- `lvgl_decoder.c` / `lvgl_decoder.h` — the LVGL image decoder on that
  TJpgDec; compiled only beside `lvgl-micropython` (below).
- `micropython.mk` / `micropython.cmake` — glue, included unconditionally
  from displayif's root build files.

`tjpgdcnf.h` is CircuitPython's, verbatim: `JD_SZBUF 512`, `JD_FORMAT 1`
(RGB565), `JD_USE_SCALE 1`, `JD_TBLCLIP 1`, `JD_FASTDECODE 1`. `jpegio.c`
refuses to build with any other `JD_FORMAT`.

**With LVGL (D4 of the org's `docs/jpegio-vision.md`: displayif
self-detects).** LVGL's own TJPGD is off by config on MicroPython
(`LV_USE_TJPGD 0` in lvgl-bindings' `lv_conf.h`), so nothing clashes with
the vendored copy and LVGL has no JPEG decoder of its own; this module
supplies one. Make ports: when
`$(USER_C_MODULES)/lvgl-micropython/micropython.mk` exists — py.mk's own
usermod glob, one level under `USER_C_MODULES` — `micropython.mk` compiles
`lvgl_decoder.c` with `-DJPEGIO_LVGL_DECODER=1` and adds the bindings
checkout (`lv_conf.h`) and its `lvgl/` (`lvgl.h`) to the include path.
`JPEGIO_LVGL_BINDINGS_DIR` is derived the way `lvgl-micropython/micropython.mk`
derives `BINDINGS_DIR` (the sibling `lvgl-bindings`), honours a
`BINDINGS_DIR` given on the make command line, and can be overridden
itself; `JPEGIO_LVGL=0` / `1` forces the decision. CMake ports: the default
is the sibling `../lvgl-micropython/micropython.cmake` of this repo, or an
`lv_micropython` target an earlier entry of a semicolon-separated
`USER_C_MODULES` list already defined; `-DJPEGIO_LVGL=ON` plus
`-DJPEGIO_LVGL_BINDINGS_DIR=...` covers any other layout. Without the
sibling nothing links against LVGL — displayif's own clean-build CI is
LVGL-less and stays so — but the module still exports
`register_lvgl_decoder()` and `lvgl_decoders()`; see *Why these two names
are unconditional* below.

## NOTICE — TJpgDec

`tjpgd/tjpgd.c`, `tjpgd/tjpgd.h` and `tjpgd/tjpgdcnf.h` are ChaN's TJpgDec
R0.03 with patch1, taken from CircuitPython's `lib/tjpgd/src/` (which
imported it from http://elm-chan.org/fsw/tjpgd/00index.html), with two
deviations. First, the RGB565 store in `mcu_output` is ChaN's original
`*d++ = w;` where CircuitPython has `__builtin_bswap16(w)`. Second, the
`stdint.h` guard at the top of `tjpgd.h` tests for the header with
`__has_include` rather than for `_WIN32`: upstream's comment says it means
"a compiler without stdint.h", but mingw defines `_WIN32` *and* ships
`stdint.h`, where `uint32_t` is `unsigned int` rather than the fallback's
`unsigned long`, so every mingw cross-build failed with conflicting types
(displayif#25). The fallback typedefs are kept for the compiler the comment
was written for, MSVC before 2010. Both CircuitPython's copy and ChaN's
R0.03 still carry the platform test; LVGL's fork sidesteps it by including
`stdint.h` unconditionally. The files keep ChaN's license header:

> Copyright (C) 2021, ChaN, all right reserved.
>
> The TJpgDec module is a free software and there is NO WARRANTY.
> No restriction on use. You can use, modify and redistribute it for
> personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
> Redistributions of source code must retain the above copyright notice.

`jpegio.c` is a port of CircuitPython's `shared-bindings/jpegio` and
`shared-module/jpegio` (Copyright (c) 2023 Jeff Epler for Adafruit
Industries, MIT).

## LVGL image decoder

The decoder itself (`lvgl_decoder.c`) is compiled only beside
`lvgl-micropython` (see Build); the two Python names below exist on every
build. It is an ordinary LVGL image decoder named `"jpegio"`
(`lv_image_decoder_t.name`), created with `lv_image_decoder_create`, so
`lv.image(...).set_src(...)` and every other LVGL image path decode
baseline JPEGs through this module's TJpgDec:

```python
import lvgl as lv
import jpegio
lv.init()
jpegio.register_lvgl_decoder()     # needed only if `import jpegio` ran before lv.init()

with open("/sd/photo.jpg", "rb") as f:
    jpeg = f.read()
dsc = lv.image_dsc_t({"header": {"w": 320, "h": 240, "cf": lv.COLOR_FORMAT.RAW},
                      "data_size": len(jpeg), "data": jpeg})
lv.image(lv.screen_active()).set_src(dsc)
```

**Registration.** Two ways, both idempotent — registering twice never adds
a second decoder:

- On `import jpegio`, if LVGL is already initialised (`lv.init()` ran), the
  module registers itself. MicroPython runs a built-in module's `__init__`
  on every `import` statement (`MICROPY_MODULE_BUILTIN_INIT`, on by default
  at the "extra features" ROM level: unix, esp32, rp2, stm32, mimxrt,
  SAMD51; not on SAMD21's "basic" level).
- `jpegio.register_lvgl_decoder() -> bool` registers explicitly: `True`
  when this call added the decoder, `False` when it was already there,
  `RuntimeError` when LVGL is not initialised. Use it when `import jpegio`
  came before `lv.init()`, and after every `lv.deinit()` / `lv.init()` cycle
  (LVGL clears its decoder list on deinit). On a build with no decoder in it
  the call always raises `RuntimeError`, and the message names both the cause
  and the way out: *no lvgl-micropython sibling at build time (rebuild with
  `JPEGIO_LVGL=1` to force it)*.

`jpegio.lvgl_decoders() -> tuple` lists LVGL's registered image decoders by
name in the order LVGL consults them — `("jpegio", "LODEPNG", "BIN")` once
registered (a new decoder goes to the head of LVGL's list), `()` before
`lv.init()`. A diagnostic: the binding cannot walk that list from Python
(MicroPython's builtin-method self check refuses
`lv.image_decoder_t.get_next(None)`), and it is how the test proves that
registering twice adds nothing. On a build with no decoder in it, `()`
always.

**Why these two names are unconditional.** They are module attributes on
every build, and only `lvgl_decoder.c` is conditional. Partly so the API
never silently disappears when the sibling scan misses — a decoder-less
build says so, in an exception, instead of raising `AttributeError` — and
partly because the CMake ports cannot compile them any other way.
MicroPython's `py/mkrules.cmake` builds the QSTR-extraction preprocessor
flags from the *port target's* own `COMPILE_DEFINITIONS`
(`get_target_property(MICROPY_CPP_DEF ${MICROPY_TARGET} COMPILE_DEFINITIONS)`),
so a usermod's `INTERFACE` define — which is how `micropython.cmake`
passes `JPEGIO_LVGL_DECODER=1` — never reaches `makeqstrdefs.py`. An
`MP_QSTR_*` referenced only inside
`#if JPEGIO_LVGL_DECODER` is therefore never collected on a CMake port, and
the build fails with `error: 'MP_QSTR_register_lvgl_decoder' undeclared`.
In MicroPython v1.28.0 that means esp32 and rp2, the only two ports with a
`CMakeLists.txt`; samd, mimxrt and stm32 carry CMake glue here for the day
they gain one, and build through their Makefile today. (The Makefile ports
put the define in `CFLAGS_USERMOD`, which `py/py.mk` folds into `CFLAGS`,
and their QSTR pass reads `CFLAGS`, which is why unix builds never showed
it.) `MP_QSTR___init__` survives inside the `#if` only because it is one of
MicroPython's static-pool qstrs (`py/makeqstrdata.py`), not because
anything here collects it, so any *new* `MP_QSTR_*` put under that `#if`
brings the break straight back. Do not re-conditionalise either function.

**Sources.** A variable source (`lv.image_dsc_t`) whose `data` starts with
the SOI marker `FF D8`, or a file (through a registered `lv.fs_drv_t`, e.g.
lvgl-bindings' `fs_driver.py`) whose first two bytes are `FF D8` — the
extension is not consulted. Nothing more is sniffed: LVGL's own `is_jpg()`
wants a 10-byte JFIF signature and so rejects every UVC MJPEG frame; here
`jd_prepare` judges, for both source kinds, in the decoder's *info* step.
So the image size LVGL sees is the stream's own — the `w`/`h` in your
`lv.image_dsc_t` header are not echoed (LVGL's TJPGD echoed them) — and a
pixel buffer that merely starts `FF D8` fails `jd_prepare` and falls
through to LVGL's built-in decoder. `cf` in the header should be
`lv.COLOR_FORMAT.RAW` (or `RAW_ALPHA`), the convention for "not pixels";
`data_size` must be the JPEG's length.

**Output.** `LV_COLOR_FORMAT_RGB565`, native byte order, stride `w * 2`,
the same pixels `JpegDecoder.decode()` produces (the LVGL test asserts
jpegio's own golden digests through `lv.image`). LVGL's software renderer
blends RGB565 into an RGB565 display by row copy, its fast path.

**Memory.** The decoder decodes the whole image in its *open* step into a
draw buffer of `w * h * 2` bytes (from LVGL's image-cache allocator, i.e.
the MicroPython heap), which LVGL frees at *close* — every refresh, unless
LVGL's image cache is on (`LV_CACHE_DEF_SIZE`, 0 in lvgl-bindings). That is
what LVGL's own full-decode decoders (`lv_lodepng.c`) do. It is not the
block-wise, no-frame-buffer path `JpegDecoder.decode(callable)` offers:
LVGL's `lv_tjpgd.c` decodes MCU by MCU only through exports it added to its
private TJpgDec (`jd_mcu_load`, `jd_mcu_output`, `jd_restart`), which ChaN's
R0.03 does not have, and `jd_decomp` is all-or-nothing. A camera preview
that must not hold a frame buffer uses `jpegio.JpegDecoder` and
`display_drv.blit_rect` directly. Scaling (`scale` 1..3) is a
`JpegDecoder` feature, not exposed to LVGL (its zoom is its own).

**Errors.** A source the decoder does not recognise in its *info* step (no
`FF D8`, or `jd_prepare` refuses the headers: progressive, DHT-less, not a
JPEG at all) is not claimed (`LV_RESULT_INVALID`), so LVGL tries its other
decoders and, when none claims it, draws nothing. A source whose *info*
succeeded but whose *open* fails (a truncated scan, a decode error inside
the frame, no memory for the `w * h * 2` buffer) also draws nothing, but
LVGL does **not** try another decoder after a failed open — its
`lv_image_decoder.c` assumes a decoder that could read the info can open
the image — so the failure is final for that draw and logged at
`LV_LOG_WARN`. No Python exception is raised from inside LVGL's draw path.
