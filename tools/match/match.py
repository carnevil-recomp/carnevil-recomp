#!/usr/bin/env python3
"""Assemble a .s produced by the SN cc1 and compare named functions against GAME.EXE.

Usage: match.py <file.s> <func>=<game_vaddr> [<func>=<vaddr> ...]
Reports per-function instruction match percentage. Immediates that carry relocations
(lui hi16 / lo16 / jal targets) are masked, since the link layout differs. Switch jump tables
are compared separately: each table the function indexes (lui/lw pair against .rodata) must send
every case to the same place in the function as the original's, or the cases are reported (the
masked instruction compare cannot see a case body attached to the wrong case values).
"""
import sys, os, subprocess, struct, re, tempfile
HERE=os.path.dirname(os.path.abspath(__file__))
BIN=os.path.join(HERE,"..","mipsel-elf-binutils","bin")
AS=os.path.join(BIN,"mipsel-elf-as"); OBJDUMP=os.path.join(BIN,"mipsel-elf-objdump"); NM=os.path.join(BIN,"mipsel-elf-nm")
GAME=os.environ.get("GAME_EXE",os.path.join(HERE,"..","..","original","partition","GAME.EXE"))
LOAD=struct.unpack("<I",open(GAME,"rb").read(8)[4:])[0]   # the header's load address (GAME_EXE=...DIAG.EXE for the diagnostics)
from capstone import *
md=Cs(CS_ARCH_MIPS, CS_MODE_MIPS32|CS_MODE_LITTLE_ENDIAN)

def assemble(src):
    import importlib.util
    spec=importlib.util.spec_from_file_location("sn2gas",os.path.join(HERE,"..","build","sn2gas.py")); m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    conv=tempfile.mktemp(suffix=".s"); open(conv,"w").write(m.convert(open(src).readlines()))
    obj=tempfile.mktemp(suffix=".o")
    subprocess.check_call([AS,"-EL","-mips4","-mabi=32","-G0","-o",obj,conv])
    return obj

def func_bytes(obj,name):
    syms=subprocess.check_output([NM,"-S","--defined-only",obj]).decode()
    for line in syms.splitlines():
        p=line.split()
        if len(p)>=4 and p[3]==name:
            addr=int(p[0],16); size=int(p[1],16)
            text=subprocess.check_output([OBJDUMP,"-h",obj]).decode()
            m=re.search(r"\.text\s+([0-9a-f]+)\s+[0-9a-f]+\s+[0-9a-f]+\s+([0-9a-f]+)",text)
            off=int(m.group(2),16)
            d=open(obj,"rb").read()
            return d[off+addr:off+addr+size], addr
    raise SystemExit("symbol %s not found"%name)

def relocs(obj):
    out=subprocess.check_output([OBJDUMP,"-r","-j",".text",obj]).decode()
    r={}
    for line in out.splitlines():
        p=line.split()
        if len(p)>=3 and re.match(r"^[0-9a-f]+$",p[0]) and p[1].startswith("R_"): r[int(p[0],16)]=p[1]
    return r
def section_relocs(obj, sec):
    """{offset: (type, symbol)} for one section (REL: addends are in the section bytes)"""
    out=subprocess.run([OBJDUMP,"-r","-j",sec,obj],capture_output=True,text=True).stdout
    r={}
    for line in out.splitlines():
        p=line.split()
        if len(p)>=3 and re.match(r"^[0-9a-f]+$",p[0]) and p[1].startswith("R_"): r[int(p[0],16)]=(p[1],p[2])
    return r
def section_bytes(obj, sec):
    hdr=subprocess.check_output([OBJDUMP,"-h",obj]).decode()
    m=re.search(r"\s%s\s+([0-9a-f]+)\s+[0-9a-f]+\s+[0-9a-f]+\s+([0-9a-f]+)"%re.escape(sec),hdr)
    if not m: return b""
    size=int(m.group(1),16); off=int(m.group(2),16)
    return open(obj,"rb").read()[off:off+size]
def jump_tables(obj, ours, oaddr, theirs, addr, game):
    """compare each switch table's targets (relative to the function) with the original's"""
    trel=section_relocs(obj,".text"); rrel=section_relocs(obj,".rodata"); rod=section_bytes(obj,".rodata")
    w=lambda b,i: struct.unpack("<I",b[i:i+4])[0]
    sx=lambda v: v-0x10000 if v&0x8000 else v
    out=[]; n=len(ours)//4; found=[]
    for k in range(n):
        t=trel.get(oaddr+4*k)
        if not t or t[0]!="R_MIPS_HI16" or t[1]!=".rodata": continue
        for j in range(k+1,min(k+4,n)):               # its lo16: the table load
            u=trel.get(oaddr+4*j)
            if u and u[0]=="R_MIPS_LO16" and u[1]==".rodata": break
        else: continue
        if not any((w(ours,4*m)&0xfc1fffff)==0x00000008 for m in range(j+1,min(j+4,n))): continue   # then jr
        found.append((k,((w(ours,4*k)&0xffff)<<16)+sx(w(ours,4*j)&0xffff),((w(theirs,4*k)&0xffff)<<16)+sx(w(theirs,4*j)&0xffff)))
    starts={f[1] for f in found}
    for k,toff,tabs in found:
        e=0; bad=[]
        while toff+4*e+4<=len(rod) and rrel.get(toff+4*e,("",""))[0]=="R_MIPS_32" and rrel[toff+4*e][1]==".text" and (e==0 or toff+4*e not in starts):
            mine=w(rod,toff+4*e)-oaddr
            if not (LOAD<=tabs+4*e<LOAD+len(game)-4): bad.append(e); e+=1; continue
            orig=w(game,tabs+4*e-LOAD)-addr
            if mine!=orig: bad.append((e,mine,orig))
            e+=1
        if e: out.append((k,e,bad,tabs))
    return out
def relmask(rtype):
    if rtype in ("R_MIPS_HI16","R_MIPS_LO16","R_MIPS_GPREL16","R_MIPS_LITERAL"): return 0xffff0000
    if rtype=="R_MIPS_26": return 0xfc000000
    return 0xffffffff

def norm(ins, masked):
    # mask relocated immediates
    if masked:
        if ins.mnemonic=="lui": return "lui %s,HI"%ins.op_str.split(",")[0]
        if ins.mnemonic in ("jal","j"): return ins.mnemonic+" T"
        ops=ins.op_str
        if re.search(r"(^|,\s*)\(\$\w+\)$",ops): ops=re.sub(r"\(\$\w+\)$",lambda m:"0"+m.group(0),ops)   # "($at)" == "0($at)"
        return ins.mnemonic+" "+re.sub(r"(-?0x[0-9a-f]+|-?\d+)(\(\$\w+\))?$",lambda m:"LO"+(m.group(2) or ""),ops)
    # mask branch targets (absolute addresses differ)
    if ins.mnemonic.startswith("b") and ins.mnemonic not in ("break",):
        return ins.mnemonic+" "+re.sub(r"0x[0-9a-f]+$","BR",ins.op_str)
    if ins.mnemonic in ("j","jal"): return ins.mnemonic+" T"
    return ins.mnemonic+" "+ins.op_str

def main():
    src=sys.argv[1]; obj=assemble(src); rel=relocs(obj)
    game=open(GAME,"rb").read()[8:]
    total_ok=total=0
    for spec in sys.argv[2:]:
        name,addr=spec.split("="); addr=int(addr,16)
        ours,oaddr=func_bytes(obj,name)
        theirs=game[addr-LOAD:addr-LOAD+len(ours)]
        n=len(ours)//4; ok=0; diffs=[]
        dis=lambda w,at: next(iter(md.disasm(struct.pack("<I",w),at)),None)
        for k in range(n):
            wo=struct.unpack("<I",ours[4*k:4*k+4])[0]; wt=struct.unpack("<I",theirs[4*k:4*k+4])[0]
            mask=relmask(rel.get(oaddr+4*k,"")); 
            if (wo&mask)==(wt&mask): ok+=1
            else:
                x=dis(wo,addr+4*k); y=dis(wt,addr+4*k)
                diffs.append("  %08x  ours: %-28s theirs: %s"%(addr+4*k, (x.mnemonic+" "+x.op_str) if x else "%08x"%wo, (y.mnemonic+" "+y.op_str) if y else "%08x"%wt))
        tabs=jump_tables(obj,ours,oaddr,theirs,addr,game)
        tbad=[t for t in tabs if t[2]]
        if tbad: ok=min(ok,n-1)                          # a misrouted switch is not a match
        total+=n; total_ok+=ok
        print("%-24s %4d insns  %3d%% match (words, relocs masked)%s"%(name,n,100*ok//max(n,1),
            "  JUMP TABLE MISMATCH" if tbad else (", %d jump table(s) ok"%len(tabs) if tabs else "")))
        for k,e,bad,ta in tbad:
            print("  switch at %08x (table 0x%08x, %d entries): %d entries go elsewhere"%(addr+4*k,ta,e,len(bad)))
            for b in bad[:8]:
                if isinstance(b,tuple): print("    entry %d: ours -> +0x%x, theirs -> +0x%x"%b)
        lim=int(os.environ.get("MATCH_MAXDIFF","12"))
        for d in diffs[:lim]: print(d)
        if len(diffs)>lim: print("  ... %d more"%(len(diffs)-lim))
    print("TOTAL %d/%d insns (%d%%)"%(total_ok,total,100*total_ok//max(total,1)))
if __name__=="__main__": main()
