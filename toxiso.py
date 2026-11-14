#!/usr/bin/env python3
"""Minimal XISO (extract-xiso / game-partition) reader.

Usage:
  python toxiso.py list  <image.iso> [path]
  python toxiso.py extract <image.iso> <outdir> [path]
  python toxiso.py extract-xbe <image.iso> <outfile>

Attribution: format documented by the Xbox scene (extract-xiso); this is an
independent minimal implementation.
"""
import os
import struct
import sys

SECTOR = 2048
MAGIC = b"MICROSOFT*XBOX*MEDIA"


class Xiso:
    def __init__(self, path):
        self.f = open(path, "rb")
        # find the volume descriptor: game partition at 0, whole dump behind video
        base = None
        for b in (0, 0x18300000):
            self.f.seek(b + 32 * SECTOR)
            if self.f.read(20) == MAGIC:
                base = b
                break
        if base is None:
            raise SystemExit("not an Xbox disc image (no MICROSOFT*XBOX*MEDIA)")
        self.base = base
        self.f.seek(base + 32 * SECTOR + 0x14)
        self.root_sector, self.root_size = struct.unpack("<II", self.f.read(8))

    def table(self, sector, size):
        self.f.seek(self.base + sector * SECTOR)
        return self.f.read(size)

    def entries(self, sector, size):
        data = self.table(sector, size)
        out = []

        def walk(off):
            if off + 14 > len(data):
                return
            left, right = struct.unpack_from("<HH", data, off)
            sec, sz, attr, nlen = struct.unpack_from("<IIBB", data, off + 4)
            name = data[off + 14:off + 14 + nlen].decode("latin-1")
            out.append((name, sec, sz, attr))
            for nxt in (left, right):
                if nxt:
                    walk(nxt * 4 - (0 if nxt else 0))
        # left/right are in 4-byte units relative to the table start
        def walk2(off):
            if off < 0 or off + 14 > len(data):
                return
            left, right = struct.unpack_from("<HH", data, off)
            sec, sz, attr, nlen = struct.unpack_from("<IIBB", data, off + 4)
            name = data[off + 14:off + 14 + nlen].decode("latin-1")
            out.append((name, sec, sz, attr))
            if left:
                walk2(left * 4)
            if right:
                walk2(right * 4)
        walk2(0)
        return out

    def root_entries(self):
        return self.entries(self.root_sector, self.root_size)

    def find(self, path):
        parts = [p for p in path.replace("\\", "/").split("/") if p]
        ents = self.root_entries()
        node = None
        for i, part in enumerate(parts):
            match = [e for e in ents if e[0].lower() == part.lower()]
            if not match:
                raise SystemExit("not found: " + "/".join(parts[: i + 1]))
            node = match[0]
            if i + 1 < len(parts):
                ents = self.entries(node[1], node[2])
        return node

    def read(self, sector, size):
        self.f.seek(self.base + sector * SECTOR)
        left = size
        chunks = []
        while left:
            chunk = self.f.read(min(left, 1 << 24))
            if not chunk:
                break
            chunks.append(chunk)
            left -= len(chunk)
        return b"".join(chunks)

    def walk(self, ents=None, prefix=""):
        ents = self.root_entries() if ents is None else ents
        for name, sec, sz, attr in ents:
            p = prefix + name
            yield p, sec, sz, attr
            if attr & 0x10:
                yield from self.walk(self.entries(sec, sz), p + "/")


def safe(name):
    bad = '<>:"|?*'
    out = "".join("_" if c in bad else c for c in name)
    return out or "_"


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    cmd, image = sys.argv[1], sys.argv[2]
    x = Xiso(image)
    if cmd == "list":
        root = sys.argv[3] if len(sys.argv) > 3 else ""
        if root:
            e = x.find(root)
            ents = x.entries(e[1], e[2])
        else:
            ents = None
        for p, sec, sz, attr in x.walk(ents, (root.rstrip("/") + "/") if root else ""):
            print(f"{'D' if attr & 0x10 else 'F'} {sz:>10} {p}")
    elif cmd == "extract-xbe":
        e = x.find("default.xbe")
        data = x.read(e[1], e[2])
        with open(sys.argv[3], "wb") as f:
            f.write(data)
        print(f"wrote {sys.argv[3]} ({len(data)} bytes)")
    elif cmd == "extract":
        outdir = sys.argv[3]
        root = sys.argv[4] if len(sys.argv) > 4 else ""
        if root:
            e = x.find(root)
            if e[3] & 0x10:
                ents = x.entries(e[1], e[2])
                base = (root.rstrip("/") + "/") if not root.endswith("/") else root
            else:
                ents = [(e[0], e[1], e[2], e[3])]
                base = ""
        else:
            ents, base = None, ""
        os.makedirs(outdir, exist_ok=True)
        n = 0
        for p, sec, sz, attr in x.walk(ents, base):
            if attr & 0x10:
                os.makedirs(os.path.join(outdir, *[safe(s) for s in p.split("/")]), exist_ok=True)
                continue
            dest = os.path.join(outdir, *[safe(s) for s in p.split("/")])
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, "wb") as f:
                f.write(x.read(sec, sz))
            n += 1
            if n % 100 == 0:
                print(f"{n} files...", flush=True)
        print(f"extracted {n} files to {outdir}")
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
