"""jpegio -- unix MicroPython tests against the corpus and the Pillow reference.

The Phase 1 gate of the org's docs/jpegio-vision.md, checked mechanically:

  1. every baseline corpus frame opens to reference.json's size and decodes
     at scales 0..3 into a tight RGB565 buffer whose per-channel means sit
     within a tolerance of the Pillow reference (the measured error is
     printed beside the tolerance); TJpgDec's own digest per frame/scale is
     kept in frames/golden_tjpgd.json -- recorded on the first run (or with
     --record), asserted on every later one;
  2. the callback target covers the decoded image exactly once at every
     scale, in raster order, and reassembles to the buffer target's bytes;
  3. the buffer target honours x / y / stride, leaves the rest of a larger
     buffer alone, and refuses bad geometry with the numbers in the message;
  4. the DHT-less frame is refused with JDR_FMT1 -- the Phase 1 contract;
  5. the progressive frame, truncated frames, a bare SOI, and non-JPEG bytes
     all raise (never crash), and open() rejects non-source objects;
  6. the restart-marker frames decode to the baseline frame's digests;
  7. real c920e_* camera frames open, decode at every scale and are digested;
  8. one decoder object re-opens across frames and source kinds;
  9. 50 decodes do not trend gc.mem_free() down.

Run from the displayif root (or anywhere: paths come from __file__) with the
unix MicroPython built with this repo on USER_C_MODULES:

    ./micropython/ports/unix/build-standard/micropython tests/jpegio/test_jpegio.py [--record]

Plain asserts; needs only the built interpreter and frames/.  Pillow's side is
frozen in frames/reference.json by reference.py, so Pillow is not needed here.
"""
import binascii
import gc
import hashlib
import io
import json
import math
import os
import sys
import time

import jpegio

HERE = __file__.rsplit("/", 1)[0] if "/" in __file__ else "."
FRAMES = HERE + "/frames"
REFERENCE = FRAMES + "/reference.json"
GOLDEN = FRAMES + "/golden_tjpgd.json"
SCALES = (0, 1, 2, 3)
RECORD = "--record" in sys.argv[1:]

# --- fidelity tolerance ------------------------------------------------------
#
# reference.json holds per-channel means of Pillow's (libjpeg-turbo) decode,
# box-averaged the way TJpgDec's mcu_output() scales.  TJpgDec is not
# bit-identical to libjpeg: its fixed-point IDCT rounds differently, it
# replicates chroma where libjpeg interpolates ("fancy upsampling"), and at
# scale 3 it emits each block's DC value (chroma from the whole 16x16 MCU at
# 4:2:0) instead of a box average.  Per pixel those differences are large on
# the synthetic pattern's hard chroma edges (up to ~150 levels at scale 0,
# ~190 at scale 3, measured against Pillow while writing this test) but they
# alternate in sign, so the means stay close.  Measured |mean error| on this
# corpus, TJpgDec R0.03 with CircuitPython's tjpgdcnf.h, 2026-09-03:
#
#     scales 0..2, every frame:                max 1.27  (odd_size_37x29 s2, blue)
#     scale 3, 1200 decoded px (320x240 -> 40x30): max 1.59  (blue)
#     scale 3,   48 decoded px (64x48 -> 8x6):     max 1.67  (blue)
#     scale 3,   12 decoded px (37x29 -> 4x3):     max 9.33  (red)
#
# Tolerance: 2.0 levels at scales 0..2 (1.6x the measured worst; one RGB565
# step is 4 levels in green, 8 in red/blue, so a systematic one-step shift in
# any channel trips it).  At scale 3: 2.0 + 32 / sqrt(n), n = decoded pixels
# (2.9 at 40x30, 6.6 at 8x6, 11.2 at 4x3): DC-only chroma is wrong by a bounded
# amount per pixel and averages out as 1/sqrt(n), so a tiny image needs more
# room.  Means alone are a weak witness (a scrambled block order keeps them; a
# swapped R/B on this near-symmetric pattern moves them by ~1.4), which is why
# they are paired with the golden digests and the positional checks below.
MEAN_TOL = 2.0
MEAN_TOL_SCALE3_K = 32.0


def mean_tolerance(scale, npixels):
    if scale < 3:
        return MEAN_TOL
    return MEAN_TOL + MEAN_TOL_SCALE3_K / math.sqrt(npixels)


# --- positional checks on the synthetic pattern -----------------------------
#
# make_corpus.py draws: top 40 % eight colour bars; 40-70 % a gradient; bottom
# band = 1-px checker (left half) and a red|blue split (right half).  Sampling
# the centre of each bar (and of the red / blue halves) pins channel identity
# and block placement, which means cannot.  Measured deviation at those
# samples: 0 on every frame/scale the rules below admit, 8 on the 64x48 split.
# A 4:2:0 chroma sample spans 2 source px (a 16x16 MCU at scale 3), so bars
# narrower than 8 source px (16 at scale 3) are not sampled: their centre
# chroma is a mix of two bars, by construction of the format, not by defect.
BARS = ((255, 255, 255), (255, 255, 0), (0, 255, 255), (0, 255, 0),
        (255, 0, 255), (255, 0, 0), (0, 0, 255), (0, 0, 0))
POS_TOL = 24
BAR_MIN_SRC_PX = {0: 8, 1: 8, 2: 8, 3: 16}


def pattern_geometry(w, h):
    y_bars = max(1, (h * 2) // 5)
    y_grad = max(y_bars + 1, (h * 7) // 10)
    return y_bars, y_grad


def rgb565_to_888(v):
    return ((v >> 8) & 0xF8, (v >> 3) & 0xFC, (v << 3) & 0xF8)


def pixel(buf, stride, x, y):
    i = 2 * (y * stride + x)
    return rgb565_to_888(buf[i] | (buf[i + 1] << 8))


def expect_rgb(rgb):
    return (rgb[0] & 0xF8, rgb[1] & 0xFC, rgb[2] & 0xF8)


def luma(rgb):
    return (rgb[0] * 299 + rgb[1] * 587 + rgb[2] * 114) // 1000


# --- helpers -----------------------------------------------------------------

def sha256_hex(buf):
    return binascii.hexlify(hashlib.sha256(buf).digest()).decode()


def means565(buf, n):
    r = g = b = 0
    for i in range(0, 2 * n, 2):
        lo = buf[i]
        hi = buf[i + 1]
        r += hi & 0xF8
        g += ((hi & 0x07) << 5) | ((lo >> 5) << 2)
        b += (lo << 3) & 0xF8
    return (r / n, g / n, b / n)


def read_file(name):
    with open(FRAMES + "/" + name, "rb") as f:
        return f.read()


def expect_raises(exc, fn, *needles):
    """fn() must raise exc (or a subclass) whose str() contains every needle."""
    try:
        fn()
    except exc as e:
        msg = str(e)
        for needle in needles:
            assert needle in msg, "expected %r in %s: %s" % (needle, type(e).__name__, msg)
        return e
    except Exception as e:  # noqa: BLE001
        raise AssertionError("expected %s, got %s: %s" % (exc.__name__, type(e).__name__, e))
    raise AssertionError("expected %s, nothing raised" % exc.__name__)


def decode_tight(dec, source, scale):
    w, h = dec.open(source)
    dw, dh = w >> scale, h >> scale
    buf = bytearray(dw * dh * 2)
    dec.decode(buf, scale)
    return dw, dh, buf


def decode_via_callback(dec, source, scale):
    """decode(callable): coverage mask + raster-order check; returns the reassembled buffer."""
    w, h = dec.open(source)
    dw, dh = w >> scale, h >> scale
    mask = bytearray(dw * dh)
    out = bytearray(dw * dh * 2)
    state = [0, -1, -1, None]   # blocks, last y, last x, last view

    def cb(x, y, bw, bh, mv):
        assert bw > 0 and bh > 0 and 0 <= x and 0 <= y, (x, y, bw, bh)
        assert x + bw <= dw and y + bh <= dh, "block %dx%d at (%d, %d) outside %dx%d" % (bw, bh, x, y, dw, dh)
        assert len(mv) == bw * bh, (len(mv), bw, bh)
        raw = bytes(mv)
        assert len(raw) == bw * bh * 2
        assert y > state[1] or (y == state[1] and x > state[2]), "not raster order: (%d, %d) after (%d, %d)" % (x, y, state[2], state[1])
        state[1], state[2] = y, x
        for r in range(bh):
            o = (y + r) * dw + x
            assert not any(mask[o:o + bw]), "pixel written twice in row %d, x %d..%d" % (y + r, x, x + bw)
            mask[o:o + bw] = b"\x01" * bw
            out[2 * o:2 * (o + bw)] = raw[2 * r * bw:2 * (r + 1) * bw]
        state[0] += 1
        state[3] = mv

    dec.decode(cb, scale)
    gaps = mask.count(b"\x00") if hasattr(mask, "count") else sum(1 for m in mask if not m)
    assert gaps == 0, "%d of %d pixels never written at scale %d" % (gaps, dw * dh, scale)
    assert state[3] is not None and len(state[3]) == 0, "block view must be emptied after decode()"
    return out, state[0]


def check_pattern(name, scale, buf, dw, dh, w, h):
    """Positional checks on the synthetic frames; returns the number of samples checked."""
    y_bars, y_grad = pattern_geometry(w, h)
    band = y_bars >> scale
    gray = name.startswith("grayscale_")
    checked = 0
    bar_src_px = w / 8
    if band >= 2 and dw >= 8 and (gray or bar_src_px >= BAR_MIN_SRC_PX[scale]):
        for i, rgb in enumerate(BARS):
            x = int((i + 0.5) * dw / 8)
            got = pixel(buf, dw, x, band // 2)
            if gray:
                L = luma(rgb)
                want = expect_rgb((L, L, L))
            else:
                want = expect_rgb(rgb)
            err = max(abs(got[c] - want[c]) for c in range(3))
            assert err <= POS_TOL, "%s s%d bar %d at (%d, %d): got %s want %s" % (name, scale, i, x, band // 2, got, want)
            checked += 1
    if scale < 3 and w >= 64 and not gray:
        y = ((y_grad >> scale) + dh) // 2
        for xf, rgb in ((5 / 8, (255, 0, 0)), (7 / 8, (0, 0, 255))):
            x = int(dw * xf)
            got = pixel(buf, dw, x, y)
            want = expect_rgb(rgb)
            err = max(abs(got[c] - want[c]) for c in range(3))
            assert err <= POS_TOL, "%s s%d split at (%d, %d): got %s want %s" % (name, scale, x, y, got, want)
            checked += 1
    if gray:
        # TJpgDec builds R=G=B=Y for a one-component image: after RGB565
        # packing r == b exactly and g - r is 0 or 4 on every pixel.
        for i in range(0, 2 * dw * dh, 2):
            r, g, b = rgb565_to_888(buf[i] | (buf[i + 1] << 8))
            assert r == b and 0 <= g - r <= 4, "%s s%d pixel %d not grey: %s" % (name, scale, i // 2, (r, g, b))
        checked += dw * dh
    return checked


def load_golden():
    try:
        with open(GOLDEN) as f:
            return json.load(f)["frames"]
    except OSError:
        return None


def write_golden(digests):
    lines = ['{', ' "_about": [',
             '  "TJpgDec\'s own sha256 per corpus frame and scale: the RGB565 buffer decode() fills",',
             '  "(native-order uint16, row-major, tight), written by test_jpegio.py --record.",',
             '  "A parity digest, not a fidelity claim: its evidence is the Pillow mean/positional",',
             '  "check test_jpegio.py prints beside it.  Re-record deliberately after a TJpgDec or",',
             '  "tjpgdcnf.h change, never to make a red run green."',
             ' ],',
             ' "decoder": "TJpgDec R0.03 patch1, CircuitPython tjpgdcnf.h (JD_FORMAT 1, JD_USE_SCALE 1, JD_TBLCLIP 1, JD_FASTDECODE 1)",',
             ' "frames": {']
    names = sorted(digests)
    for i, name in enumerate(names):
        lines.append('  "%s": {' % name)
        for s in SCALES:
            lines.append('   "%d": "%s"%s' % (s, digests[name][s], "," if s != SCALES[-1] else ""))
        lines.append('  }%s' % ("," if i != len(names) - 1 else ""))
    lines += [' }', '}', '']
    with open(GOLDEN, "w") as f:
        f.write("\n".join(lines))


# --- the tests -----------------------------------------------------------------

def main():
    t0 = time.time()
    with open(REFERENCE) as f:
        ref = json.load(f)
    frames = ref["frames"]
    dec = jpegio.JpegDecoder()
    golden = None if RECORD else load_golden()
    digests = {}
    refused = {}
    worst = {}
    samples = 0

    # 1. sizes, tight decode at every scale, means vs Pillow, digests vs golden
    print("== 1. corpus frames: size, tight decode, means vs Pillow (tolerance beside), digest")
    for name in sorted(frames):
        entry = frames[name]
        try:
            w, h = dec.open(FRAMES + "/" + name)
        except ValueError as e:
            refused[name] = str(e)
            print("%-36s open() refused: %s" % (name, e))
            continue
        assert (w, h) == (entry["width"], entry["height"]), (name, w, h)
        assert (dec.width, dec.height) == (w, h)
        digests[name] = {}
        for s in SCALES:
            r = entry["scales"][str(s)]
            dw, dh, buf = decode_tight(dec, FRAMES + "/" + name, s)
            assert (dw, dh) == (r["width"], r["height"]), (name, s, dw, dh)
            assert len(buf) == dw * dh * 2
            m = means565(buf, dw * dh)
            err = [m[c] - r["mean_rgb565"][c] for c in range(3)]
            tol = mean_tolerance(s, dw * dh)
            worst[s] = max(worst.get(s, 0.0), max(abs(e) for e in err))
            digest = sha256_hex(buf)
            digests[name][s] = digest
            status = "recorded"
            if golden is not None:
                g = golden.get(name, {}).get(str(s))
                status = "golden match" if g == digest else "GOLDEN MISMATCH (golden %s)" % (g[:16] if g else None)
            print("%-36s s%d %4dx%-4d mean err r=%+6.3f g=%+6.3f b=%+6.3f  tol %5.2f  sha256 %s.. %s" % (
                name, s, dw, dh, err[0], err[1], err[2], tol, digest[:16], status))
            for c, e in enumerate(err):
                assert abs(e) <= tol, "%s s%d channel %d mean error %.3f exceeds %.2f" % (name, s, c, e, tol)
            if golden is not None:
                assert status == "golden match", "%s s%d: TJpgDec digest changed" % (name, s)
            if name.startswith("c920e_"):
                print("%-36s s%d means r=%.2f g=%.2f b=%.2f (Pillow r=%.2f g=%.2f b=%.2f)" % (
                    name, s, m[0], m[1], m[2], r["mean_rgb565"][0], r["mean_rgb565"][1], r["mean_rgb565"][2]))
            else:
                samples += check_pattern(name, s, buf, dw, dh, w, h)
    print("worst |mean err| per scale: " + "  ".join("s%d=%.3f" % (s, worst[s]) for s in SCALES)
          + "  (tolerance %.1f at s0..2, %.1f + %.0f/sqrt(n) at s3)" % (MEAN_TOL, MEAN_TOL, MEAN_TOL_SCALE3_K))
    print("positional samples checked on the synthetic frames: %d" % samples)
    assert samples > 0
    if golden is None:
        write_golden(digests)
        print("golden: RECORDED %d digests to %s" % (sum(len(v) for v in digests.values()), GOLDEN))
    else:
        extra = sorted(set(golden) - set(digests))
        assert not extra, "golden has frames this run did not decode: %s" % extra
        print("golden: %d digests match %s" % (sum(len(v) for v in digests.values()), GOLDEN))

    # 6. restart markers and marker order must not change a pixel
    print("== 6. restart / marker-order frames vs baseline_jfif_320x240.jpg")
    base = digests["baseline_jfif_320x240.jpg"]
    for other in ("restart_dri_320x240.jpg", "dri_first_mjpeg_style_320x240.jpg"):
        for s in SCALES:
            same = digests[other][s] == base[s]
            print("%-36s s%d %s" % (other, s, "== baseline" if same else "FINDING: differs  %s vs baseline %s" % (digests[other][s], base[s])))
            assert same, "%s decodes differently from the baseline at scale %d" % (other, s)

    # 7. real camera frames: every c920e_*.jpg in frames/ (means printed above when referenced)
    print("== 7. c920e camera frames")
    cams = sorted(n for n in os.listdir(FRAMES) if n.startswith("c920e_") and n.endswith(".jpg"))
    assert cams, "no c920e_*.jpg in %s" % FRAMES
    for name in cams:
        if name in digests:
            print("%-36s referenced: decoded and digested at every scale (see section 1)" % name)
            continue
        w, h = dec.open(FRAMES + "/" + name)
        digests[name] = {}
        for s in SCALES:
            dw, dh, buf = decode_tight(dec, FRAMES + "/" + name, s)
            m = means565(buf, dw * dh)
            digests[name][s] = sha256_hex(buf)
            print("%-36s s%d %4dx%-4d means r=%.2f g=%.2f b=%.2f sha256 %s.. (not in reference.json)" % (
                name, s, dw, dh, m[0], m[1], m[2], digests[name][s][:16]))

    # 2. callback path: exactly-once coverage at every scale, same bytes as the buffer path
    print("== 2. callback coverage")
    for name in sorted(digests):
        src = read_file(name)
        for s in SCALES:
            out, blocks = decode_via_callback(dec, src, s)
            assert sha256_hex(out) == digests[name][s], "%s s%d: callback bytes differ from buffer path" % (name, s)
            print("%-36s s%d %4d blocks, every pixel once, reassembled == buffer path" % (name, s, blocks))
    # the callback path allocates nothing per block (README): a fixed-arity
    # Python function costs no heap per call, so the whole decode should too.
    full = read_file("baseline_jfif_320x240.jpg")

    def sink(x, y, w, h, mv):
        pass

    dec.open(full)
    dec.decode(sink, 1)
    dec.open(full)
    gc.collect()
    a0 = gc.mem_alloc()
    dec.decode(sink, 0)
    grew = gc.mem_alloc() - a0
    print("callback decode of 320x240 at scale 0 (300 blocks) allocated %d bytes" % grew)
    assert grew < 300, "per-block allocation on the callback path"
    # a raise inside the callback propagates and consumes the open()
    dec.open(full)

    class Stop(Exception):
        pass

    def stop(x, y, w, h, mv):
        raise Stop("at (%d, %d)" % (x, y))

    expect_raises(Stop, lambda: dec.decode(stop), "at (0, 0)")
    expect_raises(RuntimeError, lambda: dec.decode(bytearray(8)), "decode() without open()")
    expect_raises(TypeError, lambda: (dec.open(full), dec.decode(sink, stride=5)), "stride")

    # 3. buffer path with x, y, stride; untouched region stays untouched; bad geometry
    print("== 3. buffer target: offsets, stride, untouched region, errors")
    for name, s in (("baseline_jfif_64x48.jpg", 0), ("odd_size_37x29.jpg", 0), ("baseline_jfif_320x240.jpg", 1), ("c920e_320x240_dri.jpg", 2)):
        src = read_file(name)
        dw, dh, tight = decode_tight(dec, src, s)
        x, y, stride, rows = 5, 3, dw + 13, dh + 7
        big = bytearray(b"\xa5" * (stride * rows * 2))
        want = bytearray(big)
        for r in range(dh):
            o = ((y + r) * stride + x) * 2
            want[o:o + dw * 2] = tight[r * dw * 2:(r + 1) * dw * 2]
        dec.open(src)
        dec.decode(big, s, x, y, stride=stride)
        assert big == want, "%s s%d: offset decode wrote outside or wrote wrong pixels" % (name, s)
        # keyword form, and a memoryview slice as the target
        big2 = bytearray(b"\xa5" * (stride * rows * 2 + 6))
        dec.open(src)
        dec.decode(memoryview(big2)[6:], scale=s, x=x, y=y, stride=stride)
        assert big2[6:] == want and big2[:6] == b"\xa5" * 6
        print("%-36s s%d %dx%d at (%d, %d) stride %d into %d rows: region exact, %d other bytes untouched" % (
            name, s, dw, dh, x, y, stride, rows, len(big) - dw * dh * 2))
    w, h = dec.open(full)
    n = w * h * 2
    expect_raises(ValueError, lambda: dec.decode(bytearray(1000)), "320x240", "153600", "1000")
    expect_raises(ValueError, lambda: dec.decode(bytearray(n), 0, 1, 0), "1 + 320", "320")
    expect_raises(ValueError, lambda: dec.decode(bytearray(n), 0, 0, 1), "(0, 1)", "154240", "153600")
    expect_raises(ValueError, lambda: dec.decode(bytearray(n), 0, 0, 0, stride=319), "320", "319")
    expect_raises(ValueError, lambda: dec.decode(bytearray(n), 0, 0, 0, stride=-5), "-5")
    expect_raises(ValueError, lambda: dec.decode(bytearray(n), 0, -1, 0), "-1")
    expect_raises(ValueError, lambda: dec.decode(bytearray(n), 0, 0, -2), "-2")
    expect_raises(ValueError, lambda: dec.decode(bytearray(n), 4), "4")
    expect_raises(ValueError, lambda: dec.decode(bytearray(n), -1), "-1")
    expect_raises(TypeError, lambda: dec.decode(b"\x00" * n))
    # a rejected decode() does not consume the open(): retry without re-opening
    buf = bytearray(n)
    dec.decode(buf)
    assert sha256_hex(buf) == digests["baseline_jfif_320x240.jpg"][0]
    print("bad geometry: 10 rejections named their numbers; the open() survived them and decoded")

    # 4. the DHT-less frame: Phase 1 refuses it (JDR_FMT1); nothing is injected
    print("== 4. nodht_320x240.jpg")
    # The contract Phase 1 chose (src/jpegio/README.md, "Errors"): a frame
    # without Huffman tables fails open() with JDR_FMT1; injecting the T.81
    # K.3 tables is the open decision in docs/jpegio-vision.md, not something
    # this module does silently.  If a later phase injects them, this section
    # becomes: open() -> (320, 240) and digests == baseline_jfif_320x240.jpg.
    assert "nodht_320x240.jpg" in refused, "nodht frame decoded -- the module injected tables? update section 4"
    assert sorted(refused) == ["nodht_320x240.jpg"], "unexpected refusals: %s" % refused
    e = expect_raises(ValueError, lambda: dec.open(FRAMES + "/nodht_320x240.jpg"), "JDR_FMT1", "unsupported or malformed")
    expect_raises(RuntimeError, lambda: dec.decode(bytearray(8)), "decode() without open()")
    print("refused at open(): %s" % e)

    # 5. refusals that must raise, never crash
    print("== 5. progressive, truncated, bare SOI, non-JPEG, non-source objects")
    e = expect_raises(ValueError, lambda: dec.open(FRAMES + "/progressive_320x240.jpg"), "JDR_FMT3", "progressive")
    print("progressive: %s" % e)
    rst = read_file("restart_dri_320x240.jpg")
    half = rst[:len(rst) // 2]
    assert dec.open(half) == (320, 240)      # headers are intact, the scan is not
    e = expect_raises(ValueError, lambda: dec.decode(bytearray(n)), "JDR_INP")
    print("truncated scan (bytes, %d of %d): decode() -> %s" % (len(half), len(rst), e))
    assert dec.open(io.BytesIO(half)) == (320, 240)
    expect_raises(ValueError, lambda: dec.decode(lambda *a: None), "JDR_INP")
    expect_raises(ValueError, lambda: dec.open(rst[:100]), "JDR_INP")
    print("truncated headers (100 bytes): open() refused")
    for label, src in (("bare SOI", b"\xff\xd8"), ("b'hello'", b"hello"), ("empty", b"")):
        e = expect_raises(ValueError, lambda: dec.open(src), "JDR_INP")
        expect_raises(RuntimeError, lambda: dec.decode(bytearray(8)), "decode() without open()")
        print("%s: open() -> %s; no decode possible" % (label, e))
    expect_raises(OSError, lambda: dec.open(FRAMES + "/does_not_exist.jpg"))

    class Readable:
        def read(self, n):
            return b""

    for label, src in (("int", 5), ("None", None), ("list", [1, 2]), ("float", 1.5),
                       ("object()", object()), ("Python read()", Readable()),
                       ("text file", open(FRAMES + "/README.md"))):
        expect_raises(TypeError, lambda: dec.open(src), "source must be")
    print("non-source objects (int, None, list, float, object, a Python read(), a text file): TypeError")
    expect_raises(RuntimeError, lambda: jpegio.JpegDecoder().width, "before open()")

    # 8. one decoder, many frames and source kinds
    print("== 8. re-open: frames in sequence, path / bytes / file / BytesIO / memoryview")
    order = ("baseline_jfif_64x48.jpg", "baseline_jfif_320x240.jpg", "odd_size_37x29.jpg", "grayscale_64x48.jpg", "c920e_320x240_dri.jpg")
    for name in order:
        for s in (0, 2):
            dw, dh, buf = decode_tight(dec, FRAMES + "/" + name, s)
            assert sha256_hex(buf) == digests[name][s], "%s s%d after other frames: digest changed" % (name, s)
            assert (dec.width, dec.height) == (frames[name]["width"], frames[name]["height"])
    print("sequence %s: digests unchanged" % " -> ".join(order))
    with open(FRAMES + "/baseline_jfif_320x240.jpg", "rb") as fobj:
        for label, src in (("path", FRAMES + "/baseline_jfif_320x240.jpg"), ("bytes", full), ("bytearray", bytearray(full)),
                           ("memoryview", memoryview(full)), ("file", fobj), ("BytesIO", io.BytesIO(full))):
            dw, dh, buf = decode_tight(dec, src, 1)
            assert sha256_hex(buf) == digests["baseline_jfif_320x240.jpg"][1], label
            print("%-10s -> same digest at scale 1" % label)
    # open() twice: the second image wins; a failed open() clears the first
    dec.open(FRAMES + "/baseline_jfif_64x48.jpg")
    assert dec.open(full) == (320, 240)
    buf = bytearray(n)
    dec.decode(buf)
    assert sha256_hex(buf) == digests["baseline_jfif_320x240.jpg"][0]
    dec.open(full)
    expect_raises(ValueError, lambda: dec.open(b"hello"))
    expect_raises(RuntimeError, lambda: dec.decode(buf), "decode() without open()")
    print("second open() supersedes the first; a failed open() leaves nothing to decode")

    # 9. memory: 50 decodes into the same buffer must not trend mem_free down
    print("== 9. memory over 50 decodes of 320x240 at scale 0")
    buf = bytearray(n)
    dec.open(full)
    dec.decode(buf)
    gc.collect()
    f0 = gc.mem_free()
    for _ in range(50):
        dec.open(full)
        dec.decode(buf)
    f1 = gc.mem_free()
    gc.collect()
    f2 = gc.mem_free()
    for _ in range(50):
        dec.open(full)
        dec.decode(buf)
    gc.collect()
    f3 = gc.mem_free()
    print("mem_free before %d, after 50 (uncollected) %d, collected %d, after 100 collected %d" % (f0, f1, f2, f3))
    print("uncollected garbage per open()+decode(): %d bytes" % ((f0 - f1) // 50))
    assert f0 - f1 < 50 * 512, "more than 512 bytes of garbage per decode"
    assert f2 >= f0 - 1024 and f3 >= f2 - 1024, "mem_free trends down across decodes"

    print("jpegio tests passed: %d frames, %d digests, %.2f s" % (
        len(digests), sum(len(v) for v in digests.values()), time.time() - t0))


main()
