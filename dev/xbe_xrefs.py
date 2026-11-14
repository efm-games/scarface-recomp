"""Checks whether string virtual addresses appear as immediates in the recompiled sources."""
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
XBE = ROOT / "assets" / "default.xbe"
GEN = ROOT / "generated" / "game"
SECS = [
    ("text", 0x1000, 0x1000, 0x31947C),
    ("rdata", 0x399000, 0x38D240, 0x7A23C),
    ("data", 0x414000, 0x4074A0, 0x50748),
]


def vaddr(f):
    for n, fa, v, sz in SECS:
        if fa <= f < fa + sz:
            return v + (f - fa)
    return None


def main():
    data = XBE.read_bytes()
    sources = list(GEN.glob("*.cpp"))
    blob = "\n".join(p.read_text("utf-8", "replace") for p in sources)
    for name in [b"PROFILE", b"EMPIRE", b"Tutorial", b"Rage meter", b"WheeledVehicleActionMap",
                 b"CharacterActionMap", b"BoatActionMap", b"InputLeftYAxis", b"Unit"]:
        pos = data.find(name)
        if pos < 0:
            print(f"{name.decode()}: not in the xbe")
            continue
        while pos >= 0 and data[pos - 1:pos] != b"\0":
            pos = data.find(name, pos + 1)
        if pos < 0:
            print(f"{name.decode()}: not at a string start")
            continue
        va = vaddr(pos)
        hits = len(re.findall(r"0x%08X" % va, blob, re.IGNORECASE))
        print(f"{name.decode():<24} file 0x{pos:06X} va 0x{va:08X} xrefs in recomp: {hits}")


if __name__ == "__main__":
    main()
