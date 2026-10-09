#!/usr/bin/env python3
"""Compares frames rendered by render.ts with the Hatari reference recording.
usage: compare.py <ours.rgb> <reference video.mkv> <first reference frame> [diff_dir]
Prints, for each run of frames, how many pixels differ; writes side-by-side PNGs of
the first differing frame of each run into diff_dir.
Hatari's drive LED (top right of the frame) is ignored."""
import sys, subprocess, numpy as np

W, H = 416, 276
ours = np.fromfile(sys.argv[1], np.uint8).reshape(-1, H, W, 3)
first = int(sys.argv[3])
n = len(ours)
raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', sys.argv[2], '-vf',
                      f"select='between(n\\,{first}\\,{first + n - 1})'", '-vsync', '0',
                      '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True, check=True).stdout
ref = np.frombuffer(raw, np.uint8).reshape(-1, H, W, 3)[:n].copy()
ref[:, 2:9, 398:416] = ours[:len(ref), 2:9, 398:416]  # drive LED
diff_dir = sys.argv[4] if len(sys.argv) > 4 else None
runs, cur = [], None
for i in range(len(ref)):
    bad = int((np.abs(ours[i].astype(int) - ref[i]).sum(2) > 0).sum())
    if cur and cur[2] == bad:
        cur[1] = i
    else:
        cur = [i, i, bad]
        runs.append(cur)
for a, b, bad in runs:
    print(f'ref {first + a:6d}-{first + b:6d} ours {a:5d}-{b:5d}: {bad:6d} px differ')
    if diff_dir and bad:
        from PIL import Image
        m = (np.abs(ours[a].astype(int) - ref[a]).sum(2) > 0)
        d = np.zeros_like(ref[a]); d[m] = (255, 0, 255)
        Image.fromarray(np.concatenate([ref[a], ours[a], d], 1)).save(f'{diff_dir}/diff_{first + a}.png')
total = sum((b - a + 1) for a, b, bad in runs if bad)
pixels = sum((b - a + 1) * bad for a, b, bad in runs)
print(f'{len(ref)} frames compared, {total} with differences ({pixels} differing pixels in all)')
