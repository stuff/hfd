#!/usr/bin/env python3
"""Rebuilds the full-width (borders included) version of a screen that opens the side borders.

The pictures in graphics/ were read from video RAM 320 pixels wide, but some screens
(the HIGH FIDELITY DREAMS and CODE AND DESIGN rules) also draw in the side borders.
This takes one frame of the reference recording and the known indexed picture: on each
line, the picture tells which colour each index has; every frame pixel is then turned
back into an index with that per-line colour map.

usage: widegrab.py <video.mkv> <frame> <indexed.png> <out_wide_indexed.png> [--lines A-B]
Writes a 416-pixel-wide indexed PNG covering screen lines A..B (default: the picture's),
whose x = 0 is the left edge of Hatari's frame (the ST screen starts at x = 48).
Prints the lines where a border colour could not be matched.
"""
import sys, subprocess, argparse, numpy as np
from PIL import Image

W, H, SX, SY = 416, 276, 48, 29
ap = argparse.ArgumentParser()
ap.add_argument('video'); ap.add_argument('frame', type=int); ap.add_argument('png'); ap.add_argument('out')
ap.add_argument('--lines')
a = ap.parse_args()
src = Image.open(a.png)
idx = np.asarray(src).astype(int)
raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', a.video, '-vf', f"select='eq(n\\,{a.frame})'", '-vsync', '0',
                      '-frames:v', '1', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True, check=True).stdout
F = np.frombuffer(raw, np.uint8).reshape(H, W, 3).astype(int)
key = lambda c: (int(c[0]) << 16) | (int(c[1]) << 8) | int(c[2])
l0, l1 = map(int, a.lines.split('-')) if a.lines else (0, idx.shape[0] - 1)
out = np.zeros((l1 - l0 + 1, W), np.uint8)
bad = []
for y in range(l0, l1 + 1):
    row = F[SY + y]
    cmap = {}  # colour -> index, lowest index wins
    if y < idx.shape[0]:
        for x in range(320):
            cmap.setdefault(key(row[SX + x]), idx[y, x])
            cmap[key(row[SX + x])] = min(cmap[key(row[SX + x])], idx[y, x])
    for x in range(W):
        if SX <= x < SX + 320 and y < idx.shape[0]:
            out[y - l0, x] = idx[y, x - SX]
        else:
            i = cmap.get(key(row[x]))
            if i is None:
                i = 0
                bad.append((y, x, '%06x' % key(row[x])))
            out[y - l0, x] = i
im = Image.fromarray(out, 'P')
im.putpalette(src.getpalette())
im.save(a.out, optimize=True)
lines = sorted(set(b[0] for b in bad))
print(f'{a.out}: {out.shape[1]}x{out.shape[0]}, unmatched pixels on lines {lines[:20]}{"..." if len(lines) > 20 else ""}' if bad
      else f'{a.out}: {out.shape[1]}x{out.shape[0]}, every pixel matched')
