#!/usr/bin/env python3
"""Fit a compiled SDK module onto the game: locate its functions (findfn logic), infer the
addresses of the ones that did not match from their SDK-order neighbours, and diff those.
usage: modfit.py <file.s from sncc> [--diffs N]"""
import sys, os, re, struct, subprocess, tempfile, collections, importlib.util
HERE=os.path.dirname(os.path.abspath(__file__)); BIN=os.path.join(HERE,"mipsel-elf-binutils","bin")
GAME=os.environ.get("GAME_EXE",os.path.join(HERE,"..","original","partition","GAME.EXE")); LOAD=0x800c4000; CODE_END=0x8016dc4c
from capstone import *
md=Cs(CS_ARCH_MIPS, CS_MODE_MIPS32|CS_MODE_LITTLE_ENDIAN)
def relmask(t):
    if t in ("R_MIPS_HI16","R_MIPS_LO16","R_MIPS_GPREL16","R_MIPS_LITERAL"): return 0xffff0000
    if t=="R_MIPS_26": return 0xfc000000
    return 0xffffffff
def main():
    src=sys.argv[1]; ndiff=int(sys.argv[sys.argv.index('--diffs')+1]) if '--diffs' in sys.argv else 6
    spec=importlib.util.spec_from_file_location("sn2gas",os.path.join(HERE,"sn2gas.py")); m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    conv=tempfile.mktemp(suffix=".s"); open(conv,"w").write(m.convert(open(src).readlines())); obj=tempfile.mktemp(suffix=".o")
    subprocess.check_call([os.path.join(BIN,"mipsel-elf-as"),"-EL","-mips4","-mabi=32","-G0","-o",obj,conv])
    syms=subprocess.check_output([os.path.join(BIN,"mipsel-elf-nm"),"-S","--defined-only",obj]).decode()
    funcs=sorted((int(p[0],16),int(p[1],16),p[3]) for p in (l.split() for l in syms.splitlines()) if len(p)>=4 and p[2] in "tT")
    hdr=subprocess.check_output([os.path.join(BIN,"mipsel-elf-objdump"),"-h",obj]).decode()
    toff=int(re.search(r"\.text\s+([0-9a-f]+)\s+[0-9a-f]+\s+[0-9a-f]+\s+([0-9a-f]+)",hdr).group(2),16); od=open(obj,"rb").read()
    rel={}
    for line in subprocess.check_output([os.path.join(BIN,"mipsel-elf-objdump"),"-r","-j",".text",obj]).decode().splitlines():
        p=line.split()
        if len(p)>=3 and re.match(r"^[0-9a-f]+$",p[0]) and p[1].startswith("R_"): rel[int(p[0],16)]=p[1]
    game=open(GAME,"rb").read()[8:(CODE_END-LOAD)+8]; gw=struct.unpack("<%dI"%(len(game)//4),game)
    index=collections.defaultdict(list)
    for i,w in enumerate(gw): index[w].append(i)
    words={}
    for addr,size,name in funcs:
        words[name]=[(struct.unpack("<I",od[toff+addr+4*k:toff+addr+4*k+4])[0], relmask(rel.get(addr+4*k,""))) for k in range(size//4)]
    loc={}
    for addr,size,name in funcs:
        ws=words[name]; n=len(ws)
        anchors=[(k,w) for k,(w,mk) in enumerate(ws) if mk==0xffffffff and w!=0 and len(index.get(w,()))<2000][:3]
        if n<6 or not anchors: continue
        k0,w0=anchors[0]; cands=[p-k0 for p in index[w0] if p-k0>=0 and p-k0+n<=len(gw)]
        for k,w in anchors[1:]: cands=[c for c in cands if gw[c+k]==w]
        hits=[c for c in cands if all((gw[c+k]&mk)==(w&mk) for k,(w,mk) in enumerate(ws))]
        if len(hits)==1: loc[name]=LOAD+4*hits[0]
    # infer addresses for the rest from neighbours (SDK order): after the previous function's end
    names=[f[2] for f in funcs]; sizes={f[2]:f[1] for f in funcs}
    guess={}
    for i,n in enumerate(names):
        if n in loc: continue
        # previous located neighbour
        for j in range(i-1,-1,-1):
            if names[j] in loc: 
                a=loc[names[j]]+sizes[names[j]]
                for k in range(j+1,i): a+=sizes[names[k]]
                guess[n]=a; break
    total=0; okc=0
    for addr,size,name in funcs:
        n=size//4; total+=n
        if name in loc: okc+=n; print("%-32s 0x%08x %4d  100%%"%(name,loc[name],n)); continue
        if name not in guess: print("%-32s ?          %4d  (no neighbour)"%(name,n)); continue
        a=guess[name]; ws=words[name]; ok=0; diffs=[]
        for k,(w,mk) in enumerate(ws):
            g=gw[(a-LOAD)//4+k] if (a-LOAD)//4+k<len(gw) else 0
            if (g&mk)==(w&mk): ok+=1
            else:
                x=next(iter(md.disasm(struct.pack("<I",w),a+4*k)),None); y=next(iter(md.disasm(struct.pack("<I",g),a+4*k)),None)
                diffs.append("     %08x ours: %-26s theirs: %s"%(a+4*k,(x.mnemonic+" "+x.op_str) if x else "%08x"%w,(y.mnemonic+" "+y.op_str) if y else "%08x"%g))
        okc+=ok; print("%-32s 0x%08x %4d  %3d%% (guessed addr)"%(name,a,n,100*ok//n))
        for d in diffs[:ndiff]: print(d)
    print("# %s: %d/%d functions located, %d/%d insns"%(os.path.basename(src),len(loc),len(funcs),okc,total))
main()
