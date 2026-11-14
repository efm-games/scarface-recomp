import struct
import sys
from pathlib import Path


def read_bmp(path):
    data = Path(path).read_bytes()
    assert data[:2] == b'BM', path
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bpp = struct.unpack_from('<H', data, 28)[0]
    assert bpp == 32, (path, bpp)
    stride = width * 4
    flip = height > 0
    height = abs(height)
    return data, offset, width, height, stride, flip


def sample(path, step=8):
    data, offset, width, height, stride, flip = read_bmp(path)
    rows = []
    for y in range(0, height, step):
        row = (height - 1 - y) if flip else y
        base = offset + row * stride
        rows.append(data[base:base + width * 4:4 * step])
    return rows, width, height, step


def diff(a, b):
    ra, wa, ha, sa = sample(a)
    rb, wb, hb, sb = sample(b)
    assert (wa, ha, sa) == (wb, hb, sb)
    total = 0
    count = 0
    for row_a, row_b in zip(ra, rb):
        for i in range(0, min(len(row_a), len(row_b)), 4):
            total += abs(row_a[i] - row_b[i]) + abs(row_a[i + 1] - row_b[i + 1]) + abs(row_a[i + 2] - row_b[i + 2])
            count += 3
    return total / count


def resolve(name):
    path = Path(name)
    if path.exists():
        return str(path)
    # a bare name means a screenshot of the current tag: "veh_w_a" -> dev\veh_w_a.bmp
    for tag in ('veh', 'controls', 'input', 'fps'):
        candidate = Path('dev') / f'{tag}_{name}.bmp'
        if candidate.exists():
            return str(candidate)
    return name


if __name__ == '__main__':
    pairs = sys.argv[1:]
    for pair in pairs:
        a, b = pair.split(',')
        a, b = resolve(a), resolve(b)
        print(f'{Path(a).name} -> {Path(b).name}: {diff(a, b):.3f}')
