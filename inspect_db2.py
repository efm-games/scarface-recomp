import sqlite3, os, json

dst = r"C:\Users\elton\AppData\Local\Temp\opencode\oc.db"
con = sqlite3.connect(dst)
con.row_factory = sqlite3.Row
cur = con.cursor()

print("== sessions ==")
for r in cur.execute("select id, title, directory, model, tokens_input, tokens_output, tokens_reasoning, tokens_cache_read, tokens_cache_write, cost, time_created, time_updated from session_v2 order by time_updated desc limit 10"):
    print(dict(r))

print("\n== part types ==")
for r in cur.execute("select type, count(*) c from session_part group by type"):
    print(dict(r))

print("\n== last assistant messages ==")
rows = list(cur.execute("select id, session_id, type, seq, data from session_message order by time_created desc limit 8"))
for r in rows:
    d = json.loads(r["data"]) if r["data"] else {}
    print(r["id"], r["type"], r["seq"], {k: d.get(k) for k in ("role", "tokens", "modelID", "providerID", "time") if k in d})

print("\n== step-finish parts (last 5) ==")
for r in cur.execute("select message_id, session_id, data from session_part where type like '%finish%' or type like '%usage%' or type like '%token%' order by time_created desc limit 10"):
    print(r["type"] if "type" in r.keys() else "", r["message_id"], r["data"][:500])
