"""Finds strings in the XBE and reports the code that references them, with the real virtual addresses.

The XBE section headers carry absolute virtual addresses (image base 0x10000 included), so a file offset
must be translated through its section, not by subtracting the base.
"""
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
XBE = ROOT / "assets" / "default.xbe"
GEN = ROOT / "generated" / "game"
# name, file start, virtual address (absolute, image base included), size in file
SECS = [
    ("text", 0x1000, 0x11000, 0x31847C),
    ("rdata", 0x399000, 0x39D240, 0x7A23C),
    ("data", 0x414000, 0x4174A0, 0x50748),
]
DATA = XBE.read_bytes()
SOURCES = "\n".join(p.read_text("utf-8", "replace") for p in GEN.glob("*.cpp"))


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


def lookup(name):
    want = name.encode() if isinstance(name, str) else name
    pos = DATA.find(want)
    while pos >= 0 and DATA[pos - 1:pos] != b"\0":
        pos = DATA.find(want, pos + 1)
    if pos < 0:
        print(f"{want.decode()}: not in the xbe")
        return
    va = vaddr(pos)
    hits = len(re.findall(r"0x%08X" % va, SOURCES, re.IGNORECASE))
    ptrs = []
    needle = struct.pack("<I", va)
    s = 0
    while True:
        h = DATA.find(needle, s)
        if h < 0:
            break
        ptrs.append(h)
        s = h + 1
    print(f"{want.decode():<26} file 0x{pos:06X} va 0x{va:08X} code refs {hits} "
          f"pointer slots {[hex(x) for x in ptrs[:8]]}")


if __name__ == "__main__":
    names = sys.argv[1:] or [
        "WheeledVehicleActionMap", "CharacterActionMap", "BoatActionMap",
        "tInputActionMap", "InputActionMap", "tInputManager", "InputManager",
        "tInputMap", "ActionMap", "SetAnalogDeadZone",
    ]
    for n in names:
        lookup(n)
