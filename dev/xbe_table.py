"""Walks a table of {const char* name; ...} entries in the XBE and annotates each dword."""
import struct
import sys
from pathlib import Path

XBE = Path(__file__).resolve().parent.parent / "assets" / "default.xbe"
SECS = [
    ("text", 0x1000, 0x1000, 0x31947C),
    ("rdata", 0x399000, 0x38D240, 0x7A23C),
    ("data", 0x414000, 0x4074A0, 0x50748),
]
DATA = XBE.read_bytes()


def vaddr(f):
    for n, fa, v, sz in SECS:
        if fa <= f < fa + sz:
            return v + (f - fa)
    return None


def faddr(v):
    for n, fa, vv, sz in SECS:
        if vv <= v < vv + sz:
            return fa + (v - vv)
    return None


def cstr(v):
    f = faddr(v)
    if f is None:
        return None
    end = DATA.find(b"\x00", f)
    if end < 0 or end - f > 64:
        return None
    s = DATA[f:end]
    if len(s) > 2 and all(32 <= c < 127 for c in s):
        return s.decode()
    return None


def note(v):
    if v == 0:
        return "0"
    if v == 0xFFFFFFFF:
        return "-1"
    s = cstr(v)
    if s:
        return 'str "%s"' % s
    if 0x1000 <= v < 0x31947C:
        return "code 0x%X" % v
    if 0x38D240 <= v < 0x4074A0:
        return "rdata 0x%X" % v
    if 0x4074A0 <= v < 0x4BA698:
        return "data 0x%X" % v
    return ""


def main():
    start = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x405D80
    end = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x406280
    for off in range(start, end, 4):
        v = struct.unpack_from("<I", DATA, off)[0]
        print("0x%06X va 0x%08X %08X %s" % (off, vaddr(off) or 0, v, note(v)))


if __name__ == "__main__":
    main()
