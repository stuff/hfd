import struct
def parse(d):
    title=d[:20].rstrip(b'\0').decode('latin1')
    smp=[]
    for i in range(31):
        s=d[20+i*30:50+i*30]
        smp.append(dict(name=s[:22].rstrip(b'\0').decode('latin1'),len=struct.unpack('>H',s[22:24])[0]*2,fine=s[24],vol=s[25],ls=struct.unpack('>H',s[26:28])[0]*2,ll=struct.unpack('>H',s[28:30])[0]*2))
    n=d[950]; order=list(d[952:952+n]); npat=max(d[952:1080])+1
    off=1084+npat*1024
    for s in smp:
        s['off']=off; off+=s['len']
    return dict(title=title,smp=smp,songlen=n,restart=d[951],order=order,npat=npat,patoff=1084,end=off,size=len(d),tag=d[1080:1084])
