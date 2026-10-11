#!/usr/bin/env python3
"""Rewrite SN cc1 assembly so GNU as reproduces what the SN assembler (asmmips) emitted.

asmmips differs from GNU as in these ways, all emulated here:
- it never moves instructions into branch delay slots; it only appends a nop (we assemble
  under .set noreorder and add the nops ourselves);
- `move rd,rs` is encoded as `addu rd,rs,$0` (gas uses `or`);
- `li.s`/`li.d` constants go to a literal pool loaded via lui/lwc1|ldc1 (no dedup);
- `div`/`rem` macros put the divide before the zero check (gas puts it in the delay slot).
"""
import sys, re
BR=re.compile(r"^\s*(b|bal|beq|bne|beql|bnel|blez|bgtz|bltz|bgez|blezl|bgtzl|bltzl|bgezl|bltzal|bgezal|bc1t|bc1f|bc1tl|bc1fl|j|jal|jr|jalr)\b")
LI=re.compile(r"^\s*li\.([sd])\s+(\$f\d+)\s*,\s*(\S+)")
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
    out=["\t.set\tnoreorder"]; reorder=True; lits=[]; n=0
    for line in lines:
        s=line.strip()
        m=re.match(r"\.set\s+(no)?reorder",s)
        if m:
            reorder = (m.group(1) is None); continue   # drop; we are always noreorder for gas
        m=MOVE.match(line)
        if m:
            out.append("\taddu\t%s,%s,$0"%m.groups()); continue
        m=DIVM.match(line)
        if m and m.group(2) not in ("$0","$zero"):
            out+=expand_div(*m.groups(),n); n+=1; continue
        m=LI.match(line)
        if m:
            kind,reg,val=m.groups(); lab="$LC_sn%d"%n; n+=1
            lits.append((kind,lab,val))
            out.append("\t.set\tnoat")
            out.append("\tlui\t$at,%%hi(%s)"%lab)
            out.append("\t%s\t%s,%%lo(%s)($at)"%("lwc1" if kind=="s" else "ldc1",reg,lab))
            out.append("\t.set\tat")
            continue
        out.append(line.rstrip("\n"))
        if reorder and BR.match(line) and not s.startswith("#"):
            out.append("\tnop")
        if s.startswith(".end") and lits:          # flush this function's literal pool
            out.append("\t.rdata")
            for kind,lab,val in lits:
                out.append("\t.align\t%d"%(2 if kind=="s" else 3))
                out.append("%s:\t.%s\t%s"%(lab,"float" if kind=="s" else "double",val))
            out.append("\t.text"); lits=[]
    return "\n".join(out)+"\n"
if __name__=="__main__":
    src=open(sys.argv[1]).readlines(); open(sys.argv[2],"w").write(convert(src))
