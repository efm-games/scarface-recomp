import re, sys
d = open(r'assets\default.xbe','rb').read()
for enc, pat in (('ascii', rb'[\x20-\x7e]{4,}'), ('utf16', rb'(?:[\x20-\x7e]\x00){4,}')):
    found = set()
    for m in re.finditer(pat, d):
        s = m.group()
        text = s.decode('ascii', 'ignore') if enc == 'ascii' else s.decode('utf-16le', 'ignore')
        low = text.lower()
        if any(k in low for k in ('bik', 'intro', 'logo', 'movie', 'attract', 'legal', 'publisher', 'credits', 'opening')):
            found.add((m.start(), text[:160]))
    print('==', enc, len(found))
    for off, text in sorted(found)[:120]:
        print(f'{off:08X} {text}')
