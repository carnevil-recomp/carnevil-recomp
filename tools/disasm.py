#!/usr/bin/env python3
"""Split GAME.EXE into reassemblable GNU as sources (first-iteration scaffold).

Code is emitted as real MIPS instructions with labels for every branch/jump target; data as
.word. Aliases that gas would encode differently from the original are rewritten from the raw
instruction word (e.g. 'move' -> 'or'/'addu'). Anything the assembler still cannot reproduce
byte-exact is downgraded to '.word' by the verify step (see tools/verify.py / Makefile).

usage: disasm.py <GAME.EXE> <outdir> [--functions config/functions_v103.txt] [--word-fallback offsets.txt]
"""
import sys, os, struct, argparse, re
from capstone import *
LOAD=0x800c4000
CODE_END=0x8016dc4c          # from Binary Ninja sections (v1.0.3): .text ends here, data follows
CHUNK=0x1000000            # one code file for now (branch labels are file-local); split per function once symbolized

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('exe'); ap.add_argument('outdir')
    ap.add_argument('--functions'); ap.add_argument('--word-fallback'); ap.add_argument('--code-end',type=lambda x:int(x,0),default=CODE_END)
    a=ap.parse_args()
    d=open(a.exe,'rb').read(); img=d[8:]; end=LOAD+len(img); code_end=a.code_end
    names={}
    if a.functions:
        for l in open(a.functions):
            if l.startswith('0x'):
                s,e,n=l.split()[:3]; s=int(s,16)
                if s>=LOAD and not n.startswith('sub_'):
                    n=re.sub(r'[^A-Za-z0-9_]','_',n)
                    if n in names.values(): n="%s_%08x"%(n,s)
                    names[s]=n
    fallback=set()
    if a.word_fallback and os.path.exists(a.word_fallback):
        fallback={int(x,16) for x in open(a.word_fallback).read().split()}
    md=Cs(CS_ARCH_MIPS, CS_MODE_MIPS32|CS_MODE_LITTLE_ENDIAN); md.detail=True
    ins=[]
    for A in range(LOAD, code_end, 4):
        got=list(md.disasm(img[A-LOAD:A-LOAD+4], A))
        ins.append(got[0] if got else Raw(A))
    # label targets
    targets=set(); funcs=set(names)
    for i in ins:
        if i.mnemonic in ('j','jal'):
            funcs.add(int(i.op_str,16))
        elif i.mnemonic.startswith('b') and i.mnemonic!='break':
            m=re.search(r'0x[0-9a-f]+$',i.op_str)
            if m: targets.add(int(m.group(0),16))
    def label(addr):
        if not (LOAD<=addr<code_end): return "0x%08x"%addr
        if addr in names: return names[addr]
        if addr in funcs: return "func_%08x"%addr
        return ".L%08x"%addr
    os.makedirs(a.outdir,exist_ok=True)
    files=[]
    # ---- code ----
    for start in range(LOAD, code_end, CHUNK):
        fn="code_%08x.s"%start; files.append(fn)
        with open(os.path.join(a.outdir,fn),'w') as f:
            f.write(".set noreorder\n.set noat\n.section .text\n")
            if start==LOAD: f.write(".globl _start\n_start:\n")
            for i in ins[(start-LOAD)//4:(min(start+CHUNK,code_end)-LOAD)//4]:
                A=i.address; w=struct.unpack('<I',img[A-LOAD:A-LOAD+4])[0]
                if A in funcs:
                    f.write("\n.globl %s\n%s:\n"%(label(A),label(A)) if A in names or A in funcs else "")
                elif A in targets: f.write("%s:\n"%label(A))
                if A in fallback or i.id==0 or i.mnemonic not in MIPS4:
                    f.write("\t.word 0x%08x%s\n"%(w, ("  # "+i.mnemonic+" "+i.op_str) if i.id else "")); continue
                f.write("\t%s\n"%render(i,w,label))
    # ---- data ----
    fn="data.s"; files.append(fn)
    with open(os.path.join(a.outdir,fn),'w') as f:
        f.write(".section .data\n.globl data_start\ndata_start:\n")
        off=code_end-LOAD
        while off<len(img):
            n=min(8,(len(img)-off)//4)
            if n==0:
                f.write("\t.byte "+",".join("0x%02x"%b for b in img[off:])+"\n"); break
            f.write("\t.word "+",".join("0x%08x"%w for w in struct.unpack('<%dI'%n,img[off:off+4*n]))+"\n"); off+=4*n
    open(os.path.join(a.outdir,"files.txt"),'w').write("\n".join(files)+"\n")
    print("wrote",len(files),"files;",len(ins),"insns; code 0x%08x-0x%08x data 0x%08x-0x%08x"%(LOAD,code_end,code_end,end))

MIPS4=set('''add addi addiu addu and andi beq beql bgez bgezal bgezall bgezl bgtz bgtzl blez blezl bltz bltzal bltzall bltzl bne bnel break
cache dadd daddi daddiu daddu ddiv ddivu div divu dmult dmultu dsll dsll32 dsllv dsra dsra32 dsrav dsrl dsrl32 dsrlv dsub dsubu
j jal jalr jr lb lbu ld ldl ldr lh lhu ll lld lui lw lwl lwr lwu mfhi mflo mthi mtlo mult multu nor or ori sb sc scd sd sdl sdr sh
sll sllv slt slti sltiu sltu sra srav srl srlv sub subu sw swl swr sync syscall teq teqi tge tgei tgeiu tgeu tlt tlti tltiu tltu tne tnei xor xori
movn movz movf movt pref
lwc1 ldc1 swc1 sdc1 mfc1 mtc1 dmfc1 dmtc1 cfc1 ctc1 bc1f bc1t bc1fl bc1tl
abs.s abs.d add.s add.d sub.s sub.d mul.s mul.d div.s div.d sqrt.s sqrt.d mov.s mov.d neg.s neg.d
cvt.s.d cvt.s.w cvt.s.l cvt.d.s cvt.d.w cvt.d.l cvt.w.s cvt.w.d cvt.l.s cvt.l.d round.w.s round.w.d trunc.w.s trunc.w.d ceil.w.s ceil.w.d floor.w.s floor.w.d
round.l.s round.l.d trunc.l.s trunc.l.d ceil.l.s ceil.l.d floor.l.s floor.l.d
c.f.s c.un.s c.eq.s c.ueq.s c.olt.s c.ult.s c.ole.s c.ule.s c.sf.s c.ngle.s c.seq.s c.ngl.s c.lt.s c.nge.s c.le.s c.ngt.s
c.f.d c.un.d c.eq.d c.ueq.d c.olt.d c.ult.d c.ole.d c.ule.d c.sf.d c.ngle.d c.seq.d c.ngl.d c.lt.d c.nge.d c.le.d c.ngt.d
madd.s madd.d msub.s msub.d nmadd.s nmadd.d nmsub.s nmsub.d recip.s recip.d rsqrt.s rsqrt.d movf.s movf.d movt.s movt.d movn.s movn.d movz.s movz.d
lwxc1 ldxc1 swxc1 sdxc1 mfc0 mtc0 tlbr tlbwi tlbwr tlbp eret
nop move b bal beqz bnez bgezal negu neg not li'''.split())
class Raw:
    """stand-in for a word capstone cannot decode"""
    id=0; mnemonic='.word'; op_str=''
    def __init__(self,a): self.address=a
def render(i,w,label):
    m=i.mnemonic; ops=i.op_str
    if m=='nop': return "nop"
    if m=='move':            # or vs addu
        rd=(w>>11)&31; rs=(w>>21)&31; op={0x25:'or',0x21:'addu',0x2d:'daddu'}[w&0x3f]
        return "%s $%d, $%d, $zero"%(op,rd,rs)
    if m in ('j','jal'): return "%s %s"%(m,label(int(ops,16)))
    if m.startswith('b') and m!='break':
        mm=re.search(r'0x[0-9a-f]+$',ops)
        if mm: ops=ops[:mm.start()]+label(int(mm.group(0),16))
    return "%s %s"%(m,ops)
main()
