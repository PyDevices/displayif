#!/usr/bin/env python3
"""Pillow fidelity reference for the jpegio corpus -> frames/reference.json.

Run:  python3 tests/jpegio/reference.py        (needs Pillow; 12.3 used)

For every baseline (SOF0) frame in frames/ that Pillow can decode:

  1. full decode with Pillow to 8-bit RGB (libjpeg-turbo: islow IDCT, "fancy"
     triangle chroma upsampling; grayscale is expanded to R=G=B);
  2. for scale 0..3, integer box averaging of that full decode in pure Python:
     each output pixel is the sum of a (1<<scale)^2 square, shifted right by
     2*scale (truncating), and the output is floor(w >> scale) x floor(h >> scale).
     That mirrors what TJpgDec R0.03 does in mcu_output(): it averages the same
     squares with the same truncating shift and clips per MCU so the remainder
     columns/rows of a partial MCU are dropped.  Pillow's draft() (libjpeg's
     DCT-domain scaling) and reduce() (rounds to nearest) are deliberately NOT
     used: neither matches TJpgDec's arithmetic.
  3. pack to RGB565 exactly as tjpgd.c does under JD_FORMAT 1,
     ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3), stored as little-endian
     uint16 (native order on every PyDevices target), row-major, no stride;
  4. record width, height, sha256 of the packed buffer, and per-channel means
     of the 8-bit image (mean_rgb) and of the channels recoverable from the
     RGB565 buffer, i.e. r&0xF8, g&0xFC, b&0xF8 (mean_rgb565).

This is a FIDELITY reference, not a bit-exact golden: TJpgDec's fixed-point
IDCT, nearest-neighbour chroma replication and (at scale 3) DC-only blocks all
differ from libjpeg by a little, so a test compares means (and, if it wants,
per-pixel error against a decode of the same frame) with a tolerance.  The
sha256 is there so an exact match, should one occur, can be recognised.
"""
import hashlib
import json
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import jpegwalk  # noqa: E402

FRAMES = os.path.join(HERE, "frames")
OUT = os.path.join(FRAMES, "reference.json")
SCALES = (0, 1, 2, 3)


def box_down(rgb, w, h, scale):
    """Truncating integer box average of packed 8-bit RGB by 2**scale (TJpgDec style)."""
    if scale == 0:
        return bytes(rgb), w, h
    f = 1 << scale
    sh = 2 * scale
    ow, oh = w >> scale, h >> scale
    out = bytearray(ow * oh * 3)
    o = 0
    for oy in range(oh):
        y0 = oy * f
        for ox in range(ow):
            r = g = b = 0
            for yy in range(y0, y0 + f):
                base = (yy * w + ox * f) * 3
                row = rgb[base:base + f * 3]
                r += sum(row[0::3])
                g += sum(row[1::3])
                b += sum(row[2::3])
            out[o] = r >> sh
            out[o + 1] = g >> sh
            out[o + 2] = b >> sh
            o += 3
    return bytes(out), ow, oh


def pack_rgb565(rgb):
    n = len(rgb) // 3
    out = bytearray(n * 2)
    for i in range(n):
        j = 3 * i
        v = ((rgb[j] & 0xF8) << 8) | ((rgb[j + 1] & 0xFC) << 3) | (rgb[j + 2] >> 3)
        out[2 * i] = v & 0xFF
        out[2 * i + 1] = v >> 8
    return bytes(out)


def means(rgb):
    n = len(rgb) // 3
    return [round(sum(rgb[c::3]) / n, 4) for c in range(3)]


def means_565(rgb):
    n = len(rgb) // 3
    return [round(sum(v & 0xF8 for v in rgb[0::3]) / n, 4),
            round(sum(v & 0xFC for v in rgb[1::3]) / n, 4),
            round(sum(v & 0xF8 for v in rgb[2::3]) / n, 4)]


def reference_for(path):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    full = im.tobytes()
    scales = {}
    for s in SCALES:
        rgb, ow, oh = box_down(full, w, h, s)
        packed = pack_rgb565(rgb)
        scales[str(s)] = {
            "width": ow,
            "height": oh,
            "sha256_rgb565": hashlib.sha256(packed).hexdigest(),
            "mean_rgb": means(rgb),
            "mean_rgb565": means_565(rgb),
        }
    return scales


def main():
    doc = {
        "_about": [
            "Pillow fidelity reference for tests/jpegio/frames; written by tests/jpegio/reference.py.",
            "scales[k]: decode at scale k (1/2**k) -> width x height RGB565 buffer, little-endian",
            "uint16, row-major; sha256_rgb565 is the digest of that buffer; mean_rgb are per-channel",
            "means of the 8-bit reference, mean_rgb565 the means of r&0xF8, g&0xFC, b&0xF8 (what a",
            "decoder's RGB565 output unpacks to). Downscaling is a truncating integer box average of",
            "the full libjpeg decode (as TJpgDec's mcu_output does), NOT draft()/reduce().",
            "Compare with a tolerance: TJpgDec's IDCT, chroma replication and DC-only 1/8 differ.",
        ],
        "pillow": Image.__version__,
        "rgb565": "((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3), little-endian uint16 (native order on all PyDevices targets)",
        "scale_dims": "width = source_width >> scale, height = source_height >> scale (partial-MCU remainder dropped, as TJpgDec does)",
        "frames": {},
        "not_referenced": {},
    }
    for name in sorted(os.listdir(FRAMES)):
        if not name.lower().endswith(".jpg"):
            continue
        path = os.path.join(FRAMES, name)
        with open(path, "rb") as f:
            data = f.read()
        d = jpegwalk.describe(data)
        entry = {
            "sha256_file": hashlib.sha256(data).hexdigest(),
            "bytes": len(data),
            "width": d["width"],
            "height": d["height"],
            "sof": d["sof"],
            "sampling": d["sampling"],
            "components": d["components"],
            "dri": d.get("dri"),
            "markers": " ".join(d["sequence"]),
        }
        if d["sof"] != "SOF0":
            doc["not_referenced"][name] = "%s: TJpgDec is baseline-only and must refuse it" % d["sof"]
            continue
        try:
            entry["scales"] = reference_for(path)
        except Exception as e:  # noqa: BLE001
            doc["not_referenced"][name] = "Pillow cannot decode it: %s: %s" % (type(e).__name__, e)
            continue
        doc["frames"][name] = entry
        print("%-36s %dx%d %s" % (name, d["width"], d["height"],
                                  " ".join("s%s=%dx%d" % (k, v["width"], v["height"])
                                           for k, v in entry["scales"].items())))
    for name, why in doc["not_referenced"].items():
        print("%-36s not referenced: %s" % (name, why))
    with open(OUT, "w") as f:
        json.dump(doc, f, indent=1)
        f.write("\n")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
