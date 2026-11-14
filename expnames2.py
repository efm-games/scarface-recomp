import struct
d=open(r"out\build\release\Scarface.dll","rb").read()
pe=struct.unpack_from("<I",d,0x3C)[0]; _,nsec,_,_,_,optsize,_=struct.unpack_from("<HHIIIHH",d,pe+4); opt=pe+24
magic=struct.unpack_from("<H",d,opt)[0]; dd=opt+(112 if magic==0x20B else 96); exp_rva,_=struct.unpack_from("<II",d,dd)
secs=[]; so=opt+optsize
for i in range(nsec):
    o=so+i*40; vs,va,rs,rp=struct.unpack_from("<IIII",d,o+8); secs.append((va,max(vs,rs),rp))
def r2o(r):
    for va,s,rp in secs:
        if va<=r<va+s: return rp+(r-va)
e=r2o(exp_rva); _,_,_,name_rva,base,nfuncs,nnames,funcs_rva,names_rva,ords_rva=struct.unpack_from("<IIIIIIIIII",d,e)
funcs=list(struct.unpack_from("<%dI"%nfuncs,d,r2o(funcs_rva)))
want=["00241440","00241560","002415F0","00241690","002416C0","00241720","00241770","00241790","002417C0"]
for i in range(nnames):
    nrva=struct.unpack_from("<I",d,r2o(names_rva)+i*4)[0]; o=r2o(nrva); nm=d[o:d.index(b"\0",o)].decode("latin1")
    for w in want:
        if nm.endswith(w):
            ordi=struct.unpack_from("<H",d,r2o(ords_rva)+i*2)[0]; print("%-28s %08X"%(nm,funcs[ordi]))
