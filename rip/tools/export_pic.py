# Export a picture from screen RAM + Hatari screenshot: <name>.png (as displayed, rasters included),
# <name>_indexed.png (original pixel indices, most frequent palette), <name>.json (palette, STE words,
# per-line palettes for raster colours), optional Degas .pi1 of the full screen.
import sys, json, os
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stscreen import planar, loadbmp, align

def ste_word(rgb):
    w = 0
    for sh, c in ((8, rgb[0]), (4, rgb[1]), (0, rgb[2])):
        n = round(c / 17)
        w |= (((n & 1) << 3) | (n >> 1)) << sh
    return w

def export(name, mem, base, bmp_rgb, outdir, box=None, pi1=False, dxdy=(48, 29), h=200):
    idx = planar(mem, base, h=h)
    dx, dy = dxdy
    disp = bmp_rgb[dy:dy+h, dx:dx+320].astype(np.uint8)
    if box is None:
        nz = idx != 0
        ys = np.where(nz.any(axis=1))[0]; xs = np.where(nz.any(axis=0))[0]
        box = (int(xs.min()), int(ys.min()), int(xs.max())+1, int(ys.max())+1)
    x0, y0, x1, y1 = box
    sub = idx[y0:y1, x0:x1]; sd = disp[y0:y1, x0:x1]
    # base palette: most frequent colour per index inside the box
    pal = {}
    lines = []
    for y in range(sub.shape[0]):
        lp = {}
        for i, c in zip(sub[y], sd[y]):
            lp.setdefault(int(i), '#%02x%02x%02x' % tuple(int(v) for v in c))
        lines.append(lp)
    for i in range(16):
        cnt = {}
        for lp in lines:
            if i in lp: cnt[lp[i]] = cnt.get(lp[i], 0) + 1
        pal[i] = max(cnt, key=cnt.get) if cnt else '#000000'
    varying = sorted({i for lp in lines for i, c in lp.items() if c != pal[i]})
    os.makedirs(outdir, exist_ok=True)
    Image.fromarray(sd).save(os.path.join(outdir, name + '.png'))
    p = Image.fromarray(sub.astype(np.uint8), 'P')
    flat = []
    for i in range(16):
        h = pal[i]; flat += [int(h[1:3], 16), int(h[3:5], 16), int(h[5:7], 16)]
    p.putpalette(flat + [0] * (768 - len(flat)))
    p.save(os.path.join(outdir, name + '_indexed.png'))
    meta = {'source_ram': hex(base + y0 * 160), 'screen_box_xywh': [x0, y0, x1 - x0, y1 - y0],
            'palette': [pal[i] for i in range(16)],
            'palette_ste_words': ['$%03x' % ste_word([int(pal[i][k:k+2], 16) for k in (1, 3, 5)]) for i in range(16)],
            'raster_indices': varying}
    if varying:
        meta['per_line_palette'] = [{str(i): lp[i] for i in sorted(lp) if i in varying} for lp in lines]
    json.dump(meta, open(os.path.join(outdir, name + '.json'), 'w'), indent=1)
    if pi1:
        hdr = (0).to_bytes(2, 'big') + b''.join(int(w[1:], 16).to_bytes(2, 'big') for w in meta['palette_ste_words'])
        open(os.path.join(outdir, name + '.pi1'), 'wb').write(hdr + mem[base:base + 32000])
    return box, varying
