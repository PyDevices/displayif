"""jpegwalk -- a tiny JPEG marker walker.

Plain Python with no third-party imports, written so it also runs under
MicroPython (the jpegio unix tests can import it to assert the corpus's
marker sequences).  No regexes: JPEG entropy-coded data is binary and the
only safe way to find the next marker is to walk it byte by byte honouring
byte stuffing (FF 00) and RSTn markers.

walk(data) -> list of segments, each a tuple

    (marker, name, offset, raw)

where ``raw`` is the exact bytes of that segment: the FF xx marker, plus the
two-byte length and payload for markers that carry one.  The SOS segment's
``raw`` also carries the entropy-coded scan data that follows it, up to the
next non-RSTn marker, so RSTn markers stay inside the scan data.  Therefore

    b"".join(s[3] for s in walk(data)) == data

and a file can be rebuilt after dropping or re-ordering segments.
"""

_NAMES = {
    0x01: "TEM",
    0xC0: "SOF0", 0xC1: "SOF1", 0xC2: "SOF2", 0xC3: "SOF3",
    0xC4: "DHT",
    0xC5: "SOF5", 0xC6: "SOF6", 0xC7: "SOF7",
    0xC8: "JPG", 0xC9: "SOF9", 0xCA: "SOF10", 0xCB: "SOF11",
    0xCC: "DAC", 0xCD: "SOF13", 0xCE: "SOF14", 0xCF: "SOF15",
    0xD8: "SOI", 0xD9: "EOI", 0xDA: "SOS", 0xDB: "DQT", 0xDC: "DNL",
    0xDD: "DRI", 0xDE: "DHP", 0xDF: "EXP", 0xFE: "COM",
}

SOF_MARKERS = (0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7,
               0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF)


def marker_name(m):
    if 0xD0 <= m <= 0xD7:
        return "RST%d" % (m - 0xD0)
    if 0xE0 <= m <= 0xEF:
        return "APP%d" % (m - 0xE0)
    if 0xF0 <= m <= 0xFD:
        return "JPG%d" % (m - 0xF0)
    return _NAMES.get(m, "0xFF%02X" % m)


def _standalone(m):
    return m == 0xD8 or m == 0xD9 or m == 0x01 or (0xD0 <= m <= 0xD7)


def walk(data):
    data = bytes(data)
    n = len(data)
    segs = []
    pos = 0
    while pos < n:
        if data[pos] != 0xFF:
            raise ValueError("expected a marker at offset %d, got 0x%02X" % (pos, data[pos]))
        start = pos
        while pos + 1 < n and data[pos + 1] == 0xFF:   # fill bytes
            pos += 1
        if pos + 1 >= n:
            raise ValueError("truncated marker at offset %d" % start)
        m = data[pos + 1]
        if _standalone(m):
            pos += 2
            segs.append((m, marker_name(m), start, data[start:pos]))
            if m == 0xD9:
                if pos != n:
                    segs.append((None, "TRAILING", pos, data[pos:]))
                break
            continue
        if pos + 4 > n:
            raise ValueError("truncated segment length at offset %d" % pos)
        length = (data[pos + 2] << 8) | data[pos + 3]
        if length < 2 or pos + 2 + length > n:
            raise ValueError("bad segment length %d at offset %d" % (length, pos))
        pos += 2 + length
        if m == 0xDA:
            # Entropy-coded data: skip to the next marker that is not a
            # stuffed zero (FF 00), an RSTn (FF D0..D7) or a fill byte.
            p = pos
            while True:
                q = data.find(b"\xff", p)
                if q < 0 or q + 1 >= n:
                    p = n
                    break
                b = data[q + 1]
                if b == 0x00 or (0xD0 <= b <= 0xD7):
                    p = q + 2
                    continue
                if b == 0xFF:
                    p = q + 1
                    continue
                p = q
                break
            pos = p
        segs.append((m, marker_name(m), start, data[start:pos]))
    return segs


def rebuild(segs):
    return b"".join(s[3] for s in segs)


def sequence(segs):
    """Marker names in file order; RSTn markers live inside SOS and are not listed."""
    return [s[1] for s in segs]


def sof_info(seg):
    """Decode a SOFn payload -> dict(precision, height, width, components=[(id, h, v, tq), ...])."""
    raw = seg[3]
    p = 4
    info = {
        "precision": raw[p],
        "height": (raw[p + 1] << 8) | raw[p + 2],
        "width": (raw[p + 3] << 8) | raw[p + 4],
    }
    nc = raw[p + 5]
    comps = []
    p += 6
    for _ in range(nc):
        comps.append((raw[p], raw[p + 1] >> 4, raw[p + 1] & 0x0F, raw[p + 2]))
        p += 3
    info["components"] = comps
    return info


def sampling_name(comps):
    """'4:4:4' / '4:2:2' / '4:2:0' / 'gray' / 'HxV' from SOF component sampling factors."""
    if len(comps) == 1:
        return "gray"
    h, v = comps[0][1], comps[0][2]
    if (h, v) == (1, 1):
        return "4:4:4"
    if (h, v) == (2, 1):
        return "4:2:2"
    if (h, v) == (2, 2):
        return "4:2:0"
    return "%dx%d" % (h, v)


def dri_interval(seg):
    raw = seg[3]
    return (raw[4] << 8) | raw[5]


def count_rst(seg):
    """Number of RSTn markers inside an SOS segment's scan data."""
    raw = seg[3]
    n = 0
    p = 0
    while True:
        q = raw.find(b"\xff", p)
        if q < 0 or q + 1 >= len(raw):
            return n
        if 0xD0 <= raw[q + 1] <= 0xD7:
            n += 1
        p = q + 1


def describe(data):
    """Summary dict for a whole file: sequence, dims, sampling, DRI, RST count, byte size."""
    segs = walk(data)
    d = {"bytes": len(data), "sequence": sequence(segs)}
    for s in segs:
        if s[0] in SOF_MARKERS and "width" not in d:
            i = sof_info(s)
            d["width"] = i["width"]
            d["height"] = i["height"]
            d["precision"] = i["precision"]
            d["components"] = len(i["components"])
            d["sampling"] = sampling_name(i["components"])
            d["sof"] = s[1]
        elif s[0] == 0xDD:
            d["dri"] = dri_interval(s)
        elif s[0] == 0xDA:
            d["rst_markers"] = d.get("rst_markers", 0) + count_rst(s)
    return d


def format_description(d):
    parts = ["%dx%d" % (d.get("width", 0), d.get("height", 0)),
             d.get("sof", "?"), d.get("sampling", "?"),
             "%d bytes" % d["bytes"]]
    if "dri" in d:
        parts.append("DRI=%d (%d RST)" % (d["dri"], d.get("rst_markers", 0)))
    return " ".join(d["sequence"]) + " | " + ", ".join(parts)


if __name__ == "__main__":
    import sys
    for path in sys.argv[1:]:
        with open(path, "rb") as f:
            data = f.read()
        print("%s: %s" % (path, format_description(describe(data))))
