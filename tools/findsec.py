#!/usr/bin/env python3
"""Locate a compiled module's .text/.rodata/.data inside GAME.EXE and print a modules.txt line.
usage: findsec.py <file.s from sncc> <name> <src> <profile>
.text is located through its functions (like findfn.py, but requiring one contiguous block);
.rodata/.data by searching the image for the section bytes with relocated words masked."""
import sys, os, re, struct, subprocess, tempfile, importlib.util
HERE=os.path.dirname(os.path.abspath(__file__)); BIN=os.path.join(HERE,"mipsel-elf-binutils","bin")
GAME=os.environ.get("GAME_EXE",os.path.join(HERE,"..","original","partition","GAME.EXE")); LOAD=0x800c4000; CODE_END=0x8016dc4c
def relmask(t):
    if t in ("R_MIPS_HI16","R_MIPS_LO16","R_MIPS_GPREL16","R_MIPS_LITERAL"): return 0xffff0000
    if t=="R_MIPS_26": return 0xfc000000
    if t=="R_MIPS_32": return 0
    return 0xffffffff
def sec(obj,name):
    hdr=subprocess.check_output([os.path.join(BIN,"mipsel-elf-objdump"),"-h",obj]).decode()
    m=re.search(r"\s%s\s+([0-9a-f]+)\s+[0-9a-f]+\s+[0-9a-f]+\s+([0-9a-f]+)"%re.escape(name),hdr)
    if not m: return b"",0
    size=int(m.group(1),16); off=int(m.group(2),16); return open(obj,"rb").read()[off:off+size], off
def relocs(obj,name):
    out=subprocess.run([os.path.join(BIN,"mipsel-elf-objdump"),"-r","-j",name,obj],capture_output=True,text=True).stdout
    r={}
    for line in out.splitlines():
        p=line.split()
        if len(p)>=3 and re.match(r"^[0-9a-f]+$",p[0]) and p[1].startswith("R_"): r[int(p[0],16)]=p[1]
    return r
def search(img,data,rel,wordrel):
    """find unique offset where img matches data except at relocated words"""
    n=len(data)
    if n==0: return []
    mask=bytearray(b"\xff"*n)
    for o,t in rel.items():
        if wordrel: 
            for k in range(4): mask[o+k]=0
        else:
            mk=relmask(t)
            for k in range(4): mask[o+k]=(mk>>(8*k))&0xff
    # anchor: longest run of unmasked bytes
    best=(0,0); run=0
    for i in range(n):
        run=run+1 if mask[i]==0xff else 0
        if run>best[1]: best=(i-run+1,run)
    a0,al=best; al=min(al,32); anchor=data[a0:a0+al]
    hits=[]; p=img.find(anchor)
    while p>=0:
        s=p-a0
        if s>=0 and s+n<=len(img) and all((img[s+i]&mask[i])==(data[i]&mask[i]) for i in range(n)): hits.append(s)
        p=img.find(anchor,p+1)
    return hits
def main():
    src,name,source,prof=sys.argv[1:5]
    spec=importlib.util.spec_from_file_location("sn2gas",os.path.join(HERE,"sn2gas.py")); m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    conv=tempfile.mktemp(suffix=".s"); open(conv,"w").write(m.convert(open(src).readlines())); obj=tempfile.mktemp(suffix=".o")
    subprocess.check_call([os.path.join(BIN,"mipsel-elf-as"),"-EL","-mips4","-mabi=32","-G0","-o",obj,conv])
    img=open(GAME,"rb").read()[8:]
    out={}
    for s,wr in ((".text",False),(".rodata",False),(".data",True)):
        d,off=sec(obj,s)
        if not d: out[s]=None; continue
        hits=search(img,d,relocs(obj,s),wr)
        out[s]=(hits,len(d))
        print("%-8s size %5d  hits: %s"%(s,len(d),", ".join("0x%08x"%(LOAD+h) for h in hits[:6]) or "NONE"))
    def rng(s):
        v=out.get(s)
        if not v or len(v[0])!=1: return "-          -          "
        return "0x%08x 0x%08x"%(LOAD+v[0][0],LOAD+v[0][0]+v[1])
    bss,_=sec(obj,".bss"); print(".bss     size %5d (COMMON not counted)"%len(bss))
    print("%-8s %-24s %-6s %s  %s  %s"%(name,source,prof,rng(".text"),rng(".rodata"),rng(".data")))
main()
