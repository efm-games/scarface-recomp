import struct
dll = r"out\build\release\Scarface.dll"
d = open(dll, "rb").read()
pe = struct.unpack_from("<I", d, 0x3C)[0]
_, nsec, _, _, _, optsize, _ = struct.unpack_from("<HHIIIHH", d, pe + 4)
opt = pe + 24
magic = struct.unpack_from("<H", d, opt)[0]
dd = opt + (112 if magic == 0x20B else 96)
exp_rva, _ = struct.unpack_from("<II", d, dd)
secs=[]
so=opt+optsize
for i in range(nsec):
    o=so+i*40; name=d[o:o+8].rstrip(b"\0").decode("latin1"); vs,va,rs,rp=struct.unpack_from("<IIII",d,o+8)
    secs.append((name,va,max(vs,rs),rp))
def r2o(r):
    for n,va,s,rp in secs:
        if va<=r<va+s: return rp+(r-va)
    raise ValueError(hex(r))
e=r2o(exp_rva)
_,_,_,name_rva,base,nfuncs,nnames,funcs_rva,names_rva,ords_rva=struct.unpack_from("<IIIIIIIIII",d,e)
funcs=list(struct.unpack_from("<%dI"%nfuncs,d,r2o(funcs_rva)))
want={"__imp__sub_00241260","__imp__sub_00241440","__imp__sub_00233340","__imp__sub_002335D0","__imp__sub_00234A61","__imp__sub_00234731"}
res={}
for i in range(nnames):
    nrva=struct.unpack_from("<I",d,r2o(names_rva)+i*4)[0]; o=r2o(nrva); end=d.index(b"\0",o); nm=d[o:end].decode("latin1")
    if nm in want:
        ordi=struct.unpack_from("<H",d,r2o(ords_rva)+i*2)[0]; res[nm]=funcs[ordi]
for k in sorted(want):
    print("%-24s RVA %08X" % (k,res.get(k,0)))
