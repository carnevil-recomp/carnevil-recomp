#!/usr/bin/env python3
"""Which DIAG functions reference which strings: every D_<addr> an asm_diag function names, and the
string at that address. usage: strref.py [FUNC_ADDR_PREFIX | --grep TEXT]"""
import sys, os, re, glob, struct
d=open('original/partition/DIAG.EXE','rb').read()[8:]; L=0x80100000
def s_at(a):
    o=a-L
    if not (0<=o<len(d)): return None
    e=d.find(b'\0',o); t=d[o:e]
    if 2<=len(t)<200 and all(32<=c<127 or c in (9,10) for c in t): return t.decode()
    return None
files=sorted(glob.glob('asm_diag/functions/*.s'))
want=sys.argv[1] if len(sys.argv)>1 and not sys.argv[1].startswith('--') else None
grep=sys.argv[2] if len(sys.argv)>2 and sys.argv[1]=='--grep' else None
for f in files:
    b=os.path.basename(f)
    if want and not b.startswith(want): continue
    refs=[]
    for m in re.finditer(r'%lo\(D_([0-9a-f]{8})(?:\+0x([0-9a-f]+))?\)',open(f).read()):
        a=int(m.group(1),16)+int(m.group(2) or '0',16); t=s_at(a)
        if t is not None and (a,t) not in refs: refs.append((a,t))
    if grep: refs=[r for r in refs if grep.lower() in r[1].lower()]
    if refs:
        print(b[:-2]); [print("   %08x %r"%r) for r in refs]
