#!/usr/bin/env python3
# usage: keys.py <dir> "vbl:code,vbl:code,..."  -> appends key press (down at vbl, up at vbl+8) breakpoints to pre.ini
# (needs the "key" debugger command from hatari-debugger-key.patch)
# ST scancodes: space 0x39, return 0x1c, cursor up 0x48, cursor down 0x50
import sys,os
D=sys.argv[1]; ev=sys.argv[2]
os.makedirs(D,exist_ok=True)
with open(os.path.join(D,'pre.ini'),'a') as pre:
    for i,e in enumerate(ev.split(',')):
        v,c=e.split(':'); v=int(v)
        for t,ud in ((v,'d'),(v+8,'u')):
            f=os.path.join(D,'k%d%s.ini'%(i,ud))
            open(f,'w').write('key %s %s\n'%(c,ud))
            pre.write('b VBL=%d :quiet :once :file %s\n'%(t,f))
