import re, sys, bisect

txt = open(sys.argv[1], encoding='utf-8', errors='ignore').read()
rows = re.findall(r'Name: (\S+)\s*\n\s*RVA: 0x([0-9A-Fa-f]+)', txt)
ex = sorted((int(r, 16), n) for n, r in rows)
addrs = [a for a, _ in ex]
for arg in sys.argv[2:]:
    t = int(arg, 16)
    i = bisect.bisect_right(addrs, t) - 1
    if i >= 0:
        a, n = ex[i]
        print(f"{t:08X} -> {n} +0x{t - a:X}")
    else:
        print(f"{t:08X} -> ?")
