"""Decode the vendored PAL-sign EPROM-derived tables at build time.

Tables contain little-endian Cb,Y0,Cr,Y1 pixel pairs at BT.601 studio levels.
No hardware code, NumPy or runtime image decoder is needed.
"""
from pathlib import Path
import struct

from PIL import Image


def table(path, kind):
    raw = bytearray()
    for line in path.read_text().splitlines():
        if line.startswith(".4byte "):
            for value in line.split(None, 1)[1].split(","):
                raw.extend(struct.pack("<I", int(value, 0)))
    return [v[0] for v in struct.iter_unpack("<" + kind, raw)]


def rom_image(name, variant=2):
    root = Path(__file__).resolve().parent.parent / "assets/pm5644"
    pal, idx, lengths, starts, lines = (
        table(root / f"{name}_{suffix}.inc.h", kind)
        for suffix, kind in (("pal", "I"), ("idx", "H"), ("len", "B"),
                             ("start", "I"), ("line", "H")))
    if variant < 0 or (variant + 1) * 576 > len(lines):
        raise ValueError(f"{name}: missing variant {variant}")

    def rgb(y, cb, cr):
        # Studio-range BT.601 YCbCr -> full-range RGB, rounded and clipped.
        y = (y - 16) * 255 / 219
        cb, cr = (cb - 128) * 255 / 224, (cr - 128) * 255 / 224
        return bytes(max(0, min(255, round(v))) for v in
                     (y + 1.402 * cr, y - .344136 * cb - .714136 * cr,
                      y + 1.772 * cb))

    pairs = []
    for value in pal:
        cb, y0, cr, y1 = struct.pack("<I", value)
        pairs.append(rgb(y0, cb, cr) + rgb(y1, cb, cr))
    pixels = bytearray()
    for row in lines[variant * 576:(variant + 1) * 576]:
        if row + 1 >= len(starts):
            raise ValueError(f"{name}: invalid row {row}")
        a, b = starts[row:row + 2]
        if not 0 <= a <= b <= min(len(idx), len(lengths)):
            raise ValueError(f"{name}: invalid run bounds")
        data = bytearray()
        for i in range(a, b):
            if not lengths[i] or idx[i] >= len(pairs):
                raise ValueError(f"{name}: invalid run {i}")
            data.extend(pairs[idx[i]] * lengths[i])
        if len(data) != 720 * 3:
            raise ValueError(f"{name}: row has {len(data)} bytes")
        pixels.extend(data)
    return Image.frombytes("RGB", (720, 576), bytes(pixels))
