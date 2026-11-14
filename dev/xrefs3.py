import struct,re,bisect
from pathlib import Path
d=Path(r'assets\default.xbe').read_bytes()
u32=lambda o:struct.unpack_from('<I',d,o)[0]
base,hsize,isize=u32(0x104),u32(0x108),u32(0x10C)
img=bytearray(isize); img[:hsize]=d[:hsize]
for i in range(u32(0x11C)):
    va,vs,raw,rs=struct.unpack_from('<IIII',d,u32(0x120)-base+i*56+4)
    n=min(vs,rs); img[va-base:va-base+n]=d[raw:raw+n]
mapping=Path(r'generated\game\x86_func_mapping.cpp').read_text()
funcs=sorted((int(a,16),n) for a,n in re.findall(r'\{0x([0-9A-F]+)u, (sub_[0-9A-F]+)\}',mapping))
starts=[a for a,_ in funcs]
def func_of(va):
    i=bisect.bisect_right(starts,va)-1
    return funcs[i] if i>=0 else None
for ref in [0x22f9cb,0x22f8f7,0x22f8a4,0x22f862,0x22f82a,0x35514e,0x22f716,0x22feb8,0x22fea4,0x22fd3d,0x230028,0x31fb7f,0x3e2bd3,0x231507,0x231595]:
    print(hex(ref),func_of(ref))
print('refs to table 0x44e960-0x44e9a0')
for target in range(0x44e960,0x44e9a0,4):
    for m in re.finditer(re.escape(struct.pack('<I',target)),bytes(img)):
        off=base+m.start()
        if off<isize: print(hex(target),'->',hex(off),func_of(off))
# strings table
for a in range(0x44e900,0x44e9b0,4):
    v=struct.unpack_from('<I',img,a-base)[0]
    print(hex(a),hex(v),end=' ')
    if a%16==12: print()
print()
# dump around 0x44e950
for o in range(0x44e950,0x44e9a0,16):
    b=img[o-base:o-base+16]
    print(f'{o:08X}',b.hex(' '),''.join(chr(c) if 32<=c<127 else '.' for c in b))
