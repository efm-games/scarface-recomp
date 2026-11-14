import struct
d=open(r'assets\default.xbe','rb').read()
magic,base,size,enc,ts,addr=struct.unpack_from('<IIIIII',d,0)
print(hex(magic),hex(base),size,hex(enc),hex(addr))
ns=struct.unpack_from('<I',d,0x20)[0]
print('sections',ns)
for i in range(ns):
    off=0x20+4+i*0x38
    flags,va,vsize,raw,rsize=struct.unpack_from('<IIIII',d,off)
    print(i,hex(flags),'va',hex(va),'vsize',hex(vsize),'raw',hex(raw),'rsize',hex(rsize))
