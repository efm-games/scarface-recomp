import sys, struct

# crop a BMP: crop_ascii.py file bmp x0 y0 x1 y1 [cols] [rows]
p = sys.argv[1]
d = open(p, 'rb').read()
off = struct.unpack_from('<I', d, 10)[0]
w = struct.unpack_from('<i', d, 18)[0]
h = struct.unpack_from('<i', d, 22)[0]
bpp = struct.unpack_from('<H', d, 28)[0]
top_down = h < 0
h = abs(h)
stride = ((w * bpp // 8) + 3) // 4 * 4


def pix(x, y):
    yy = y if top_down else h - 1 - y
    i = off + yy * stride + x * (bpp // 8)
    return (d[i] + d[i + 1] + d[i + 2]) // 3


x0, y0, x1, y1 = map(int, sys.argv[2:6])
cols = int(sys.argv[6]) if len(sys.argv) > 6 else 110
rows = int(sys.argv[7]) if len(sys.argv) > 7 else 40
C = ' .:-=+*#%@'
print(f'crop {x0},{y0}..{x1},{y1} of {w}x{h}')
for ry in range(rows):
    line = ''
    for rx in range(cols):
        a = x0 + rx * (x1 - x0) // cols
        b = max(a + 1, x0 + (rx + 1) * (x1 - x0) // cols)
        c = y0 + ry * (y1 - y0) // rows
        e = max(c + 1, y0 + (ry + 1) * (y1 - y0) // rows)
        m = 0
        n = 0
        k = 0
        for y in range(c, e):
            for x in range(a, b):
                v = pix(x, y)
                m = max(m, v)
                n += v
                k += 1
        v = max(n // k, m // 3)
        line += C[min(len(C) - 1, v * len(C) // 256)]
    print(line)
