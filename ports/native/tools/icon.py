#!/usr/bin/env python3
"""Generates the program's icon from the demo's loading screen: the golden ear on its
dark blue background, in pixel art, at build time.

usage: icon.py <rip dir> <out dir>

Writes into <out dir>:
- hfd.iconset, for the macOS bundle (rounded square with Apple's margins), which
  iconutil turns into hfd.icns at build time (AppKit, unlike the Finder, doesn't
  read every hand-made .icns);
- hfd.ico and hfd.rc, for the Windows executable;
- icon.c, the RGBA pixels SDL gives the window on Linux.
Standard library only."""
import json, math, struct, sys, zlib
from pathlib import Path

from embed import hex_color, indexed_png

PICTURE = 'graphics/pictures/loading_screen_indexed.png'
META = 'graphics/pictures/loading_screen.json'
# The ear in the picture: x 118-208, y 0-174 (LOADING is below, from line 189).
EAR_X0, EAR_X1, EAR_Y0, EAR_Y1 = 118, 209, 0, 175
BACKGROUND = 0x004  # colour 0 of the loading screen
MASTER = 1024


def rgb(c):
    return ((c >> 8) & 15) * 17, ((c >> 4) & 15) * 17, (c & 15) * 17


def ear_pixels(rip):
    """The loading screen's colours: {(x, y): (r, g, b)} for the ear's pixels."""
    w, h, px = indexed_png(rip / PICTURE)
    meta = json.loads((rip / META).read_text())
    palette = [hex_color(c) for c in meta['palette']]
    per_line = meta.get('per_line_palette') or []
    ear = {}
    for y in range(EAR_Y0, EAR_Y1):
        line = list(palette)
        for i, c in (per_line[y] if y < len(per_line) else {}).items():
            if c:
                line[int(i)] = hex_color(c)
        for x in range(EAR_X0, EAR_X1):
            i = px[y * w + x]
            if i:
                ear[(x, y)] = rgb(line[i])
    return ear


def render(ear, margin, radius, scale):
    """The icon at MASTER×MASTER, RGBA rows: a rounded square of the background colour
    inset by `margin`, with the ear centred in it, each ST pixel `scale` pixels wide."""
    bg = rgb(BACKGROUND)
    cx, cy = (EAR_X0 + EAR_X1) / 2, (EAR_Y0 + EAR_Y1) / 2
    half = MASTER / 2 - margin
    rows = []
    for y in range(MASTER):
        row = bytearray(MASTER * 4)
        py = y + 0.5 - MASTER / 2
        for x in range(MASTER):
            px = x + 0.5 - MASTER / 2
            # signed distance to the rounded square, for an antialiased edge
            qx, qy = abs(px) - (half - radius), abs(py) - (half - radius)
            outside = (max(qx, 0) ** 2 + max(qy, 0) ** 2) ** 0.5 + min(max(qx, qy), 0) - radius
            alpha = min(1.0, max(0.0, 0.5 - outside))
            if not alpha:
                continue
            c = ear.get((math.floor(px / scale + cx), math.floor(py / scale + cy)), bg)
            row[x * 4:x * 4 + 4] = bytes((*c, round(alpha * 255)))
        rows.append(row)
    return rows


def halve(rows):
    """Half size, averaging 2×2 blocks (colours weighted by alpha)."""
    n = len(rows) // 2
    out = []
    for y in range(n):
        a, b = rows[2 * y], rows[2 * y + 1]
        row = bytearray(n * 4)
        for x in range(n):
            o = x * 8
            px = (a[o:o + 4], a[o + 4:o + 8], b[o:o + 4], b[o + 4:o + 8])
            alpha = sum(p[3] for p in px)
            if alpha:
                for k in range(3):
                    row[x * 4 + k] = (sum(p[k] * p[3] for p in px) + alpha // 2) // alpha
                row[x * 4 + 3] = (alpha + 2) // 4
        out.append(row)
    return out


def png(rows):
    size = len(rows)
    raw = b''.join(b'\0' + bytes(r) for r in rows)
    chunk = lambda kind, data: struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def sizes(master):
    """{size: rows} from MASTER down to 16."""
    out, rows = {}, master
    while len(rows) >= 16:
        out[len(rows)] = rows
        rows = halve(rows)
    return out


def iconset(images, path):
    """The PNGs of a macOS .iconset, under their standard names."""
    path.mkdir(exist_ok=True)
    for pt in (16, 32, 128, 256, 512):
        write(path / f'icon_{pt}x{pt}.png', images[pt])
        write(path / f'icon_{pt}x{pt}@2x.png', images[2 * pt])


def ico(images):
    ns = [16, 32, 64, 128, 256]
    head = struct.pack('<HHH', 0, 1, len(ns))
    offset, entries, data = 6 + 16 * len(ns), b'', b''
    for n in ns:
        entries += struct.pack('<BBBBHHII', n % 256, n % 256, 0, 0, 1, 32, len(images[n]), offset + len(data))
        data += images[n]
    return head + entries + data


def write(path, data):
    # always written, even when unchanged, so that Make sees the outputs up to date
    path.write_bytes(data)


def main(rip, out):
    out.mkdir(parents=True, exist_ok=True)
    ear = ear_pixels(rip)
    # macOS: Apple's grid, an 824-pixel rounded square in 1024, the ear 4 pixels per ST pixel.
    mac = sizes(render(ear, margin=100, radius=185, scale=4))
    iconset({n: png(r) for n, r in mac.items()}, out / 'hfd.iconset')
    # Windows and Linux: the square fills the icon.
    full = sizes(render(ear, margin=8, radius=160, scale=5))
    write(out / 'hfd.ico', ico({n: png(full[n]) for n in (16, 32, 64, 128, 256)}))
    write(out / 'hfd.rc', b'1 ICON "hfd.ico"\n')
    pixels = b''.join(bytes(r) for r in full[128])
    c = ['/* Generated by tools/icon.py from rip/. Do not edit. */', '#include "assets.h"',
         f'static const uint8_t icon_rgba[{len(pixels)}] = {{']
    c += [','.join(str(b) for b in pixels[i:i + 32]) + ',' for i in range(0, len(pixels), 32)]
    c += ['};', 'const uint8_t *const ICON_RGBA = icon_rgba;', '']
    write(out / 'icon.c', '\n'.join(c).encode())


if __name__ == '__main__':
    main(Path(sys.argv[1]), Path(sys.argv[2]))
