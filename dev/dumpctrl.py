import struct
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
        print(f'{o:08X} {b.hex(" ")} {chr(32) if False else ""}{"".join(chr(c) if 32<=c<127 else "." for c in b)}')
dump(0x3D30E0,0x140)
print()
dump(0x3A4B80,0x120)
print()
dump(0x3A3490,0x80)
print()
for a in [0x3A4BA0,0x3A4BD9,0x3A4C38,0x441EA1,0x441EAF,0x44314B,0x44567F,0x4491BF,0x4491F4,0x44C733,0x39E0BC,0x3B5350]:
    print(hex(a), bytes(img[a-base:a-base+120]).split(b'\0')[0])
