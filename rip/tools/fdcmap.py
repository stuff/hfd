#!/usr/bin/env python3
# usage: fdcmap.py <hatari --trace fdc output>  -> which disk sectors are loaded where in RAM
import re,sys
rows=[]
for l in open(sys.argv[1]):
    if 'type II read sector' in l:
        g=dict(re.findall(r'(\w+)=(0x[0-9a-fA-F]+|[\w]+)',l))
        t=g.get('head_track') or g.get('track')
        rows.append((int(g['VBL']),int(t,16),int(g['side']),int(g['sector'],16),int(g['addr'],16),g['pc']))
runs=[]
for v,t,s,sec,a,pc in rows:
    lin=(t*2+s)*10+sec-1
    if runs and runs[-1]['next']==lin and runs[-1]['anext']==a:
        r=runs[-1]; r['n']+=1; r['next']=lin+1; r['anext']=a+512; r['vend']=v
    else:
        runs.append(dict(v=v,vend=v,t=t,s=s,sec=sec,lin=lin,n=1,next=lin+1,a=a,anext=a+512,pc=pc))
for r in runs: print('VBL %5d-%5d  T%02d S%d sec%2d (img off %06x) x%3d secs -> RAM %06x-%06x pc=%s'%(r['v'],r['vend'],r['t'],r['s'],r['sec'],r['lin']*512,r['n'],r['a'],r['anext'],r['pc']))
print(len(rows),'sectors read')
