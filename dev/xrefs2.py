import struct,re
from pathlib import Path
d=Path(r'assets\default.xbe').read_bytes()
u32=lambda o:struct.unpack_from('<I',d,o)[0]
base,hsize,isize=u32(0x104),u32(0x108),u32(0x10C)
img=bytearray(isize); img[:hsize]=d[:hsize]
for i in range(u32(0x11C)):
    va,vs,raw,rs=struct.unpack_from('<IIII',d,u32(0x120)-base+i*56+4)
    n=min(vs,rs); img[va-base:va-base+n]=d[raw:raw+n]
def dump(a,n):
    for o in range(a,a+n,16):
        b=img[o-base:o-base+16]
        s=''.join(chr(c) if 32<=c<127 else '.' for c in b)
        print(f'{o:08X} {b.hex(" ")} {s}')
for r in [(0x3d2d80,0x3d2e20),(0x3d2ff0,0x3d3110)]: dump(*r)
print('refs to 3d2dcc..3d30e0 in data+code')
for target in range(0x3d2dcc,0x3d30e0,4):
    pat=struct.pack('<I',target)
    for m in re.finditer(re.escape(pat),bytes(img)):
        print(hex(target),'->',hex(base+m.start()))
