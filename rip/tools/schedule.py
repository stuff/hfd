#!/usr/bin/env python3
"""Builds a chained Hatari debugger schedule (only one breakpoint armed at a time,
so the emulation stays fast).
usage: schedule.py <dir> "<vbl:scancode,...>" "<capture vbls>"
 - key events: key down at vbl, key up 8 VBLs later (needs hatari-debugger-key.patch)
 - captures: 'info video' + screenshot s_<vbl>.bmp + RAM dump m_<vbl>.bin ($0-$80000)
Writes <dir>/pre.ini, to be used with hrun.sh <dir> ..."""
import sys, os
D = os.path.abspath(sys.argv[1]); os.makedirs(D, exist_ok=True)
ev = {}
for e in filter(None, sys.argv[2].split(',')):
    v, c = e.split(':'); v = int(v)
    ev.setdefault(v, []).append('key %s d' % c); ev.setdefault(v + 8, []).append('key %s u' % c)
for v in map(int, sys.argv[3].split()):
    ev.setdefault(v, []).append('echo VBLMARK %d\ninfo video\nscreenshot %s/s_%d.bmp\nsavebin %s/m_%d.bin 0 $80000' % (v, D, v, D, v))
vs = sorted(ev)
for a, b in zip(vs, vs[1:] + [None]):
    s = '\n'.join(ev[a]) + '\n'
    if b: s += 'b VBL=%d :quiet :once :file %s/e_%d.ini\n' % (b, D, b)
    open('%s/e_%d.ini' % (D, a), 'w').write(s)
open(D + '/pre.ini', 'w').write('b VBL=%d :quiet :once :file %s/e_%d.ini\n' % (vs[0], D, vs[0]))
print(len(vs), 'events, last at VBL', vs[-1])
