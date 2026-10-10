#!/usr/bin/env python3
"""Compares frames recorded by capture with the Hatari reference recording.
usage: compare.py <frames dir> <first frame> <reference video.mkv> <first reference frame> <count>
                  [--at X,Y] [--diff DIR]
Compares the ST screen (320 × 200, at (48, 29) in the reference) with the same area of
our frames, at (X, Y) (default 93,52: the port's display window in capture's frames).
Colours are compared as the 12-bit values both machines use: Hatari shows a 4-bit
component n as n × 17, vAmiga as n × 16.
Prints, for each run of frames, how many pixels differ; with --diff, writes side-by-side
PNGs (reference, ours, differences) of the first differing frame of each run."""
import argparse
import av
import numpy as np
from PIL import Image

p = argparse.ArgumentParser()
p.add_argument('frames')
p.add_argument('first', type=int)
p.add_argument('video')
p.add_argument('ref_first', type=int)
p.add_argument('count', type=int)
p.add_argument('--at', default='93,52')
p.add_argument('--diff')
a = p.parse_args()
x, y = map(int, a.at.split(','))


def reference():
    video = av.open(a.video)
    for i, frame in enumerate(video.decode(video.streams.video[0])):
        if i >= a.ref_first + a.count:
            break
        if i >= a.ref_first:
            yield frame.to_ndarray(format='rgb24')[29:229, 48:368] // 17


runs, cur = [], None
for i, ref in enumerate(reference()):
    ours = np.asarray(Image.open(f'{a.frames}/{a.first + i:05d}.ppm'))[y:y + 200, x:x + 320] // 16
    diff = (ours != ref).any(2)
    bad = int(diff.sum())
    if cur and cur[2] == bad:
        cur[1] = i
    else:
        cur = [i, i, bad]
        runs.append(cur)
        if a.diff and bad:
            d = np.zeros_like(ref)
            d[diff] = (15, 0, 15)
            Image.fromarray((np.concatenate([ref, ours, d], 1) * 17).astype(np.uint8)).save(
                f'{a.diff}/diff_{a.ref_first + i}.png')

for i, j, bad in runs:
    print(f'ref {a.ref_first + i:6d}-{a.ref_first + j:6d} ours {a.first + i:5d}-{a.first + j:5d}: {bad:6d} px differ')
frames = sum(j - i + 1 for i, j, bad in runs if bad)
print(f'{sum(j - i + 1 for i, j, _ in runs)} frames compared, {frames} with differences')
