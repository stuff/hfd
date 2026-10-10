#!/usr/bin/env python3
"""Compares frames recorded by capture with the Hatari reference recording.
usage: compare.py <frames dir> <reference video.mkv> [--from VBL] [--to VBL] [--diff DIR]

Each of our frames is compared with the reference frame of the VBL it shows (capture's
vbl.txt), on the whole 416 × 276 frame: ours at (37, 30) in capture's frames, where the
port puts it. Hatari's drive LED (top right) is ignored. Colours are compared as the
12-bit values both machines use: Hatari shows a 4-bit component n as n × 17, vAmiga as
n × 16.
Prints, for each run of frames, how many pixels differ; with --diff, writes side-by-side
PNGs (reference, ours, differences) of the first differing frame of each run."""
import argparse
import av
import numpy as np
from PIL import Image

W, H, X0, Y0 = 416, 276, 37, 30

p = argparse.ArgumentParser()
p.add_argument('frames')
p.add_argument('video')
p.add_argument('--from', dest='first', type=int, default=0)
p.add_argument('--to', type=int, default=1 << 30)
p.add_argument('--diff')
a = p.parse_args()

shown = {}  # VBL -> our frame showing it (the first one)
for line in open(f'{a.frames}/vbl.txt'):
    frame, vbl = map(int, line.split())
    if a.first <= vbl <= a.to:
        shown.setdefault(vbl, frame)
if not shown:
    raise SystemExit('no frame shows these VBLs')

runs, cur = [], None
video = av.open(a.video)
for v, frame in enumerate(video.decode(video.streams.video[0])):
    if v > max(shown):
        break
    if v not in shown:
        continue
    ref = frame.to_ndarray(format='rgb24') // 17
    ours = np.asarray(Image.open(f'{a.frames}/{shown[v]:05d}.ppm'))[Y0:Y0 + H, X0:X0 + W] // 16
    ref[2:9, 398:416] = ours[2:9, 398:416]  # the drive LED
    diff = (ours != ref).any(2)
    bad = int(diff.sum())
    if cur and cur[2] == bad and cur[1] == v - 1:
        cur[1] = v
    else:
        cur = [v, v, bad]
        runs.append(cur)
        if a.diff and bad:
            d = np.zeros_like(ref)
            d[diff] = (15, 0, 15)
            Image.fromarray((np.concatenate([ref, ours, d], 1) * 17).astype(np.uint8)).save(
                f'{a.diff}/diff_{v}.png')

for i, j, bad in runs:
    print(f'VBL {i:5d}-{j:5d} (ours {shown[i]:5d}-{shown[j]:5d}): {bad:6d} px differ')
print(f'{len(shown)} frames compared, {sum(j - i + 1 for i, j, bad in runs if bad)} with differences')
