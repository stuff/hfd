# Render a RAM range as bitmap (planar4 or 1bpp) to spot graphics: memview.py dump start end bytes_per_row mode out.png [rows_per_col]
# memview.py dump.bin start end bytes_per_row mode(planar4|1bpp) out.png [rows_per_col]
import sys
from PIL import Image, ImageDraw
d=open(sys.argv[1],'rb').read(); a=int(sys.argv[2],0); b=int(sys.argv[3],0); bpr=int(sys.argv[4]); mode=sys.argv[5]; out=sys.argv[6]
rpc=int(sys.argv[7]) if len(sys.argv)>7 else 800
rows=(b-a)//bpr
W=bpr*2 if mode=='planar4' else bpr*8
cols=(rows+rpc-1)//rpc
img=Image.new('RGB',(cols*(W+60),rpc),(40,0,0))
px=img.load(); dr=ImageDraw.Draw(img)
pal=[(int(i*17),int(i*17),int(i*17)) for i in range(16)]
for r in range(rows):
    c,y=divmod(r,rpc); x0=c*(W+60)+60
    if y%50==0: dr.text((c*(W+60)+2,y),'%06x'%(a+r*bpr),fill=(255,255,0))
    row=d[a+r*bpr:a+(r+1)*bpr]
    if mode=='1bpp':
        for i,byte in enumerate(row):
            for k in range(8):
                if byte&(0x80>>k): px[x0+i*8+k,y]=(255,255,255)
                else: px[x0+i*8+k,y]=(0,0,0)
    else:
        for g in range(bpr//8):
            w=[(row[g*8+2*p]<<8)|row[g*8+2*p+1] for p in range(4)]
            for k in range(16):
                v=sum(((w[p]>>(15-k))&1)<<p for p in range(4))
                px[x0+g*16+k,y]=pal[v]
img.save(out); print(img.size)
