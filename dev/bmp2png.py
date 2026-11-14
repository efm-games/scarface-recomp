import struct, sys, zlib

# Minimal BMP to PNG, so the Read tool can show screenshots. 24/32 bpp, top-down or bottom-up.
src, dst = sys.argv[1], sys.argv[2]
d = open(src, 'rb').read()
off = struct.unpack_from('<I', d, 10)[0]
w = struct.unpack_from('<i', d, 18)[0]
h = struct.unpack_from('<i', d, 22)[0]
bpp = struct.unpack_from('<H', d, 28)[0]
comp = struct.unpack_from('<I', d, 30)[0]
top_down = h < 0
h = abs(h)
assert comp in (0, 3), f'compression {comp}'
step = bpp // 8
stride = (w * step + 3) // 4 * 4
rows = []
for y in range(h):
    yy = y if top_down else h - 1 - y
    base = off + yy * stride
    line = bytearray(1)  # filter 0
    for x in range(w):
        i = base + x * step
        b, g, r = d[i], d[i + 1], d[i + 2]
        line += bytes((r, g, b))
    rows.append(bytes(line))
raw = b''.join(rows)


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xFFFFFFFF)


png = b'\x89PNG\r\n\x1a\n'
png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
png += chunk(b'IDAT', zlib.compress(raw, 6))
png += chunk(b'IEND', b'')
open(dst, 'wb').write(png)
print(f'{src} {w}x{h} -> {dst}')
