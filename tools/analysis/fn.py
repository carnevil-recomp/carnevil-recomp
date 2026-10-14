#!/usr/bin/env python3
"""Print a function's generated assembly with string/data contents shown inline.
usage: fn.py <name|0xaddr> [--raw]   (reads asm/functions/*.s and the original image)"""
import sys, os, glob, re, struct
LOAD=0x800c4000; END=0x801a4224
img=open('original/partition/GAME.EXE','rb').read()[8:]
def sym_addr():
    m={}
    for l in open('config/symbols_abs.ld'):
        mm=re.match(r'(\w+) = (0x[0-9a-f]+);',l)
        if mm: m[mm.group(1)]=int(mm.group(2),16)
    for f in glob.glob('asm/data/*.s'):
        a=int(re.search(r'data_([0-9a-f]{8})',f).group(1),16); off=0
        for l in open(f):
            t=l.split('#')[0].strip()
            if t.endswith(':'): m[t[:-1]]=a+off
            elif t.startswith('.4byte'): off+=4*(t.count(',')+1)
            elif t.startswith('.byte'): off+=t.count(',')+1
    return m
def cstr(a):
    s=img[a-LOAD:a-LOAD+80].split(b'\0')[0]
    return '"'+s.decode('latin1').replace('\n','\\n')+'"' if s and all(32<=c<127 or c in (10,13,9) for c in s) else None
def main():
    key=sys.argv[1]; files=glob.glob('asm/functions/*.s')
    if key.startswith('0x'): f=[x for x in files if os.path.basename(x).startswith(key[2:].lower()+'_')]
    else: f=[x for x in files if os.path.basename(x)[9:-2]==key]
    if not f: sys.exit("no such function")
    syms=sym_addr(); text=open(f[0]).read()
    for l in text.splitlines():
        out=l
        for m in re.finditer(r'%(hi|lo)\(([A-Za-z_]\w*)(?:\+0x([0-9a-f]+))?\)',l):
            n=m.group(2); a=syms.get(n)
            if a is None:
                mm=re.match(r'D_([0-9a-f]{8})$',n)
                if mm: a=int(mm.group(1),16)
            if a is not None and m.group(1)=='lo':
                a+=int(m.group(3) or '0',16)
                if LOAD<=a<END:
                    s=cstr(a); w=struct.unpack('<I',img[a-LOAD:a-LOAD+4])[0] if a+4<=END else 0
                    out+="   # 0x%08x %s"%(a, s if s else "= 0x%08x"%w)
        print(out)
main()
