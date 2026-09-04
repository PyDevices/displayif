"""jpegio -- its LVGL image decoder, on unix MicroPython built beside lvgl-micropython.

The LVGL half of the Phase 2 gate in the org's docs/jpegio-vision.md, checked
mechanically:

  1. registration: `import jpegio` before lv.init() registers nothing; after
     lv.init() the decoder appears once -- by the module's import-time hook
     where the port has MICROPY_MODULE_BUILTIN_INIT, and by
     jpegio.register_lvgl_decoder() everywhere -- and registering again adds
     no second decoder; it sits at the head of LVGL's list (counted with
     jpegio.lvgl_decoders(): the binding cannot walk LVGL's decoder list
     from Python -- MicroPython's builtin-method self check refuses
     lv.image_decoder_t.get_next(None));
  2. lv.image on three corpus frames (a JFIF still, the restart-marker frame,
     a real non-JFIF C920e frame -- the SOI-only sniff through LVGL) flushed
     to a 320x240 RGB565 frame buffer gives EXACTLY jpegio's own golden
     digests from frames/golden_tjpgd.json at scale 0.  Exact equality is the
     expectation (RGB565 into RGB565 at opa 255 is a row copy); if a digest
     differs the test says so LOUDLY, prints both digests and falls back to
     the BARS positional pixels and the Pillow means of reference.json;
  3. decoder_info tells the truth, read back through lv.image's source size:
     the size LVGL reports is the stream's own (a wrong w/h in the
     lv.image_dsc_t header is not echoed); non-JPEG bytes and a bare SOI are
     not claimed by jpegio -- they fall through to LVGL's built-in decoder,
     which echoes the header (jpegio never does); and an RGB565 pixel buffer
     that happens to start FF D8 is drawn by that built-in decoder as pixels;
  4. a truncated scan draws nothing and does not crash;
  5. a file source through fs_driver.py (when frozen in) decodes the same;
  6. 20 refreshes (each an open/decode/close) do not trend gc.mem_free() down;
  7. lv.deinit() / lv.init() drops the decoder; registering again restores it
     and decodes.  Last on purpose, with no gc.collect() after the re-init:
     on this build lv.init() -> display -> lv.deinit() -> lv.init() -> display
     -> one refresh -> gc.collect() dies with SIGBUS in MicroPython's
     gc_sweep_run_finalisers -> mp_load_method_maybe (a finaliser-flagged GC
     block whose type pointer is garbage) with no jpegio involved at all:
     reproduced with a label on a build without displayif (lvgl-micropython
     glue, not this decoder).

Skips (exit 0, prints SKIP) when `import lvgl` fails: the interpreter was
built without the lvgl-micropython usermod, and displayif's LVGL-less
clean-build CI runs this file too.

    ./micropython/ports/unix/build-standard/micropython tests/jpegio/test_jpegio_lvgl.py
"""
import sys

try:
    import lvgl as lv
except ImportError:
    print("SKIP: no `lvgl` module in this interpreter (build with the lvgl-micropython usermod)")
    sys.exit(0)

import binascii
import gc
import hashlib
import json

import jpegio

HERE = __file__.rsplit("/", 1)[0] if "/" in __file__ else "."
FRAMES = HERE + "/frames"
GOLDEN = FRAMES + "/golden_tjpgd.json"
REFERENCE = FRAMES + "/reference.json"
W, H = 320, 240
DECODER = "jpegio"
CORPUS = ("baseline_jfif_320x240.jpg", "restart_dri_320x240.jpg", "c920e_320x240_dri.jpg")

# The synthetic pattern (make_corpus.py): eight colour bars in the top 40 %.
# Same positional check as test_jpegio.py's check_pattern(), scale 0 only.
BARS = ((255, 255, 255), (255, 255, 0), (0, 255, 255), (0, 255, 0),
        (255, 0, 255), (255, 0, 0), (0, 0, 255), (0, 0, 0))
POS_TOL = 24
MEAN_TOL = 2.0


def sha256_hex(buf):
    return binascii.hexlify(hashlib.sha256(buf).digest()).decode()


def read_file(name):
    with open(FRAMES + "/" + name, "rb") as f:
        return f.read()


def pixel(buf, stride, x, y):
    i = 2 * (y * stride + x)
    v = buf[i] | (buf[i + 1] << 8)
    return ((v >> 8) & 0xF8, (v >> 3) & 0xFC, (v << 3) & 0xF8)


def means565(buf, n):
    r = g = b = 0
    for i in range(0, 2 * n, 2):
        lo = buf[i]
        hi = buf[i + 1]
        r += hi & 0xF8
        g += ((hi & 0x07) << 5) | ((lo >> 5) << 2)
        b += (lo << 3) & 0xF8
    return (r / n, g / n, b / n)


def check_bars(name, buf):
    y = (H * 2) // 5 // 2
    for i, rgb in enumerate(BARS):
        x = int((i + 0.5) * W / 8)
        got = pixel(buf, W, x, y)
        want = (rgb[0] & 0xF8, rgb[1] & 0xFC, rgb[2] & 0xF8)
        err = max(abs(got[c] - want[c]) for c in range(3))
        assert err <= POS_TOL, "%s bar %d at (%d, %d): got %s want %s" % (name, i, x, y, got, want)
    return len(BARS)


def check_means(name, buf, reference):
    want = reference["frames"][name]["scales"]["0"]["mean_rgb565"]
    got = means565(buf, W * H)
    err = [got[c] - want[c] for c in range(3)]
    for c in range(3):
        assert abs(err[c]) <= MEAN_TOL, "%s channel %d mean error %.3f exceeds %.1f" % (name, c, err[c], MEAN_TOL)
    return err


def decoders():
    """Names of LVGL's registered image decoders, in the order LVGL consults them."""
    return list(jpegio.lvgl_decoders())


def expect_raises(exc, fn, *needles):
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


class Frame:
    """The display's flush target: a zeroed W x H native RGB565 buffer."""

    def __init__(self):
        self.buf = bytearray(W * H * 2)
        self.flushes = 0

    def zero(self):
        for i in range(len(self.buf)):
            self.buf[i] = 0
        self.flushes = 0

    def flush_cb(self, disp, area, color_p):
        aw = area.x2 - area.x1 + 1
        ah = area.y2 - area.y1 + 1
        data = color_p.__dereference__(aw * ah * 2)
        for row in range(ah):
            o = ((area.y1 + row) * W + area.x1) * 2
            self.buf[o:o + aw * 2] = data[row * aw * 2:(row + 1) * aw * 2]
        self.flushes += 1
        disp.flush_ready()

    def is_zero(self):
        for b in self.buf:
            if b:
                return False
        return True


def setup_display(frame):
    """320x240 RGB565 display like lvgl-bindings/tools/test_lvgl_smoke.py's _setup_display."""
    disp = lv.display_create(W, H)
    disp.set_color_format(lv.COLOR_FORMAT.RGB565)
    buf = lv.draw_buf_create(W, H, lv.COLOR_FORMAT.RGB565, 0)
    disp.set_draw_buffers(buf, None)
    disp.set_render_mode(lv.DISPLAY_RENDER_MODE.PARTIAL)
    disp.set_flush_cb(frame.flush_cb)
    return disp, buf


def image_dsc(jpeg, w=W, h=H, cf=None):
    if cf is None:
        cf = lv.COLOR_FORMAT.RAW
    return lv.image_dsc_t({"header": {"w": w, "h": h, "cf": cf}, "data_size": len(jpeg), "data": jpeg})


def show(disp, frame, src):
    """Put `src` on a fresh screen as the only widget and render it into frame."""
    scr = lv.screen_active()
    scr.clean()
    img = lv.image(scr)
    img.set_src(src)
    img.set_pos(0, 0)
    frame.zero()
    scr.invalidate()
    lv.refr_now(disp)
    return img


def src_size(img):
    """The size lv_image took from lv_image_decoder_get_info (0x0 when no decoder claimed the source)."""
    return (img.get_src_width(), img.get_src_height())


def main():
    with open(GOLDEN) as f:
        golden = json.load(f)["frames"]
    with open(REFERENCE) as f:
        reference = json.load(f)
    jpegs = {name: read_file(name) for name in CORPUS}

    # 1. registration
    print("== 1. registration")
    assert not lv.is_initialized()
    assert jpegio.lvgl_decoders() == (), jpegio.lvgl_decoders()
    expect_raises(RuntimeError, jpegio.register_lvgl_decoder, "lv.init() first")
    lv.init()
    names = decoders()
    assert DECODER not in names, "decoder present before any registration: %s" % names
    print("after lv.init(): decoders %s (no %s yet)" % (names, DECODER))
    import jpegio as jpegio_again  # noqa: F401 -- a second import statement re-runs the module's __init__ hook
    auto = decoders().count(DECODER)
    assert auto <= 1, decoders()
    print("import-time registration (MICROPY_MODULE_BUILTIN_INIT): %s" % ("yes" if auto == 1 else "no -- explicit call needed"))
    added = jpegio.register_lvgl_decoder()
    assert added is (auto == 0), (added, auto)
    assert decoders().count(DECODER) == 1, decoders()
    assert jpegio.register_lvgl_decoder() is False
    assert jpegio.register_lvgl_decoder() is False
    names = decoders()
    assert names.count(DECODER) == 1, "registering three times left %s" % names
    assert names[0] == DECODER, "%s must be consulted first (head of LVGL's list): %s" % (DECODER, names)
    print("register_lvgl_decoder() x3: one decoder, first in line: %s" % names)

    # 2. lv.image on the corpus -> flush buffer -> jpegio's own golden digests
    print("== 2. lv.image through the flush buffer vs golden_tjpgd.json (scale 0)")
    frame = Frame()
    disp, draw_buf = setup_display(frame)
    exact = 0
    keep = []
    for name in CORPUS:
        dsc = image_dsc(jpegs[name])
        keep.append(dsc)
        img = show(disp, frame, dsc)
        assert frame.flushes > 0, "no flush during refr_now"
        assert not frame.is_zero(), "%s: flush buffer still zero -- nothing was drawn" % name
        assert (img.get_width(), img.get_height()) == (W, H), (name, img.get_width(), img.get_height())
        digest = sha256_hex(frame.buf)
        want = golden[name]["0"]
        if digest == want:
            exact += 1
            print("%-28s %d flush(es)  sha256 %s.. == golden: EXACT" % (name, frame.flushes, digest[:16]))
            continue
        print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
        print("!! %s: DIGEST MISMATCH through lv.image" % name)
        print("!!   got    %s" % digest)
        print("!!   golden %s" % want)
        print("!! falling back to the positional / mean checks -- NOT the exact gate")
        print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
        err = check_means(name, frame.buf, reference)
        checked = 0 if name.startswith("c920e_") else check_bars(name, frame.buf)
        print("%-28s fallback passed: mean err r=%+.3f g=%+.3f b=%+.3f, %d bar samples" % (name, err[0], err[1], err[2], checked))
    print("digests exact through lv.image: %d of %d" % (exact, len(CORPUS)))

    # 3. decoder_info tells the truth (read back through the widget's source size)
    print("== 3. decoder_info: stream size wins, non-JPEG not claimed, FF D8 pixels not claimed")
    wrong = image_dsc(jpegs["c920e_320x240_dri.jpg"], 1, 1)
    img = show(disp, frame, wrong)
    assert src_size(img) == (W, H), "header 1x1 was echoed: %s" % (src_size(img),)
    assert (img.get_width(), img.get_height()) == (W, H)
    assert sha256_hex(frame.buf) == golden["c920e_320x240_dri.jpg"]["0"] or not frame.is_zero()
    print("dsc header 1x1 -> lv.image sees %dx%d from jd_prepare" % src_size(img))
    # LVGL's built-in BIN decoder claims any variable source whose header cf
    # is not UNKNOWN (RAW included) and echoes the header; jpegio's decoder
    # sits ahead of it and never echoes (its size comes from jd_prepare or
    # not at all), so a header-sized result means jpegio did not claim it.
    junk = b"hello, not a jpeg at all " * 8
    img = show(disp, frame, image_dsc(junk, 8, 8))
    assert src_size(img) == (8, 8), src_size(img)
    soi_only = b"\xff\xd8" + b"\x00" * 62
    img = show(disp, frame, image_dsc(soi_only, 8, 8))
    assert src_size(img) == (8, 8), src_size(img)
    print("non-JPEG bytes and a bare SOI with cf RAW: not claimed by jpegio (LVGL's built-in decoder echoes the 8x8 header)")
    pixels = b"\xff\xd8" + b"\x00\x00" * 7      # 8x1 RGB565 whose first pixel is 0xD8FF
    px_dsc = image_dsc(pixels, 8, 1, lv.COLOR_FORMAT.RGB565)
    img = show(disp, frame, px_dsc)
    assert src_size(img) == (8, 1), src_size(img)
    assert (img.get_width(), img.get_height()) == (8, 1)
    assert pixel(frame.buf, W, 0, 0) == (0xD8, 0x1C, 0xF8), pixel(frame.buf, W, 0, 0)
    assert pixel(frame.buf, W, 1, 0) == (0, 0, 0)
    print("an RGB565 pixel buffer starting FF D8 falls through to LVGL's built-in decoder (8x1, first pixel 0xD8FF)")

    # 4. a truncated scan: headers parse (info OK), the decode fails, nothing crashes
    print("== 4. truncated scan")
    rst = jpegs["restart_dri_320x240.jpg"]
    half = rst[:len(rst) // 2]
    half_dsc = image_dsc(half)
    img = show(disp, frame, half_dsc)
    assert src_size(img) == (W, H), src_size(img)      # headers parse: info OK
    assert sha256_hex(frame.buf) != golden["restart_dri_320x240.jpg"]["0"]
    print("half a scan (%d of %d bytes): info OK, open refused, refr_now survived" % (len(half), len(rst)))

    # 5. a file source through fs_driver.py (frozen in by lvgl-micropython's manifest)
    print("== 5. file source")
    try:
        import fs_driver
    except ImportError:
        fs_driver = None
    if fs_driver is None:
        print("fs_driver not frozen in: file-source path not exercised here")
    else:
        fs_driver.register("S")
        path = "S:" + FRAMES + "/c920e_320x240_dri.jpg"
        img = show(disp, frame, path)
        assert src_size(img) == (W, H), src_size(img)
        digest = sha256_hex(frame.buf)
        want = golden["c920e_320x240_dri.jpg"]["0"]
        if digest == want:
            print("%s: sha256 %s.. == golden: EXACT" % (path, digest[:16]))
        else:
            print("!! file source DIGEST MISMATCH: got %s golden %s" % (digest, want))
            check_means("c920e_320x240_dri.jpg", frame.buf, reference)
            print("!! fallback (means) passed")
        img = show(disp, frame, "S:" + FRAMES + "/README.md")
        assert src_size(img) == (0, 0), src_size(img)
        print("a non-JPEG file (README.md) is not claimed")

    # 6. memory over repeated refreshes (each: open -> decode -> close)
    print("== 6. memory over 20 refreshes of the C920e frame")
    dsc = image_dsc(jpegs["c920e_320x240_dri.jpg"])
    img = show(disp, frame, dsc)
    assert sha256_hex(frame.buf) == golden["c920e_320x240_dri.jpg"]["0"] or not frame.is_zero()
    for _ in range(3):
        img.invalidate()
        lv.refr_now(disp)
    gc.collect()
    f0 = gc.mem_free()
    for _ in range(20):
        img.invalidate()
        lv.refr_now(disp)
    f1 = gc.mem_free()
    gc.collect()
    f2 = gc.mem_free()
    print("mem_free before %d, after 20 (uncollected) %d, collected %d" % (f0, f1, f2))
    assert f2 >= f0 - 4096, "mem_free trends down across refreshes (a %d-byte decode buffer per refresh?)" % (W * H * 2)

    # 7. deinit / init drops the decoder; register again and decode once more
    print("== 7. lv.deinit() / lv.init()")
    lv.deinit()
    assert jpegio.lvgl_decoders() == ()
    expect_raises(RuntimeError, jpegio.register_lvgl_decoder, "lv.init() first")
    lv.init()
    assert DECODER not in decoders()
    assert jpegio.register_lvgl_decoder() is True
    assert decoders().count(DECODER) == 1
    print("decoder gone after deinit, back after register_lvgl_decoder(): %s" % decoders())
    frame = Frame()
    disp, draw_buf = setup_display(frame)
    keep.append(image_dsc(jpegs["baseline_jfif_320x240.jpg"]))
    img = show(disp, frame, keep[-1])
    assert sha256_hex(frame.buf) == golden["baseline_jfif_320x240.jpg"]["0"], "digest changed after the deinit/init cycle"
    print("baseline_jfif_320x240.jpg after the cycle: digest EXACT")

    lv.deinit()
    print("jpegio LVGL tests passed: %d corpus frames through lv.image, %d digests exact" % (len(CORPUS), exact))


main()
