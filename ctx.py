import sqlite3, os, json, shutil

src = os.path.expanduser(r"~\.local\share\opencode\opencode.db")
dst = r"C:\Users\elton\AppData\Local\Temp\opencode\oc.db"
os.makedirs(os.path.dirname(dst), exist_ok=True)
for ext in ("", "-wal", "-shm"):
    if os.path.exists(src + ext):
        shutil.copyfile(src + ext, dst + ext)
con = sqlite3.connect(dst)
con.row_factory = sqlite3.Row
cur = con.cursor()

ctx_limit = 200_000
input_cap = 160_000

sess = list(cur.execute(
    "select id, tokens_input, tokens_output, tokens_reasoning, tokens_cache_read, tokens_cache_write "
    "from session_v2 order by time_updated desc limit 1"))[0]

last = None
for r in cur.execute("select data from session_message where type='assistant' order by time_created desc limit 20"):
    d = json.loads(r["data"]) if r["data"] else {}
    t = d.get("tokens")
    if t:
        last = t
        break

if last:
    prompt = last["input"] + last["cache"]["read"] + last["cache"]["write"]
    answer = last["output"]
    ctx = prompt + answer
else:
    ctx = sess["tokens_input"] + sess["tokens_cache_read"] + sess["tokens_output"]

print(f"context ~{ctx:,} / {ctx_limit:,} = {ctx/ctx_limit*100:.1f}% used, {ctx_limit-ctx:,} left")
print(f"input cap {input_cap:,}: ~{max(0,input_cap-ctx):,} left before request limit")
print(f"session new tokens: in {sess['tokens_input']:,} + out {sess['tokens_output']:,} + reasoning {sess['tokens_reasoning']:,}"
      f" | cache read {sess['tokens_cache_read']:,}")
