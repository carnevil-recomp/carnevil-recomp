#!/usr/bin/env python3
"""Compare a rebuilt image with the original and list the addresses of differing code words.
usage: verify.py <original.exe> <built.bin> [fallback.txt] [--add]
With --add, extends fallback.txt with the differing addresses so disasm.py emits them as .word
(only right while bootstrapping the assembly; never after C modules are registered, since their
differences are source problems)."""
import sys, struct
LOAD=0x800c4000
orig=open(sys.argv[1],'rb').read()[8:]; built=open(sys.argv[2],'rb').read()
if len(orig)!=len(built): print("SIZE MISMATCH orig %d built %d"%(len(orig),len(built)))
n=min(len(orig),len(built)); diffs=[LOAD+o for o in range(0,n-3,4) if orig[o:o+4]!=built[o:o+4]]
print("differing words:",len(diffs), "first:", ["0x%08x"%d for d in diffs[:8]])
if len(sys.argv)>3 and diffs and '--add' in sys.argv:
    old=set()
    try: old={int(x,16) for x in open(sys.argv[3]).read().split()}
    except FileNotFoundError: pass
    open(sys.argv[3],'w').write("\n".join("0x%08x"%a for a in sorted(old|set(diffs)))+"\n")
sys.exit(0 if not diffs and len(orig)==len(built) else 1)
