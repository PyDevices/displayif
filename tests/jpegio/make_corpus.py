#!/usr/bin/env python3
"""Build the jpegio test corpus in frames/ and write frames/README.md.

Run:  python3 tests/jpegio/make_corpus.py        (needs Pillow; 12.3 used)

Every synthetic frame is the same procedural test pattern, drawn in pure
Python and encoded by Pillow/libjpeg:

  * top 40 %      -- eight 100 % colour bars (white, yellow, cyan, green,
                     magenta, red, blue, black): hard chroma edges every w/8 px
  * 40 % .. 70 %  -- gradient: R along x, G along y, B along the anti-diagonal
  * bottom band   -- left half a 1-px black/white checker (the highest
                     spatial frequency an 8x8 DCT can carry, exercises the
                     AC tables and 4:2:0 chroma against a luma edge); right
                     half a pure-red / pure-blue split (a saturated chroma
                     edge that chroma subsampling has to straddle)

Files that are derived (nodht, dri_first) are made by re-writing the marker
segments with jpegwalk -- never by regex on the binary.  Real camera frames
named c920e_*.jpg are never touched; they are only described in the README.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import jpegwalk  # noqa: E402

FRAMES = os.path.join(HERE, "frames")
QUALITY = 90
RESTART_BLOCKS = 4   # DRI interval in MCUs for the restart frames

C920E_PROVENANCE = ("Logitech C920e over usbif UVC on ESP32-S3, captured by the usbif "
                    "session 2026-09-03; DHT present; DRI present")

BARS = [(255, 255, 255), (255, 255, 0), (0, 255, 255), (0, 255, 0),
        (255, 0, 255), (255, 0, 0), (0, 0, 255), (0, 0, 0)]


def pattern(w, h):
    """The synthetic test pattern as a Pillow RGB image (see module docstring)."""
    y_bars = max(1, (h * 2) // 5)
    y_grad = max(y_bars + 1, (h * 7) // 10)
    buf = bytearray(w * h * 3)
    i = 0
    for y in range(h):
        for x in range(w):
            if y < y_bars:
                r, g, b = BARS[x * 8 // w]
            elif y < y_grad:
                r = x * 255 // max(1, w - 1)
                g = (y - y_bars) * 255 // max(1, y_grad - y_bars - 1)
                b = 255 - (x + y) * 255 // max(1, w + h - 2)
            elif x < w // 2:
                r = g = b = 255 if (x + y) & 1 else 0
            else:
                r, g, b = (255, 0, 0) if x < (w * 3) // 4 else (0, 0, 255)
            buf[i] = r
            buf[i + 1] = g
            buf[i + 2] = b
            i += 3
    return Image.frombytes("RGB", (w, h), bytes(buf))


def write(name, data):
    with open(os.path.join(FRAMES, name), "wb") as f:
        f.write(data)
    return data


def read(name):
    with open(os.path.join(FRAMES, name), "rb") as f:
        return f.read()


def save(name, im, **kw):
    kw.setdefault("quality", QUALITY)
    im.save(os.path.join(FRAMES, name), "JPEG", **kw)
    return read(name)


def decoded(name):
    return Image.open(os.path.join(FRAMES, name)).convert("RGB").tobytes()


def strip_dht(data):
    segs = [s for s in jpegwalk.walk(data) if s[0] != 0xC4]
    out = jpegwalk.rebuild(segs)
    seq = jpegwalk.sequence(jpegwalk.walk(out))
    assert "DHT" not in seq, seq
    for must in ("SOI", "SOF0", "SOS", "EOI"):
        assert must in seq, (must, seq)
    return out


def dri_first(data):
    """Re-order a JFIF file's segments the way a Logitech C920e emits UVC MJPEG:
    SOI DQT.. SOF0 DHT.. APP0 DRI SOS (i.e. NOT JFIF-first)."""
    order = ["SOI", "DQT", "SOF0", "DHT", "APP0", "DRI", "SOS", "EOI"]
    by = {}
    for s in jpegwalk.walk(data):
        by.setdefault(s[1], []).append(s)
    segs = []
    for name in order:
        segs.extend(by.pop(name))
    assert not by, "unexpected segments: %s" % sorted(by)
    return jpegwalk.rebuild(segs)


# name -> (how it is made)
DESCRIPTIONS = {}


def notes(data):
    """Walker-derived facts a marker sequence alone does not show."""
    out = []
    dqt_tables = dqt_segs = 0
    for m, name, _off, raw in jpegwalk.walk(data):
        if name == "APP0":
            ident = raw[4:9]
            out.append("APP0=JFIF" if ident == b"JFIF\0" else
                       "APP0 payload is not JFIF (%d bytes, %s)" % (
                           len(raw) - 4, "all zero" if not any(raw[4:]) else raw[4:12].hex()))
        elif m == 0xDB:
            dqt_segs += 1
            p = 4
            while p < len(raw):
                dqt_tables += 1
                p += 1 + (128 if raw[p] >> 4 else 64)
        elif name == "COM":
            out.append("COM %d bytes%s" % (len(raw) - 4, " (all zero)" if not any(raw[4:]) else ""))
        elif name == "TRAILING":
            out.append("%d bytes after EOI (%s..)" % (len(raw), raw[:4].hex()))
    out.insert(0, "%d DQT table%s in %d segment%s" % (
        dqt_tables, "s" if dqt_tables != 1 else "", dqt_segs, "s" if dqt_segs != 1 else ""))
    return "; ".join(out)


def build():
    os.makedirs(FRAMES, exist_ok=True)
    p64 = pattern(64, 48)
    p320 = pattern(320, 240)

    DESCRIPTIONS["baseline_jfif_64x48.jpg"] = (
        "baseline JFIF still of the test pattern; Pillow save(quality=%d, subsampling='4:2:0')" % QUALITY)
    save("baseline_jfif_64x48.jpg", p64, subsampling="4:2:0")

    DESCRIPTIONS["baseline_jfif_320x240.jpg"] = (
        "baseline JFIF still of the test pattern at 320x240; Pillow save(quality=%d, "
        "subsampling='4:2:0')" % QUALITY)
    base320 = save("baseline_jfif_320x240.jpg", p320, subsampling="4:2:0")

    DESCRIPTIONS["restart_dri_320x240.jpg"] = (
        "as baseline_jfif_320x240 but with restart markers: Pillow "
        "save(..., restart_marker_blocks=%d) -> DRI segment + RSTn every %d MCUs"
        % (RESTART_BLOCKS, RESTART_BLOCKS))
    restart = save("restart_dri_320x240.jpg", p320, subsampling="4:2:0",
                   restart_marker_blocks=RESTART_BLOCKS)
    assert decoded("restart_dri_320x240.jpg") == decoded("baseline_jfif_320x240.jpg")

    DESCRIPTIONS["nodht_320x240.jpg"] = (
        "baseline_jfif_320x240 with every DHT (FFC4) segment removed by jpegwalk -- the "
        "'MJPEG without Huffman tables' case; a decoder must either inject the standard "
        "tables (ITU T.81 K.3) or refuse it")
    write("nodht_320x240.jpg", strip_dht(base320))
    try:
        same = decoded("nodht_320x240.jpg") == decoded("baseline_jfif_320x240.jpg")
        DESCRIPTIONS["nodht_320x240.jpg"] += (
            "; Pillow/libjpeg-turbo decodes it by injecting the standard tables (pixels %s "
            "baseline_jfif_320x240)" % ("identical to" if same else "DIFFER from"))
    except Exception as e:  # noqa: BLE001
        DESCRIPTIONS["nodht_320x240.jpg"] += "; Pillow refuses it (%s: %s)" % (type(e).__name__, e)

    DESCRIPTIONS["progressive_320x240.jpg"] = (
        "the 320x240 pattern saved progressive (SOF2): Pillow save(progressive=True, "
        "subsampling='4:2:0'); TJpgDec is baseline-only and must refuse it")
    save("progressive_320x240.jpg", p320, subsampling="4:2:0", progressive=True)

    DESCRIPTIONS["odd_size_37x29.jpg"] = (
        "baseline, odd dimensions so the right column and bottom row of MCUs are partial "
        "(37 = 2x16 + 5, 29 = 1x16 + 13); Pillow save(quality=%d, subsampling='4:2:0')" % QUALITY)
    save("odd_size_37x29.jpg", pattern(37, 29), subsampling="4:2:0")

    DESCRIPTIONS["grayscale_64x48.jpg"] = (
        "baseline grayscale (one component): pattern(64,48).convert('L'); "
        "Pillow save(quality=%d)" % QUALITY)
    save("grayscale_64x48.jpg", p64.convert("L"))

    DESCRIPTIONS["dri_first_mjpeg_style_320x240.jpg"] = (
        "restart_dri_320x240 with its segments re-ordered by jpegwalk to SOI DQT DQT SOF0 "
        "DHT DHT DHT DHT APP0 DRI SOS (APP0/JFIF after the tables, as a Logitech C920e "
        "emits UVC MJPEG); Pillow decodes it pixel-identical to restart_dri_320x240")
    write("dri_first_mjpeg_style_320x240.jpg", dri_first(restart))
    assert decoded("dri_first_mjpeg_style_320x240.jpg") == decoded("restart_dri_320x240.jpg")
    seq = jpegwalk.describe(read("dri_first_mjpeg_style_320x240.jpg"))["sequence"]
    assert seq[:3] == ["SOI", "DQT", "DQT"] and seq[3] == "SOF0" and seq[-3:] == ["DRI", "SOS", "EOI"], seq
    assert seq.index("APP0") > seq.index("DHT"), seq


def readme():
    names = sorted(n for n in os.listdir(FRAMES) if n.lower().endswith(".jpg"))
    lines = [
        "# jpegio test corpus",
        "",
        "Generated by [`../make_corpus.py`](../make_corpus.py) (Pillow %s); marker sequences and"
        " sizes below come from [`../jpegwalk.py`](../jpegwalk.py). `reference.json` is the"
        " Pillow fidelity reference written by [`../reference.py`](../reference.py) -- per"
        " baseline frame and scale 0..3: RGB565 buffer sha256 and per-channel means. It is a"
        " tolerance reference, not a bit-exact golden (TJpgDec and libjpeg differ in IDCT"
        " rounding and chroma upsampling)." % Image.__version__,
        "",
        "Synthetic frames share one procedural pattern: colour bars (top 40 %), an x/y/diagonal"
        " RGB gradient (40-70 %), a 1-px black/white checker (bottom left) and a red|blue"
        " split (bottom right). Marker sequences omit the RSTn markers inside the scan; their"
        " count is given with the DRI interval (a DRI of 0 means the segment is present but restart"
        " intervals are disabled). `TRAILING` = bytes after EOI.",
        "",
        "| file | what / how made | markers | size | sampling | bytes | DRI | notes |",
        "|------|-----------------|---------|------|----------|-------|-----|-------|",
    ]
    for name in names:
        d = jpegwalk.describe(read(name))
        if name.startswith("c920e_"):
            what = "real camera frame: %s" % C920E_PROVENANCE
        else:
            what = DESCRIPTIONS.get(name, "(not produced by make_corpus.py)")
        dri = "%d (%d RST)" % (d["dri"], d.get("rst_markers", 0)) if "dri" in d else "-"
        lines.append("| `%s` | %s | `%s` | %dx%d %s | %s | %d | %s | %s |" % (
            name, what, " ".join(d["sequence"]), d["width"], d["height"], d.get("sof", "?"),
            d.get("sampling", "?"), d["bytes"], dri, notes(read(name))))
    lines.append("")
    with open(os.path.join(FRAMES, "README.md"), "w") as f:
        f.write("\n".join(lines))
    return names


if __name__ == "__main__":
    build()
    for name in readme():
        print("%-36s %s" % (name, jpegwalk.format_description(jpegwalk.describe(read(name)))))
