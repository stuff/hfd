# Atari ST screen helpers: planar 4-plane decode, Hatari BMP screenshot loading,
# alignment of screen RAM with a screenshot, per-line palette recovery.
import sys
import numpy as np
from PIL import Image
def planar(mem,base,w=320,h=200,bpl=None):
    bpl=bpl or w//2
    a=np.frombuffer(mem[base:base+bpl*h],dtype='>u2').reshape(h,bpl//2)[:, :w//4]
    a=a.reshape(h,w//16,4)
    bits=np.arange(15,-1,-1)
    idx=np.zeros((h,w//16,16),dtype=np.uint8)
    for p in range(4):
        idx|=(((a[:,:,p:p+1]>>bits)&1)<<p).astype(np.uint8)
    return idx.reshape(h,w)
def loadbmp(f):
    b=Image.open(f).convert('RGB'); b=b.resize((b.size[0]//2,b.size[1]//2),Image.NEAREST)
    return np.asarray(b).astype(np.uint32)
def key(c): return (c[...,0]<<16)|(c[...,1]<<8)|c[...,2]
def align(idx,rgb):
    H,W=rgb.shape[:2]; k=key(rgb); best=None
    ys=np.arange(0,200,5)
    for dy in range(0,H-200+1):
        for dx in range(0,W-320+1):
            sub=k[dy+ys,dx:dx+320].astype(np.int64); ii=idx[ys].astype(np.int64)
            pairs=(ys[:,None]<<40)|(ii<<32)|sub
            bad=len(np.unique(pairs))-len(np.unique((ys[:,None]<<40)|(ii<<32)))
            if best is None or bad<best[0]: best=(bad,dx,dy)
    return best
def line_palettes(idx,rgb,dx,dy):
    pals=[]
    for y in range(idx.shape[0]):
        p={}
        for i,c in zip(idx[y],rgb[dy+y,dx:dx+320]): p.setdefault(int(i),tuple(int(v) for v in c))
        pals.append(p)
    return pals
if __name__=='__main__':
    mem=open(sys.argv[1],'rb').read(); base=int(sys.argv[2],0)
    rgb=loadbmp(sys.argv[3]); idx=planar(mem,base); print(align(idx,rgb))
