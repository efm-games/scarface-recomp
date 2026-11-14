import re
import html

s = open(r"D:\ScarfaceRecomp\controls.html", encoding="utf-8", errors="replace").read()
tables = re.findall(r"<table.*?</table>", s, re.S)
print("tables", len(tables))
for ti, t in enumerate(tables[:2]):
    print("==== TABLE", ti, "====")
    rows = re.findall(r"<tr.*?</tr>", t, re.S)
    for r in rows:
        cells = re.findall(r"<td.*?</td>", r, re.S)
        if not cells:
            cells = re.findall(r"<th.*?</th>", r, re.S)
        if not cells:
            continue
        vals = []
        for c in cells:
            parts = re.findall(r'alt="([^"]*)"|<img[^>]*src="[^"]*/([^/"?]+)"|>([^<>]+)<', c)
            out = []
            for a, b, txt in parts:
                if a:
                    out.append(a)
                elif b:
                    out.append(b)
                elif txt.strip():
                    out.append(html.unescape(txt.strip()))
            vals.append(" | ".join([x for x in out if x]))
        print(" || ".join(vals))
