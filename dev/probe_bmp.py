import sys
p = sys.argv[1]
d = open(p,'rb').read()
off = int.from_bytes(d[10:14],'little')
w = int.from_bytes(d[18:22],'little'); h = int.from_bytes(d[22:26],'little')
bpp = int.from_bytes(d[28:30],'little')
print('off',off,'w',w,'h',h,'bpp',bpp)
stride = ((w*bpp//8)+3)//4*4
rows = h//2
def pix(x,y):
    yy = h-1-y
    i = off + yy*stride + x*(bpp//8)
    b,g,r = d[i],d[i+1],d[i+2]
    return (r+g+b)//3
# columns containing anything > 40
cols = [0]*w; rws=[0]*h
for y in range(0,h,2):
    for x in range(0,w,2):
        if pix(x,y) > 40:
            cols[x]+=1; rws[y]+=1
xs = [x for x in range(w) if cols[x]]
ys = [y for y in range(h) if rws[y]]
print('x range', min(xs) if xs else None, max(xs) if xs else None)
print('y range', min(ys) if ys else None, max(ys) if ys else None)
# histogram of brightness
import collections
hist = collections.Counter()
for y in range(0,h,4):
    for x in range(0,w,4):
        hist[min(pix(x,y)//32,7)] += 1
print('hist', sorted(hist.items()))
