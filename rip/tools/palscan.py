#!/usr/bin/env python3
"""Reads back the palette of every screen line, VBL by VBL, from the reference recording.

Given an indexed image known to be on screen (pixel indices from graphics/), each pixel of
the recorded frame tells the colour of its index on its line. Colours are printed as 0xRGB
nibbles (4-bit linear, as in the web port), 'bd' is the border colour (= colour 0).

usage: palscan.py <video.mkv> <first> <last> <indexed.png> [--at X,Y] [--lines A-B] [--changes]
  --at X,Y    position of the image on the ST screen (default 0,0)
  --lines A-B screen lines to report (default: the image's)
  --changes   only print a line when it differs from the previous frame
"""
import sys, subprocess, argparse, numpy as np
from collections import Counter
from PIL import Image

W, H, SX, SY = 416, 276, 48, 29
ap = argparse.ArgumentParser()
ap.add_argument('video'); ap.add_argument('first', type=int); ap.add_argument('last', type=int); ap.add_argument('png')
ap.add_argument('--at', default='0,0'); ap.add_argument('--lines'); ap.add_argument('--changes', action='store_true')
a = ap.parse_args()
idx = np.asarray(Image.open(a.png)).astype(int)
ox, oy = map(int, a.at.split(','))
l0, l1 = map(int, a.lines.split('-')) if a.lines else (oy, oy + idx.shape[0] - 1)
raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', a.video, '-vf', f"select='between(n\\,{a.first}\\,{a.last})'",
                      '-vsync', '0', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True, check=True).stdout
frames = np.frombuffer(raw, np.uint8).reshape(-1, H, W, 3) // 17
prev = {}
for n, F in enumerate(frames):
    for y in range(l0, l1 + 1):
        row = F[SY + y]
        m = {}
        iy = y - oy
        if 0 <= iy < idx.shape[0]:
            for i in np.unique(idx[iy]):
                xs = np.nonzero(idx[iy] == i)[0] + ox
                xs = xs[(xs >= 0) & (xs < 320)]
                if len(xs):
                    m[int(i)] = Counter('%x%x%x' % tuple(row[SX + x]) for x in xs).most_common(1)[0][0]
        s = 'bd=%x%x%x ' % tuple(row[5]) + ' '.join('%d=%s' % kv for kv in sorted(m.items()))
        if not a.changes or prev.get(y) != s:
            print(a.first + n, y, s)
        prev[y] = s
