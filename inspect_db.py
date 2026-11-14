import sqlite3, shutil, os, sys, json

src = os.path.expanduser(r"~\.local\share\opencode\opencode.db")
dst = r"C:\Users\elton\AppData\Local\Temp\opencode\oc.db"
os.makedirs(os.path.dirname(dst), exist_ok=True)
for ext in ("", "-wal", "-shm"):
    if os.path.exists(src + ext):
        shutil.copyfile(src + ext, dst + ext)
con = sqlite3.connect(dst)
con.row_factory = sqlite3.Row
cur = con.cursor()
tables = [r[0] for r in cur.execute("select name from sqlite_master where type='table'")]
print("tables:", tables)
for t in tables:
    try:
        cols = [r[1] for r in cur.execute(f"pragma table_info({t})")]
        print(t, cols)
    except Exception as e:
        print(t, e)
