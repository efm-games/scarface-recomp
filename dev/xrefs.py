import struct, re
from pathlib import Path
d=Path(r'assets\default.xbe').read_bytes()
u32=lambda o:struct.unpack_from('<I',d,o)[0]
base,hsize,isize=u32(0x104),u32(0x108),u32(0x10C)
img=bytearray(isize); img[:hsize]=d[:hsize]
for i in range(u32(0x11C)):
    va,vs,raw,rs=struct.unpack_from('<IIII',d,u32(0x120)-base+i*56+4)
    n=min(vs,rs); img[va-base:va-base+n]=d[raw:raw+n]
print('base',hex(base),'size',hex(isize),'entry',hex(u32(0x128)^0xA8FC57AB))
# find strings
for needle in [b'art/movies/', b'thx_51.bik', b'prologue_51.bik', b'movies.rcf', b'MovieFlowClient', b'attract.']:
    print('==',needle)
    for m in re.finditer(re.escape(needle), bytes(img)):
        print(hex(base+m.start()), img[m.start():m.start()+96].split(b'\0')[0])
# mapping funcs
mapping=Path(r'generated\game\x86_func_mapping.cpp').read_text()
funcs=sorted((int(a,16),n) for a,n in re.findall(r'\{0x([0-9A-F]+)u, (sub_[0-9A-F]+)\}',mapping))
import bisect
starts=[a for a,_ in funcs]
def func_of(va):
    i=bisect.bisect_right(starts,va)-1
    return funcs[i] if i>=0 else None
# scan code for refs to selected string addresses
targets={}
for needle in [b'art/movies/', b'movies.rcf', b'MovieFlowClient', b'prologue_51.bik']:
    for m in re.finditer(re.escape(needle), bytes(img)):
        targets[base+m.start()]=needle
for va,needle in targets.items():
    print('TARGET',hex(va),needle)
    pat=struct.pack('<I',va)
    for m in re.finditer(re.escape(pat), bytes(img[0x11000:0x39D22C])):
        off=0x11000+m.start(); f=func_of(off)
        print('  ref',hex(off),'func',f)
