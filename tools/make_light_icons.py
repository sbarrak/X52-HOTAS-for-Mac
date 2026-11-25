#!/usr/bin/env python3
"""Draws the light icons of the manual: templates/light-icons/<name>.png (96 x 96, transparent background).

Standard library only (a small anti-aliased renderer and a PNG writer). The icons are what the manual shows for a
light: a full disc is steady, black rays around it mean flashing, a disc in two halves alternates two colours (the
primary one on the left, the secondary on the right, so red then amber is not amber then red), a disc in three parts
is the mode's own colour. A hollow disc is off.

    python3 tools/make_light_icons.py [output folder]

Names: off; g a r (steady); g_flash a_flash r_flash; ra ar rg gr ag ga (two colours: primary left, secondary right) and the
same with _flash; gar (three colours, the mode's own); gar_flash. 21 icons.
"""
import math
import os
import struct
import sys
import zlib

SIZE = 96
SS = 4   # samples per pixel side
COLORS = {"g": ((46, 158, 87), (28, 105, 58)), "a": ((240, 180, 41), (178, 126, 14)), "r": ((214, 69, 69), (148, 38, 38))}
GREY = (138, 138, 138)
RADIUS = 0.30       # of the icon size
RIM = 0.035
RAYS = 8
RAY_COLOR = (34, 34, 34)


def sector_color(letters, theta):
    """The colour letter at angle theta (radians, 0 = up, clockwise) for 1, 2 or 3 colours."""
    n = len(letters)
    if n == 1:
        return letters[0]
    if n == 2:
        return letters[0] if math.sin(theta) < 0 else letters[1]   # left half first
    t = (theta % (2 * math.pi)) / (2 * math.pi)
    return letters[min(int(t * 3), 2)]


def sample(letters, flashing, off, x, y):
    """RGBA of one sample at (x, y) in 0..1 icon coordinates; alpha 0 outside the drawing."""
    dx, dy = x - 0.5, y - 0.5
    rho = math.hypot(dx, dy)
    theta = math.atan2(dx, -dy)   # 0 = up, clockwise
    if off:
        if rho <= RADIUS:
            return GREY + (255,) if rho >= RADIUS - RIM else (255, 255, 255, 255)
        return (0, 0, 0, 0)
    if rho <= RADIUS:
        fill, dark = COLORS[sector_color(letters, theta)]
        return dark + (255,) if rho >= RADIUS - RIM else fill + (255,)
    if flashing and RADIUS + 0.06 <= rho <= RADIUS + 0.17:
        step = 2 * math.pi / RAYS
        off_angle = abs((theta + step / 2) % step - step / 2)
        if off_angle * rho < 0.024:
            return RAY_COLOR + (255,)
    return (0, 0, 0, 0)


def render(letters, flashing=False, off=False):
    rows = []
    for py in range(SIZE):
        row = bytearray()
        for px in range(SIZE):
            r = g = b = a = 0.0
            for sy in range(SS):
                for sx in range(SS):
                    cr, cg, cb, ca = sample(letters, flashing, off, (px + (sx + 0.5) / SS) / SIZE, (py + (sy + 0.5) / SS) / SIZE)
                    r += cr * ca
                    g += cg * ca
                    b += cb * ca
                    a += ca
            n = SS * SS
            if a == 0:
                row += bytes((0, 0, 0, 0))
            else:
                row += bytes((int(r / a), int(g / a), int(b / a), int(a / n)))
        rows.append(bytes(row))
    return rows


def png(rows):
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xffffffff)
    raw = b"".join(b"\x00" + r for r in rows)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


ICONS = [("off", "", False, True)]
for letters in ("g", "a", "r"):
    ICONS.append((letters, letters, False, False))
for letters in ("g", "a", "r"):
    ICONS.append((letters + "_flash", letters, True, False))
PAIRS = ("ra", "ar", "rg", "gr", "ag", "ga")
for letters in PAIRS:
    ICONS.append((letters, letters, False, False))
for letters in PAIRS:
    ICONS.append((letters + "_flash", letters, True, False))
ICONS.append(("gar", "gar", False, False))
ICONS.append(("gar_flash", "gar", True, False))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "templates/light-icons"
    os.makedirs(out, exist_ok=True)
    for name, letters, flashing, off in ICONS:
        with open(os.path.join(out, name + ".png"), "wb") as f:
            f.write(png(render(letters, flashing, off)))
    print("Wrote %d icons to %s" % (len(ICONS), out))


if __name__ == "__main__":
    main()
