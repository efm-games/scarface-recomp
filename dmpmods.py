import struct, sys

path = sys.argv[1] if len(sys.argv) > 1 else r"D:\ScarfaceRecomp\Scarface\out\build\release\CrashDumps\Scarface.2026.11.08_03.01.19.dmp"
d = open(path, "rb").read()

assert d[:4] == b"MDMP", d[:4]
ver, nstreams, dir_rva, chksum, tstamp, flags = struct.unpack_from("<IIIIII", d, 4)
print("version", ver, "streams", nstreams, "flags", flags)

streams = {}
for i in range(nstreams):
    st, size, rva = struct.unpack_from("<III", d, dir_rva + i * 12)
    streams.setdefault(st, (size, rva))

print("stream types", sorted(streams))

def mdstring(rva):
    (ln,) = struct.unpack_from("<I", d, rva)
    return d[rva + 4:rva + 4 + ln].decode("utf-16-le", "replace")

if 4 in streams:
    size, rva = streams[4]
    (n,) = struct.unpack_from("<I", d, rva)
    print("modules", n)
    off = rva + 4
    for i in range(n):
        base, isize, csum, ts, name_rva = struct.unpack_from("<QIIII", d, off)
        name = mdstring(name_rva)
        print(f"  {base:016X} {isize:>9X}  {name}")
        off += 108

# exception stream (6): thread id + exception record
if 6 in streams:
    size, rva = streams[6]
    tid, align = struct.unpack_from("<II", d, rva)
    er = rva + 8
    code, eflags, erec, eaddr, nparams = struct.unpack_from("<IIQQI", d, er)
    params = struct.unpack_from("<15Q", d, er + 32)
    print(f"exception tid {tid} code {code:08X} addr {eaddr:016X} nparams {nparams} params {[hex(p) for p in params[:nparams]]}")
