import struct,re
from pathlib import Path
d=Path(r'assets\default.xbe').read_bytes()
u32=lambda o:struct.unpack_from('<I',d,o)[0]
base,hsize,isize=u32(0x104),u32(0x108),u32(0x10C)
img=bytearray(isize); img[:hsize]=d[:hsize]
for i in range(u32(0x11C)):
    va,vs,raw,rs=struct.unpack_from('<IIII',d,u32(0x120)-base+i*56+4)
    n=min(vs,rs); img[va-base:va-base+n]=d[raw:raw+n]
for needle in [b'Accelerate',b'accelerate',b'Handbrake',b'handbrake',b'Brake',b'brake',b'Horn',b'horn',b'Sprint',b'sprint',b'controls',b'Controls',b'input.cfg',b'.cfg',b'Keyboard']:
    hits=[m.start() for m in re.finditer(re.escape(needle), bytes(img))]
    print(needle, len(hits), [hex(base+h) for h in hits[:8]])
