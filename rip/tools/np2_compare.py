#!/usr/bin/env python3
"""Compare the NoisePacker-2 module found in a HFD RAM dump with candidate .mod files.

usage: np2_compare.py <ram_dump.bin> <candidate.mod> [more.mod ...] [--addr 0x26000]

The demo depacks the selected song at $26000 in NoisePacker 2 format (header
with relocated sample pointers, 3-byte notes per track). NoisePacker renumbers
patterns, drops empty samples and may store voices in reverse order, so the
pattern comparison only uses the note periods of each voice, and samples are
searched byte-for-byte anywhere in the dump.
"""
import sys, struct

PER = [0,856,808,762,720,678,640,604,570,538,508,480,453,428,404,381,360,339,320,302,285,269,
       254,240,226,214,202,190,180,170,160,151,143,135,127,120,113,107,101,95,90,85,80,75,71,67,63,60,57]

def r16(d, o): return struct.unpack('>H', d[o:o+2])[0]
def r32(d, o): return struct.unpack('>L', d[o:o+4])[0]

def parse_np2(mem, p):
    nins = ((mem[p] << 4) & 0xf0) | (mem[p+1] >> 4)
    npos = r16(mem, p+2) // 2
    o = p + 8
    smp = []
    for _ in range(nins):
        smp.append(dict(addr=r32(mem, o), len=r16(mem, o+4)*2, fine=mem[o+6], vol=mem[o+7],
                        ll=r16(mem, o+12)*2, ls=r16(mem, o+14)*2))
        o += 16
    o += 4
    order = [r16(mem, o+2*i) >> 3 for i in range(npos)]
    o += 2*npos
    npat = max(order) + 1
    trk = [[r16(mem, o+8*i+2*j) for j in range(4)] for i in range(npat)]
    o += 8*npat
    pats = []
    for i in range(npat):
        voices = []
        for j in range(4):
            q = o + trk[i][j]
            voices.append(tuple(PER[min((mem[q+3*k] & 0xfe)//2, len(PER)-1)] for k in range(64)))
        pats.append(voices)
    return dict(nins=nins, order=order, smp=smp, pats=pats)

def parse_mod(d):
    smp = []
    for i in range(31):
        s = d[20+i*30:50+i*30]
        smp.append(dict(len=r16(s, 22)*2, vol=s[25], ls=r16(s, 26)*2, ll=r16(s, 28)*2))
    n = d[950]
    order = list(d[952:952+n])
    npat = max(d[952:1080]) + 1
    off = 1084 + npat*1024
    for s in smp:
        s['off'] = off; off += s['len']
    pats = []
    for i in range(npat):
        voices = []
        for j in range(4):
            cells = [d[1084+i*1024+k*16+j*4:1084+i*1024+k*16+j*4+2] for k in range(64)]
            voices.append(tuple(((c[0] & 0xf) << 8) | c[1] for c in cells))
        pats.append(voices)
    return dict(order=order, smp=smp, pats=pats, npat=npat, title=d[:20].rstrip(b'\0').decode('latin1'))

def main():
    args = sys.argv[1:]
    addr = 0x26000
    if '--addr' in args:
        i = args.index('--addr'); addr = int(args[i+1], 0); del args[i:i+2]
    mem = open(args[0], 'rb').read()
    n = parse_np2(mem, addr)
    print('RAM module: %d samples, %d positions, %d patterns' % (n['nins'], len(n['order']), len(n['pats'])))
    for f in args[1:]:
        d = open(f, 'rb').read(); m = parse_mod(d)
        best = None
        for rev in (False, True):
            idx = {}
            for i, p in enumerate(m['pats']):
                idx.setdefault(tuple(p), i)
            mp = {}
            for i, p in enumerate(n['pats']):
                key = tuple(p[::-1] if rev else p)
                if key in idx: mp[i] = idx[key]
            if best is None or len(mp) > len(best[1]): best = (rev, mp)
        rev, mp = best
        order_same = [mp.get(x, -1) for x in n['order']] == m['order']
        found = tot = 0; missing = []
        for i, s in enumerate(m['smp']):
            if s['len'] < 16: continue
            tot += 1
            if mem.find(d[s['off']+2:s['off']+s['len']]) >= 0: found += 1
            else: missing.append(i+1)
        print('%-40s patterns %2d/%2d  order identical: %-5s  samples identical %2d/%2d %s' % (
            f.split('/')[-1][:40], len(mp), len(n['pats']), order_same, found, tot,
            ('differing: %s' % missing) if missing else ''))

if __name__ == '__main__':
    main()
