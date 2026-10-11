#!/usr/bin/env python3
"""Compare a compiled module object's .text/.rodata/.data bytes with the original image ranges
from config/modules.txt (relocation words in .text/.data are expected to differ when unlinked).
usage: secdiff.py <module-name> [object]   (object defaults to build/<src>.o)"""
import sys, subprocess
B='tools/mipsel-elf-binutils/bin/'
name=sys.argv[1]
for l in open('config/modules.txt'):
    f=l.split()
    if f and f[0]==name: break
else: sys.exit("no module "+name)
obj=sys.argv[2] if len(sys.argv)>2 else 'build/'+f[1][:-2]+'.o'
d=open('original/partition/GAME.EXE','rb').read()[8:]; L=0x800c4000
rng={'.text':(3,4),'.rodata':(5,6),'.data':(7,8)}
for sec,(a,b) in rng.items():
    if f[a]=='-': continue
    s,e=int(f[a],16),int(f[b],16)
    subprocess.check_call([B+'mipsel-elf-objcopy','-O','binary','--only-section='+sec,obj,'/tmp/secdiff.bin'])
    ours=open('/tmp/secdiff.bin','rb').read(); theirs=d[s-L:e-L]
    nd=sum(1 for i in range(min(len(ours),len(theirs))) if ours[i]!=theirs[i])
    print("%-8s ours %5d theirs %5d  differing bytes %d%s"%(sec,len(ours),len(theirs),nd,"" if len(ours)==len(theirs) else "  SIZE MISMATCH"))
    if sec!='.text' and (nd or len(ours)!=len(theirs)):
        for i in range(min(len(ours),len(theirs))):
            if ours[i]!=theirs[i]:
                print("   first diff +0x%x: ours %r theirs %r"%(i,ours[i:i+24],theirs[i:i+24])); break
