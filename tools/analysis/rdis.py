#!/usr/bin/env python3
"""Annotated disassembly of an address range of the original image (independent of asm/):
labels from config/names.txt and the Binja export, strings/words for lui/lo pairs.
usage: dis.py <start> <end>"""
import sys, re, struct
from capstone import *
LOAD=0x800c4000; END=0x801a4224
img=open('original/partition/GAME.EXE','rb').read()[8:]
names={}
for l in open('config/binja_symbols.txt'):
    p=l.rstrip('\n').split('\t')
    if len(p)>3 and p[1] in ('func','data'): names[int(p[0],16)]=p[2]
for l in open('config/names.txt'):
    if l.startswith('0x'): a,n=l.split()[:2]; names[int(a,16)]=n
def cstr(a):
    s=img[a-LOAD:a-LOAD+60].split(b'\0')[0]
    return '"'+s.decode('latin1').replace('\n','\\n').replace('\r','\\r')+'"' if s and all(32<=c<127 or c in (10,13,9) for c in s) else None
s,e=int(sys.argv[1],16),int(sys.argv[2],16)
md=Cs(CS_ARCH_MIPS, CS_MODE_MIPS32|CS_MODE_LITTLE_ENDIAN); md.skipdata=True
hi={}
for i in md.disasm(img[s-LOAD:e-LOAD],s):
    a=i.address; lab=names.get(a)
    if lab: print("%s:"%lab)
    line="  %08x  %-8s %s"%(a,i.mnemonic,i.op_str); note=""
    if i.mnemonic=='lui':
        r,v=i.op_str.split(', '); hi[r]=int(v,16)
    m=re.match(r'(\$\w+), (-?0x[0-9a-f]+|\-?\d+)\((\$\w+)\)',i.op_str) or re.match(r'(\$\w+), (\$\w+), (-?0x[0-9a-f]+|-?\d+)$',i.op_str)
    if m and i.mnemonic in ('lw','sw','lh','sh','lb','sb','lbu','lhu','addiu','lwc1','swc1','ldc1'):
        base,off=(m.group(2),m.group(3)) if i.mnemonic=='addiu' else (m.group(3),m.group(2))
        if base in hi:
            t=((hi[base]<<16)+int(off,16))&0xffffffff
            n=names.get(t); st=cstr(t) if LOAD<=t<END else None
            note="   # %08x %s%s"%(t, n or "", (" "+st) if st else (" = 0x%08x"%struct.unpack('<I',img[t-LOAD:t-LOAD+4])[0] if LOAD<=t<END-3 and not n else ""))
    if i.mnemonic in ('jal','j'):
        t=int(i.op_str,16); n=names.get(t)
        if n: line="  %08x  %-8s %s"%(a,i.mnemonic,n)
    print(line+note)
