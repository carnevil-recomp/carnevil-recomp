#!/usr/bin/env python3
"""Near matches: for each unnamed DIAG function, the named GAME.EXE function whose masked code is
most similar (difflib ratio over masked words, lengths within 30%). For naming library code that
DIAG links at another revision than the game; a name is only taken after reading both.
usage: fuzzy.py [MIN_RATIO] > report"""
import sys, struct, difflib
sys.path.insert(0,'tools/diag')
LO={0x09,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2e,0x31,0x35,0x37,0x39,0x3d,0x3f}
def mask(w):
    op=w>>26
    if op in (2,3): return w&0xfc000000
    if op==0x0f or op in LO: return w&0xffff0000
    return w
def img(p):
    d=open(p,'rb').read(); return struct.unpack('<%dI'%((len(d)-8)//4),d[8:8+(len(d)-8)//4*4])
gw=img('original/partition/GAME.EXE'); dw=img('original/partition/DIAG.EXE')
GL=0x800c4000; DL=0x80100000
minr=float(sys.argv[1]) if len(sys.argv)>1 else 0.7
names={}
for l in open('config/names.txt'):
    if l.startswith('0x'): x,n=l.split()[:2]; names[int(x,16)]=n
game=[]
for l in open('config/functions_v103.txt'):
    if not l.startswith('0x'): continue
    s,e,n=l.split()[:3]; s=int(s,16); e=int(e,16)+1; n=names.get(s,n)
    if n.startswith('sub_') or not (GL<=s<0x8016dc4c) or e-s<24: continue
    game.append((n,[mask(gw[(a-GL)//4]) for a in range(s,e&~3,4)]))
for l in open('config/diag/functions.txt'):
    if not l.startswith('0x'): continue
    s,e,n=l.split()[:3]; s=int(s,16); e=int(e,16)+1
    if not n.startswith('sub_') or e-s<24: continue
    me=[mask(dw[(a-DL)//4]) for a in range(s,e&~3,4)]
    best=(0,None)
    for gn,gm in game:
        if not (0.7*len(me)<=len(gm)<=1.3*len(me)): continue
        sm=difflib.SequenceMatcher(None,me,gm,autojunk=False)
        if sm.real_quick_ratio()<=best[0] or sm.quick_ratio()<=best[0]: continue
        r=sm.ratio()
        if r>best[0]: best=(r,gn)
    if best[0]>=minr: print("0x%08x %5d  %.2f %s"%(s,e-s,best[0],best[1])); sys.stdout.flush()
