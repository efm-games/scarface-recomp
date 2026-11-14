"""Dump guest VAs (dwords) from an XBE for crash analysis: python dev/xbe_peek.py 3D863C 4BC28C ..."""
import struct
import sys

XBE = r"C:\Users\elton\Downloads\Scarface\assets\default.xbe"
d = open(XBE, "rb").read()
assert d[:4] == b"XBEH", d[:4]
base, header_size, image_size = struct.unpack_from("<III", d, 0x104)
nsec = struct.unpack_from("<I", d, 0x11C)[0]
sec_table = struct.unpack_from("<I", d, 0x120)[0] - base
secs = []
for i in range(nsec):
    o = sec_table + i * 0x38
    va, vsize, raw, rawsize = struct.unpack_from("<IIII", d, o + 4)
    secs.append((va, max(vsize, rawsize), raw, rawsize))
print("base %08X header %X image %X sections %d" % (base, header_size, image_size, nsec))
for va, size, raw, rawsize in secs:
    print("  sec %08X..%08X file %X..%X" % (va, va + size, raw, raw + rawsize))


def off(va):
    for sva, size, raw, rawsize in secs:
        if sva <= va < sva + size:
            return raw + (va - sva)
    raise ValueError("%08X" % va)


def dword(va):
    return struct.unpack_from("<I", d, off(va))[0]


for arg in sys.argv[1:]:
    va = int(arg, 16)
    o = off(va)
    vals = struct.unpack_from("<8I", d, o)
    print("%08X (file %X): %s" % (va, o, " ".join("%08X" % v for v in vals)))
