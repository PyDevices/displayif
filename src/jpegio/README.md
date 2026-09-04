# jpegio — baseline JPEG decoder (CircuitPython API, TJpgDec R0.03)

`import jpegio` gives MicroPython the same `JpegDecoder` CircuitPython ships
natively, so one script decodes on both — CircuitPython uses its own, a
MicroPython firmware built with displayif uses this one. It exists so a
`displaydev` firmware can show a camera's MJPEG frames (or a JPEG on flash)
without carrying LVGL for its decoder (org `docs/jpegio-vision.md`).

Platform-neutral C: built on every port from the root `micropython.mk` /
`micropython.cmake`. Output is native-order RGB565, block by block, straight
from TJpgDec — no full-frame buffer inside the module.

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
- `tjpgd/` — vendored TJpgDec: `tjpgd.c`, `tjpgd.h`, `tjpgdcnf.h`.
- `micropython.mk` / `micropython.cmake` — glue, included unconditionally
  from displayif's root build files.

`tjpgdcnf.h` is CircuitPython's, verbatim: `JD_SZBUF 512`, `JD_FORMAT 1`
(RGB565), `JD_USE_SCALE 1`, `JD_TBLCLIP 1`, `JD_FASTDECODE 1`. `jpegio.c`
refuses to build with any other `JD_FORMAT`.

**Phase 2 hook — `JPEGIO_VENDOR_TJPGD`** (Make and CMake, default `1`):
set it to `0` to leave `tjpgd/tjpgd.c` out of the build and link against a
TJpgDec another usermod already provides (LVGL's, once `lv_tjpgd.c` honours
`JD_FORMAT` and LVGL's `tjpgdcnf.h` matches this one). The header on the
include path must then describe the same configuration, because `JDEC`'s
layout depends on `JD_FASTDECODE`. Nothing detects LVGL yet; today a
firmware that links both LVGL (with `LV_USE_TJPGD 1`) and this module gets
two `jd_prepare` definitions and must pass `JPEGIO_VENDOR_TJPGD=0` by hand
— that is exactly the Phase 2 work.

## NOTICE — TJpgDec

`tjpgd/tjpgd.c`, `tjpgd/tjpgd.h` and `tjpgd/tjpgdcnf.h` are ChaN's TJpgDec
R0.03 with patch1, taken from CircuitPython's `lib/tjpgd/src/` (which
imported it from http://elm-chan.org/fsw/tjpgd/00index.html), with one line
changed: the RGB565 store in `mcu_output` is ChaN's original `*d++ = w;`
where CircuitPython has `__builtin_bswap16(w)`. The files keep ChaN's
license header:

> Copyright (C) 2021, ChaN, all right reserved.
>
> The TJpgDec module is a free software and there is NO WARRANTY.
> No restriction on use. You can use, modify and redistribute it for
> personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
> Redistributions of source code must retain the above copyright notice.

`jpegio.c` is a port of CircuitPython's `shared-bindings/jpegio` and
`shared-module/jpegio` (Copyright (c) 2023 Jeff Epler for Adafruit
Industries, MIT).

## Two copies today, one tomorrow

In a firmware that also carries LVGL (with `LV_USE_TJPGD`), LVGL links its own TJpgDec and exports
the same `jd_prepare`/`jd_decomp` symbols. `tjpgd/tjpgd.h` prefixes this module's two entry points
(`jpegio_jd_*`) so both copies link — about 6 KB of duplicated code, deliberately temporary. Phase 2
of `pydevices/docs/jpegio-vision.md` unifies the TJpgDec config (this module's, CP's) across LVGL and
jpegio and sets `JPEGIO_VENDOR_TJPGD=0` when LVGL is present, leaving one copy.
