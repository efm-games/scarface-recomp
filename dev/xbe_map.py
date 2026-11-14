"""Maps XBE virtual addresses to file offsets and finds the input action map table.

The strings AirplaneActionMap / BoatActionMap / WheeledVehicleActionMap / CharacterActionMap are
registered somewhere as name -> map. Finding the table of pointers to those strings leads to the
code that picks the current map, which is the on-foot / in-vehicle signal the keyboard layer needs.
"""
import struct
import sys
from pathlib import Path

XBE = Path(__file__).resolve().parent.parent / "assets" / "default.xbe"


def sections(data):
    assert data[:4] == b"XBEH", "not an XBE"
    base = struct.unpack_from("<I", data, 0x104)[0]
    count = struct.unpack_from("<I", data, 0x11C)[0]
    addr = struct.unpack_from("<I", data, 0x120)[0] - base
    out = []
    for i in range(count):
        off = addr + i * 0x38
        flags, vaddr, vsize, faddr, fsize = struct.unpack_from("<IIIII", data, off)
        name_addr = struct.unpack_from("<I", data, off + 0x14)[0]
        name = ""
        if name_addr:
            no = name_addr - base
            end = data.index(b"\0", no)
            name = data[no:end].decode("latin-1")
        out.append(dict(flags=flags, va=vaddr - base, vsize=vsize, fa=faddr, fsize=fsize, name=name))
    return out


def to_file(secs, va):
    for s in secs:
        if s["va"] <= va < s["va"] + s["vsize"]:
            return s["fa"] + (va - s["va"])
    return None


def main():
    data = XBE.read_bytes()
    secs = sections(data)
    print(f"base 0x{struct.unpack_from('<I', data, 0x104)[0]:X}")
    for s in secs:
        print(
            f"{s['name']:<12} va 0x{s['va']:08X} vsize 0x{s['vsize']:X} "
            f"file 0x{s['fa']:X} size 0x{s['fsize']:X}"
        )
    # the action map names, as file offsets found by scanning the raw image
    names = [b"AirplaneActionMap", b"BoatActionMap", b"WheeledVehicleActionMap",
             b"CharacterActionMap", b"HelicopterActionMap"]
    print()
    for name in names:
        pos = data.find(name)
        while pos != -1 and data[pos - 1:pos] != b"\0":
            pos = data.find(name, pos + 1)
        if pos == -1:
            print(f"{name.decode()}: not found")
            continue
        # the string points here, but the XBE string table is not what code references: the section
        # layout gives the virtual address the game actually uses
        sec = next((s for s in secs if s["fa"] <= pos < s["fa"] + s["fsize"]), None)
        if not sec:
            print(f"{name.decode()}: file 0x{pos:X} in no section")
            continue
        va = sec["va"] + (pos - sec["fa"])
        print(f"{name.decode():<22} file 0x{pos:06X} va 0x{va:08X} section {sec['name']}")
    print()
    # any 32-bit little endian pointer to those virtual addresses anywhere in the image
    want = {}
    for name in names:
        pos = data.find(name)
        if pos == -1:
            continue
        sec = next((s for s in secs if s["fa"] <= pos < s["fa"] + s["fsize"]), None)
        if sec:
            want[sec["va"] + (pos - sec["fa"])] = name.decode()
    for va, name in want.items():
        needle = struct.pack("<I", va)
        hits = []
        start = 0
        while True:
            p = data.find(needle, start)
            if p == -1:
                break
            hits.append(p)
            start = p + 1
        for h in hits:
            sec = next((s for s in secs if s["fa"] <= h < s["fa"] + s["fsize"]), None)
            where = f"{sec['name']} va 0x{sec['va'] + (h - sec['fa']):08X}" if sec else "header"
            print(f"ptr to {name:<22} at file 0x{h:06X} ({where})")


if __name__ == "__main__":
    main()
