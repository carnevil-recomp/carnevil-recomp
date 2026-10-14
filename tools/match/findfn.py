#!/usr/bin/env python3
"""Locate compiled functions inside GAME.EXE by code pattern.

usage: findfn.py <file.s from sncc> [--min-insns N]
Assembles the file (through sn2gas), then for every function searches the game's text for a
position where all non-relocated words are identical (relocation fields masked). Prints
'name address insns' for unique hits, so SDK sources can be mapped to the binary without
prior symbols, and a per-file summary."""
import sys, os, re, struct, subprocess, tempfile, collections, importlib.util
HERE=os.path.dirname(os.path.abspath(__file__)); BIN=os.path.join(HERE,"..","mipsel-elf-binutils","bin")
GAME=os.environ.get("GAME_EXE",os.path.join(HERE,"..","..","original","partition","GAME.EXE")); LOAD=0x800c4000; CODE_END=0x8016dc4c
def relmask(t):
    if t in ("R_MIPS_HI16","R_MIPS_LO16","R_MIPS_GPREL16","R_MIPS_LITERAL"): return 0xffff0000
    if t=="R_MIPS_26": return 0xfc000000
    return 0xffffffff
def main():
    src=sys.argv[1]; minins=int(sys.argv[sys.argv.index('--min-insns')+1]) if '--min-insns' in sys.argv else 6
    spec=importlib.util.spec_from_file_location("sn2gas",os.path.join(HERE,"..","build","sn2gas.py")); m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    conv=tempfile.mktemp(suffix=".s"); open(conv,"w").write(m.convert(open(src).readlines()))
    obj=tempfile.mktemp(suffix=".o")
    r=subprocess.run([os.path.join(BIN,"mipsel-elf-as"),"-EL","-mips4","-mabi=32","-G0","-o",obj,conv],capture_output=True,text=True)
    if r.returncode: print("assemble failed:",r.stderr[:300]); sys.exit(1)
    syms=subprocess.check_output([os.path.join(BIN,"mipsel-elf-nm"),"-S","--defined-only",obj]).decode()
    funcs=[(int(p[0],16),int(p[1],16),p[3]) for p in (l.split() for l in syms.splitlines()) if len(p)>=4 and p[2] in "tT"]
    hdr=subprocess.check_output([os.path.join(BIN,"mipsel-elf-objdump"),"-h",obj]).decode()
    mm=re.search(r"\.text\s+([0-9a-f]+)\s+[0-9a-f]+\s+[0-9a-f]+\s+([0-9a-f]+)",hdr); toff=int(mm.group(2),16)
    od=open(obj,"rb").read()
    rel={}
    for line in subprocess.check_output([os.path.join(BIN,"mipsel-elf-objdump"),"-r","-j",".text",obj]).decode().splitlines():
        p=line.split()
        if len(p)>=3 and re.match(r"^[0-9a-f]+$",p[0]) and p[1].startswith("R_"): rel[int(p[0],16)]=p[1]
    game=open(GAME,"rb").read()[8:(CODE_END-LOAD)+8]; gw=struct.unpack("<%dI"%(len(game)//4),game)
    index=collections.defaultdict(list)
    for i,w in enumerate(gw): index[w].append(i)
    found=0; total=0
    for addr,size,name in sorted(funcs):
        n=size//4
        if n<minins: continue
        total+=1
        words=[(struct.unpack("<I",od[toff+addr+4*k:toff+addr+4*k+4])[0], relmask(rel.get(addr+4*k,""))) for k in range(n)]
        anchors=[(k,w) for k,(w,mk) in enumerate(words) if mk==0xffffffff and w!=0 and len(index.get(w,()))<2000][:3]
        if not anchors: continue
        k0,w0=anchors[0]; cands=[p-k0 for p in index[w0] if p-k0>=0 and p-k0+n<=len(gw)]
        for k,w in anchors[1:]: cands=[c for c in cands if gw[c+k]==w]
        hits=[c for c in cands if all((gw[c+k]&mk)==(w&mk) for k,(w,mk) in enumerate(words))]
        if len(hits)==1: found+=1; print("%-32s 0x%08x %4d"%(name,LOAD+4*hits[0],n))
        elif len(hits)>1: print("%-32s AMBIGUOUS %s"%(name,",".join("0x%08x"%(LOAD+4*h) for h in hits[:4])))
    print("# %s: %d/%d functions located"%(os.path.basename(src),found,total))
main()
