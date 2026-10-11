#!/usr/bin/env python3
"""Rewrite SN cc1 assembly so GNU as reproduces what the SN assembler (asmmips) emitted.

asmmips differs from GNU as in these ways, all emulated here:
- it never moves instructions into branch delay slots; it only appends a nop (we assemble
  under .set noreorder and add the nops ourselves);
- `move rd,rs` is encoded as `addu rd,rs,$0` (gas uses `or`);
- `li.s`/`li.d` constants go to a literal pool loaded via lui/lwc1|ldc1 (no dedup);
- `div`/`rem` macros put the divide before the zero check (gas puts it in the delay slot);
- `.lcomm` objects are aligned to min(16, size rounded up to a power of two) inside the
  object's .bss (seen in switch, sprites, ostrings and sound: 800/960-byte arrays at 16).
"""
import sys, re
BREAK=re.compile(r"^\s*break\s+(0x[0-9a-fA-F]+|\d+)\s*$")
LCOMM=re.compile(r"^\s*\.lcomm\s+([^,\s]+)\s*,\s*(\d+)\s*$")
BR=re.compile(r"^\s*(b|bal|beq|bne|beql|bnel|blez|bgtz|bltz|bgez|blezl|bgtzl|bltzl|bgezl|bltzal|bgezal|bc1t|bc1f|bc1tl|bc1fl|j|jal|jr|jalr)\b")
LI=re.compile(r"^\s*li\.([sd])\s+(\$f\d+)\s*,\s*(\S+)")
# load/store with a symbolic address plus a base register: asmmips expands through $at,
# gas through the destination register (loads) -- so expand it ourselves the asmmips way
SYMBASE=re.compile(r"^\s*(lw|sw|lh|lhu|lb|lbu|sh|sb|lwc1|swc1|ldc1|sdc1|lwl|lwr|swl|swr)\s+(\$\w+)\s*,\s*([A-Za-z_$][\w$.]*(?:[+-]\d+)?)\((\$\w+)\)\s*$")   # symbols may contain '.' (sz.2) or '$' ($L94)
MOVE=re.compile(r"^\s*move\s+(\$\w+)\s*,\s*(\$\w+)\s*$")   # asmmips encodes move as addu rd,rs,$0 (gas would use or)
DIVM=re.compile(r"^\s*(div|divu|rem|remu)\s+(\$\w+)\s*,\s*(\$\w+)\s*,\s*(\$\w+)\s*$")
def expand_div(op,d,s,t,n):
    """asmmips expansion of the div/rem macros: the real div comes first, then the zero-divisor
    check with a nop (GNU as would put the div in the bnez delay slot)."""
    unsigned=op.endswith("u"); res="mflo" if op.startswith("div") else "mfhi"
    out=["\t%s\t$0,%s,%s"%("divu" if unsigned else "div",s,t),
         "\tbne\t%s,$0,$Ldz%d"%(t,n), "\tnop", "\tbreak\t7", "$Ldz%d:"%n]
    if not unsigned:
        out+=["\t.set\tnoat","\taddiu\t$at,$0,-1","\tbne\t%s,$at,$Ldo%d"%(t,n),"\tlui\t$at,0x8000",
              "\tbne\t%s,$at,$Ldo%d"%(s,n),"\tnop","\tbreak\t6","$Ldo%d:"%n,"\t.set\tat"]
    out.append("\t%s\t%s"%(res,d))
    return out
def convert(lines):
    """Also: asmmips puts every li.s/li.d constant (no dedup, in order of appearance) into a
    literal pool and loads it via lui $at / lwc1|ldc1, so do the same in .rdata."""
    out=["\t.set\tnoreorder"]; reorder=True; lits=[]; n=0; bss_off=0
    for line in lines:
        s=line.strip()
        m=re.match(r"\.set\s+(no)?reorder",s)
        if m:
            reorder = (m.group(1) is None)             # drop; we are always noreorder for gas
            out.append("#SN_REORDER %d"%reorder); continue
        m=MOVE.match(line)
        if m:
            out.append("\taddu\t%s,%s,$0"%m.groups()); continue
        if s.startswith(".def"): continue          # SDB debug records (.def/.val/.scl/.type/.endef): gas has no COFF debug
        m=SYMBASE.match(line)
        if m:
            op,rt,sym,base=m.groups()
            out+=["\t.set\tnoat","\tlui\t$at,%%hi(%s)"%sym,"\taddu\t$at,$at,%s"%base,"\t%s\t%s,%%lo(%s)($at)"%(op,rt,sym),"\t.set\tat"]
            continue
        m=DIVM.match(line)
        if m and m.group(2) not in ("$0","$zero"):
            out+=expand_div(*m.groups(),n); n+=1; continue
        m=BREAK.match(line)
        if m and int(m.group(1),0)>1023:           # asmmips takes a 20-bit break code; gas only 10 bits
            out.append("\t.word\t0x%08x"%((int(m.group(1),0)<<6)|0xd)); continue
        m=LCOMM.match(line)
        if m:                                      # asmmips aligns each .lcomm to min(16, pow2ceil(size))
            sym,size=m.group(1),int(m.group(2)); a=4   # relative to the object's .bss start; pad by hand so
            while a<size and a<16: a*=2                # the section itself keeps alignment 1 and ld places it
            pad=(-bss_off)%a; bss_off+=pad+size        # exactly where modules.txt says
            out+=["\t.section\t.bss"]+(["\t.space\t%d"%pad] if pad else [])+["%s:"%sym,"\t.space\t%d"%size,"\t.previous"]
            continue
        m=LI.match(line)
        if m:
            kind,reg,val=m.groups(); lab="$LC_sn%d"%n; n+=1
            lits.append((kind,lab,val))
            out.append("\t.set\tnoat")
            out.append("\tlui\t$at,%%hi(%s)"%lab)
            out.append("\t%s\t%s,%%lo(%s)($at)"%("lwc1" if kind=="s" else "ldc1",reg,lab))
            out.append("\t.set\tat")
            continue
        if s.startswith(".rdata") and lits:        # asmmips also flushes pending literals when cc1
            out.append("\t.rdata")                  # switches to .rdata mid-function (switch jump tables)
            for kind,lab,val in lits:
                out.append("\t.align\t%d"%(2 if kind=="s" else 3))
                out.append("%s:\t.%s\t%s"%(lab,"float" if kind=="s" else "double",val))
            lits=[]
        out.append(line.rstrip("\n"))
        if reorder and BR.match(line) and not s.startswith("#"):
            out.append("\tnop")
        if s.startswith(".end") and lits:          # flush this function's literal pool
            out.append("\t.rdata")
            for kind,lab,val in lits:
                out.append("\t.align\t%d"%(2 if kind=="s" else 3))
                out.append("%s:\t.%s\t%s"%(lab,"float" if kind=="s" else "double",val))
            out.append("\t.text"); lits=[]
    return "\n".join(mf_hazards(out))+"\n"
MF=re.compile(r"^\s*(mflo|mfhi)\b"); MULT=re.compile(r"^\s*(mult|multu|dmult|dmultu|div|divu|ddiv|ddivu)\b")
INSN=re.compile(r"^\s*[a-z][a-z0-9.]*(\s|$)")
def mf_hazards(lines):
    """asmmips (reorder mode) keeps two instructions between mflo/mfhi and the next mult/div
    (R4000 hazard); it pads with nops. Seen in LOCKUP.C dump_used_handle."""
    out=[]; reorder=True; since=99
    for l in lines:
        if l.startswith("#SN_REORDER"): reorder=(l.split()[1]=="1"); continue
        if INSN.match(l) and not l.lstrip().startswith("."):
            if reorder and MULT.match(l) and since<2: out+=["\tnop"]*(2-since); since=2
            since = 0 if MF.match(l) else since+1
        out.append(l)
    return out
if __name__=="__main__":
    src=open(sys.argv[1]).readlines(); open(sys.argv[2],"w").write(convert(src))
