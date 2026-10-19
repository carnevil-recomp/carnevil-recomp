#!/usr/bin/env python3
"""Aligned diff of one function: ours (from a trymatch build, build/try/<file>.s) against the
original, opcode-level (registers and immediates dropped) so that a missing or extra stretch
shows where it is instead of shifting every line after it.
usage: aligndiff.py build/try/X.s func 0xADDR [--full]"""
import sys, os, difflib
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import match
src,name,addr=sys.argv[1],sys.argv[2],int(sys.argv[3],16)
obj=match.assemble(src); ours,_=match.func_bytes(obj,name)
game=open(match.GAME,'rb').read()[8:]; theirs=game[addr-match.LOAD:addr-match.LOAD+len(ours)+64]
def dis(b,a):
    return [(i.address,i.mnemonic,i.op_str) for i in match.md.disasm(b,a)]
A=dis(ours,addr); B=dis(theirs,addr)[:len(A)+16]
full='--full' in sys.argv
key=lambda t: t[1] if not full else t[1]+' '+t[2]
sm=difflib.SequenceMatcher(None,[key(x) for x in A],[key(x) for x in B],autojunk=False)
for op,i1,i2,j1,j2 in sm.get_opcodes():
    if op=='equal': print("   = %d insns (ours %x / theirs %x)"%(i2-i1,A[i1][0],B[j1][0])); continue
    n=max(i2-i1,j2-j1)
    for k in range(n):
        a=A[i1+k] if i1+k<i2 else None; b=B[j1+k] if j1+k<j2 else None
        print("%s %-34s | %s"%(op[0], ("%x %s %s"%a) if a else '', ("%x %s %s"%b) if b else ''))
