#!/usr/bin/env python3
"""Recursive-descent 68000 disassembler for raw memory images (capstone).
usage: rdis.py image.bin start end out.s entry1,entry2,... [--name title]"""
import sys,os,re,struct
here=os.path.dirname(os.path.abspath(__file__))
for p in (os.path.join(here,'.build','pylib'),):
    if os.path.isdir(p): sys.path.insert(0,p)
import capstone
from capstone import m68k as M

HW={0xff8201:'vbase_hi',0xff8203:'vbase_mid',0xff820d:'vbase_lo',0xff820a:'sync',0xff8260:'shifter_res',0xff8240:'palette',
    0xff8604:'fdc_data',0xff8606:'dma_mode',0xff8609:'dma_hi',0xff860b:'dma_mid',0xff860d:'dma_lo',0xff8800:'psg_sel',0xff8802:'psg_wr',
    0xff8900:'snd_ctrl',0xff8901:'snd_ctrl_lo',0xff8903:'snd_start_hi',0xff8905:'snd_start_mid',0xff8907:'snd_start_lo',0xff890f:'snd_end_hi',0xff8911:'snd_end_mid',0xff8913:'snd_end_lo',0xff8921:'snd_mode',
    0xfffa01:'mfp_gpip',0xfffa07:'mfp_iera',0xfffa09:'mfp_ierb',0xfffa0b:'mfp_ipra',0xfffa0d:'mfp_iprb',0xfffa0f:'mfp_isra',0xfffa11:'mfp_isrb',0xfffa13:'mfp_imra',0xfffa15:'mfp_imrb',0xfffa17:'mfp_vr',
    0xfffa19:'mfp_tacr',0xfffa1b:'mfp_tbcr',0xfffa1f:'mfp_tadr',0xfffa21:'mfp_tbdr',0xfffc00:'acia_kbd_ctrl',0xfffc02:'acia_kbd_data',0xff8a00:'blitter'}
VEC={0x8:'v_buserr',0xc:'v_addrerr',0x10:'v_illegal',0x14:'v_zerodiv',0x24:'v_trace',0x68:'v_hbl',0x70:'v_vbl',0x80:'v_trap0',0x84:'v_trap1',0x110:'v_timerd',0x114:'v_timerc',0x118:'v_acia',0x120:'v_timerb',0x134:'v_timera'}

def main():
    img,start,end,out,ents=sys.argv[1],int(sys.argv[2],0),int(sys.argv[3],0),sys.argv[4],[int(x,0) for x in sys.argv[5].split(',')]
    title=sys.argv[sys.argv.index('--name')+1] if '--name' in sys.argv else os.path.basename(img)
    mem=open(img,'rb').read()
    md=capstone.Cs(capstone.CS_ARCH_M68K,capstone.CS_MODE_BIG_ENDIAN|capstone.CS_MODE_M68K_000); md.detail=True
    code={}   # addr -> insn
    labels={}
    work=list(ents)
    for e in ents: labels[e]='entry_%06x'%e
    def inside(a): return start<=a<end and not a&1
    while work:
        a=work.pop()
        while inside(a) and a not in code:
            try: ins=next(md.disasm(mem[a:a+10],a))
            except StopIteration: break
            code[a]=ins
            mn=ins.mnemonic
            tgt=None
            for op in ins.operands:
                if op.type==M.M68K_OP_BR_DISP: tgt=a+2+op.br_disp.disp
            if tgt is None and mn in ('jmp','jsr'):
                op=ins.operands[0]
                if op.type==M.M68K_OP_MEM and op.address_mode in (M.M68K_AM_ABSOLUTE_DATA_LONG,M.M68K_AM_ABSOLUTE_DATA_SHORT):
                    tgt=op.imm if op.imm else None
                    m=re.search(r'\$([0-9a-f]+)',ins.op_str); tgt=int(m.group(1),16) if m else None
                elif op.type==M.M68K_OP_MEM and op.address_mode==M.M68K_AM_PCI_DISP:
                    tgt=a+2+op.mem.disp
            # lea/pea pc-relative and immediate moves into vectors -> potential code
            if mn.startswith(('lea','pea')) and ins.operands[0].type==M.M68K_OP_MEM and ins.operands[0].address_mode==M.M68K_AM_PCI_DISP:
                t=a+2+ins.operands[0].mem.disp
                labels.setdefault(t,'L%06x'%t)
            if mn.startswith('move.l') and ins.op_str.startswith('#$'):
                m=re.match(r'#\$([0-9a-f]+), \$([0-9a-f]+)\.w',ins.op_str)
                if m and int(m.group(2),16) in VEC and inside(int(m.group(1),16)):
                    t=int(m.group(1),16); labels[t]='%s_handler'%VEC[int(m.group(2),16)][2:]; work.append(t)
            if tgt is not None and inside(tgt):
                if mn.startswith(('jsr','bsr')): labels.setdefault(tgt,'sub_%06x'%tgt)
                else: labels.setdefault(tgt,'L%06x'%tgt)
                work.append(tgt)
            a+=ins.size
            if mn in ('rts','rte','rtr','jmp','bra','bra.b','bra.w','illegal') or mn.startswith('bra'):
                break
    def fmt(ins):
        s=ins.op_str
        def rep(m):
            v=int(m.group(1),16); suf=m.group(2) or ''
            if v in labels: return labels[v]
            hv=v|0xffff0000 if suf=='.w' and v>=0x8000 else v
            hv&=0xffffff
            if hv in HW: return HW[hv]+suf
            if suf=='.w' and v in VEC: return VEC[v]+'.w'
            return m.group(0)
        s=re.sub(r'\$([0-9a-f]+)(\.[wl])?',rep,s)
        return '%-8s %s'%(ins.mnemonic,s)
    with open(out,'w') as f:
        f.write('; %s\n; recursive disassembly of $%06x-$%06x, entries %s\n; code bytes: %d / %d\n\n'%(title,start,end,','.join('$%x'%e for e in ents),sum(i.size for i in code.values()),end-start))
        a=start
        while a<end:
            if a in labels: f.write('\n%s:\n'%labels[a])
            if a in code:
                ins=code[a]; f.write('    %-36s ; %06x: %s\n'%(fmt(ins),a,ins.bytes.hex())); a+=ins.size
            else:
                b=a; 
                while b<end and b not in code and (b==a or b not in labels): b+=1
                blob=mem[a:b]
                for k in range(0,len(blob),16):
                    ch=blob[k:k+16]
                    asc=''.join(chr(c) if 32<=c<127 else '.' for c in ch)
                    f.write('    dc.b %-64s ; %06x %s\n'%(','.join('$%02x'%c for c in ch),a+k,asc))
                a=b
    print(out,len(code),'instructions,',len(labels),'labels')
main()
