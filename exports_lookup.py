import struct, sys

dll = r"C:\Users\elton\Downloads\Scarface\out\build\release\Scarface.dll"

# RVAs to resolve: default is the crash's DLL frames from scarface_002.log
targets = [int(a, 16) for a in sys.argv[1:]] or [0x79812A, 0x7989B1, 0x76426E, 0x7649AA, 0x769879, 0x768B26]

d = open(dll, "rb").read()
pe = struct.unpack_from("<I", d, 0x3C)[0]
assert d[pe:pe + 4] == b"PE\0\0", d[pe:pe + 4]
machine, nsec, _, _, _, optsize, _ = struct.unpack_from("<HHIIIHH", d, pe + 4)
opt = pe + 24
magic = struct.unpack_from("<H", d, opt)[0]
dd = opt + (112 if magic == 0x20B else 96)
exp_rva, exp_size = struct.unpack_from("<II", d, dd)

# section table for rva->file offset
secs = []
so = opt + optsize
for i in range(nsec):
    o = so + i * 40
    name = d[o:o + 8].rstrip(b"\0").decode("latin1")
    vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", d, o + 8)
    secs.append((name, va, max(vsize, rawsize), rawptr))

def rva2off(rva):
    for name, va, size, raw in secs:
        if va <= rva < va + size:
            return raw + (rva - va)
    raise ValueError(hex(rva))

e = rva2off(exp_rva)
_, _, _, name_rva, base, nfuncs, nnames, funcs_rva, names_rva, ords_rva = struct.unpack_from("<IIIIIIIIII", d, e)

funcs = list(struct.unpack_from("<%dI" % nfuncs, d, rva2off(funcs_rva)))
names = []
for i in range(nnames):
    nrva = struct.unpack_from("<I", d, rva2off(names_rva) + i * 4)[0]
    o = rva2off(nrva)
    end = d.index(b"\0", o)
    nm = d[o:end].decode("latin1")
    ordi = struct.unpack_from("<H", d, rva2off(ords_rva) + i * 2)[0]
    names.append((funcs[ordi], nm))

# every function RVA, name where present
by_rva = {}
for i, r in enumerate(funcs):
    if r:
        by_rva.setdefault(r, "ord_%d" % (base + i))
for r, nm in names:
    if r:
        by_rva[r] = nm

addrs = sorted(by_rva)
print("exports:", nfuncs, "named:", nnames, "code range: %X..%X" % (addrs[0], addrs[-1]))
for target in targets:
    lo = None
    for a in addrs:
        if a <= target:
            lo = a
        else:
            break
    hi = next((a for a in addrs if a > target), None)
    nm = by_rva.get(lo) if lo is not None else None
    print("RVA %08X -> %s + %#x   (next export %s)" % (target, nm, target - lo if lo is not None else 0,
                                                        "%X" % hi if hi else "none"))
