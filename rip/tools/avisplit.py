#!/usr/bin/env python3
"""Streams a broken (>4GB, unfinalized) Hatari BMP AVI: writes half-size frames to
an ffv1 mkv (via ffmpeg) and the audio to a wav.  usage: avisplit.py in.avi outdir"""
import sys, struct, subprocess, wave, numpy as np
src, out = sys.argv[1], sys.argv[2]
f = open(src, 'rb'); b = f.read(1 << 20); f.seek(b.find(b'movi') + 4)
W, H = 832, 552
ff = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'bgr24', '-s', '%dx%d' % (W // 2, H // 2),
                       '-r', '50', '-i', '-', '-c:v', 'ffv1', '-level', '3', out + '/video.mkv'], stdin=subprocess.PIPE)
wv = wave.open(out + '/audio.wav', 'wb'); wv.setnchannels(2); wv.setsampwidth(2); wv.setframerate(44100)
nf = na = 0
while True:
    h = f.read(8)
    if len(h) < 8: break
    cid, sz = h[:4], struct.unpack('<I', h[4:])[0]
    d = f.read(sz)
    if sz & 1: f.read(1)
    if len(d) < sz: break
    if cid == b'00db':
        stride = (W * 3 + 3) & ~3
        a = np.frombuffer(d[-stride * H:], np.uint8).reshape(H, stride)[::-1, :W * 3].reshape(H, W, 3)[::2, ::2]
        ff.stdin.write(np.ascontiguousarray(a).tobytes()); nf += 1
    elif cid == b'01wb':
        wv.writeframes(d); na += len(d) // 4
    elif cid in (b'LIST', b'JUNK', b'idx1', b'RIFF'):
        if cid in (b'LIST', b'RIFF'): f.seek(-sz + 4, 1)  # descend into list
ff.stdin.close(); ff.wait(); wv.close()
print('frames', nf, 'audio samples', na, 'seconds', na / 44100)
