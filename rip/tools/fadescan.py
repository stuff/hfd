#!/usr/bin/env python3
"""Finds, VBL by VBL, the fade level of each band of lines of a picture in the recording.

The picture's full-brightness colours come from its JSON (palette + per_line_palette).
For every frame and band, the fade level j (0 = full, 7 = black) is the one for which
fade(colour, j) gives the recorded colour of every index used on the band's lines
(the border counts as colour 0). Both fades of the demo are tried: 'm' (main program,
drops the STE low bit) and 'l' (loader, keeps it). '?' means no level fits.

usage: fadescan.py <video.mkv> <first> <last> <picture_indexed.png> <picture.json> name:A-B [name:A-B ...]
Prints one line per change: VBL, then name=level for each band.
"""
import sys, json, subprocess, numpy as np
from PIL import Image

W, H, SX, SY = 416, 276, 48, 29
video, first, last, png, js = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4], sys.argv[5]
bands = []
for b in sys.argv[6:]:
    name, r = b.split(':'); a0, a1 = map(int, r.split('-')); bands.append((name, a0, a1))
idx = np.asarray(Image.open(png)).astype(int)
meta = json.load(open(js))
nib = lambda h: tuple(int(h[i:i + 2], 16) // 17 for i in (1, 3, 5))
base_pal = [nib(c) for c in meta['palette']]
per_line = meta.get('per_line_palette') or [{}] * idx.shape[0]

def fade(c, j, keep):
    if j == 0: return c
    out = []
    for n in c:
        v3 = max(0, (n >> 1) - j)
        out.append((v3 << 1) | (n & 1 if keep and v3 else 0))
    return tuple(out)

# per band: list of (line, x or None for border, full colour)
probes = {}
for name, a0, a1 in bands:
    pts = []
    for y in range(a0, a1 + 1):
        pal = list(base_pal)
        for k, v in per_line[y].items(): pal[int(k)] = nib(v)
        pts.append((y, None, pal[0]))
        for i in np.unique(idx[y]):
            if i == 0: continue
            x = int(np.nonzero(idx[y] == i)[0][0])
            pts.append((y, x, pal[int(i)]))
    probes[name] = pts

raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', video, '-vf', f"select='between(n\\,{first}\\,{last})'",
                      '-vsync', '0', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True, check=True).stdout
frames = np.frombuffer(raw, np.uint8).reshape(-1, H, W, 3) // 17
prev = None
for n, F in enumerate(frames):
    res = []
    for name, a0, a1 in bands:
        got = [tuple(int(v) for v in (F[SY + y, 5] if x is None else F[SY + y, SX + x])) for y, x, c in probes[name]]
        lvl = '?'
        for keep, tag in ((False, 'm'), (True, 'l')):
            for j in range(8):
                if all(fade(c, j, keep) == g for (y, x, c), g in zip(probes[name], got)):
                    lvl = str(j) if tag == 'm' else 'l' + str(j); break
            if lvl != '?': break
        res.append(f'{name}={lvl}')
    s = ' '.join(res)
    if s != prev: print(first + n, s)
    prev = s
