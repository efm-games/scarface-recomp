import struct
from pathlib import Path
d=Path(r'assets\default.xbe').read_bytes()
u32=lambda o:struct.unpack_from('<I',d,o)[0]
base,hsize,isize=u32(0x104),u32(0x108),u32(0x10C)
img=bytearray(isize); img[:hsize]=d[:hsize]
for i in range(u32(0x11C)):
    va,vs,raw,rs=struct.unpack_from('<IIII',d,u32(0x120)-base+i*56+4)
    n=min(vs,rs); img[va-base:va-base+n]=d[raw:raw+n]
def s(a): return bytes(img[a-base:a-base+160]).split(b'\0')[0]
for a in [0x3D2F80,0x3D2FA8,0x3D2FC4,0x3D2FE0,0x3D2FF8,0x3D3014,0x3D3030,0x3D3048,0x3D30DC,0x3D30EC,0x3D311C,0x3D3130,0x3D3140,0x3D3158,0x3D3224,0x3D3244,0x3D325C,0x3D3284,0x3D329C,0x3D32A8,0x39F280]:
    print(hex(a),s(a))
