#!/usr/bin/env python3
"""Rewrite SN cc1 assembly so GNU as reproduces what the SN assembler (asmmips) emitted.

asmmips never moves instructions into branch delay slots; it only appends a nop. GNU as in
reorder mode would swap the previous instruction in. So: emit everything under
.set noreorder and add an explicit nop after every branch/jump that cc1 left in a reorder
region (cc1's own noreorder blocks already contain their delay-slot instruction).
"""
import sys, re
BR=re.compile(r"^\s*(b|bal|beq|bne|beql|bnel|blez|bgtz|bltz|bgez|blezl|bgtzl|bltzl|bgezl|bltzal|bgezal|bc1t|bc1f|bc1tl|bc1fl|j|jal|jr|jalr)\b")
LI=re.compile(r"^\s*li\.([sd])\s+(\$f\d+)\s*,\s*(\S+)")
def convert(lines):
    """Also: asmmips puts every li.s/li.d constant (no dedup, in order of appearance) into a
    literal pool and loads it via lui $at / lwc1|ldc1, so do the same in .rdata."""
    out=["\t.set\tnoreorder"]; reorder=True; lits=[]; n=0
    for line in lines:
        s=line.strip()
        m=re.match(r"\.set\s+(no)?reorder",s)
        if m:
            reorder = (m.group(1) is None); continue   # drop; we are always noreorder for gas
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
    if lits:
        out.append("\t.rdata")
        for kind,lab,val in lits:
            out.append("\t.align\t%d"%(2 if kind=="s" else 3))
            out.append("%s:\t.%s\t%s"%(lab,"float" if kind=="s" else "double",val))
    return "\n".join(out)+"\n"
if __name__=="__main__":
    src=open(sys.argv[1]).readlines(); open(sys.argv[2],"w").write(convert(src))
