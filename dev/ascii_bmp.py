import sys, struct
p=sys.argv[1]
d=open(p,'rb').read()
off=struct.unpack_from('<I',d,10)[0]; w=struct.unpack_from('<i',d,18)[0]; h=struct.unpack_from('<i',d,22)[0]; bpp=struct.unpack_from('<H',d,28)[0]
top_down = h < 0; h=abs(h)
stride=((w*bpp//8)+3)//4*4
def pix(x,y):
    yy = y if top_down else h-1-y
    i=off+yy*stride+x*(bpp//8)
    return (d[i]+d[i+1]+d[i+2])//3
C=' .:-=+*#%@'
cols,rows=95,48
print(f'{w}x{h} bpp {bpp} top_down {top_down}')
mx=0
for ry in range(rows):
    line=''
    for rx in range(cols):
        x0=rx*w//cols; x1=max(x0+1,(rx+1)*w//cols)
        y0=ry*h//rows; y1=max(y0+1,(ry+1)*h//rows)
        m=0;n=0;c=0
        for y in range(y0,y1):
            for x in range(x0,x1):
                v=pix(x,y); m=max(m,v); n+=v; c+=1
        mx=max(mx,m)
        v=max(n//c, m//3)
        line+=C[min(len(C)-1,v*len(C)//256)]
    print(line)
print('max', mx)
