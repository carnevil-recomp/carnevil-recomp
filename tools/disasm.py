#!/usr/bin/env python3
"""Split GAME.EXE into per-function GNU as sources with symbolized data references.

- Code: one file per function (boundaries = Binary Ninja function starts + jal/j targets),
  real MIPS IV instructions, labels for every branch/jump target (global when the branch
  crosses a function boundary), Binja names where known.
- Data references: every lui is paired with the lo16 instructions that use it (addiu / loads /
  stores, also through `addu base,base,index`), and emitted as %hi(sym)/%lo(sym+delta) so the
  linker resolves the address. Symbols are placed in the data dump at byte granularity, in
  .text as labels, or defined absolutely (BSS, odd addresses) in config/symbols_abs.ld.
- Fallbacks: config/word_fallback.txt (emit .word) and config/nosym.txt (keep numeric hi/lo).

usage: disasm.py <GAME.EXE> <outdir> [--functions F] [--symbols S] [--word-fallback W] [--nosym N]
"""
import sys, os, struct, re, argparse
from capstone import *
LOAD=0x800c4000; CODE_END=0x8016dc4c; BSS_END=0x80800000
GPR=['$zero','$at','$v0','$v1','$a0','$a1','$a2','$a3','$t0','$t1','$t2','$t3','$t4','$t5','$t6','$t7',
     '$s0','$s1','$s2','$s3','$s4','$s5','$s6','$s7','$t8','$t9','$k0','$k1','$gp','$sp','$fp','$ra']
LO_OPS={0x09:'addiu',0x20:'lb',0x21:'lh',0x22:'lwl',0x23:'lw',0x24:'lbu',0x25:'lhu',0x26:'lwr',0x27:'lwu',
        0x28:'sb',0x29:'sh',0x2a:'swl',0x2b:'sw',0x2e:'swr',0x31:'lwc1',0x35:'ldc1',0x37:'ld',0x39:'swc1',0x3d:'sdc1',0x3f:'sd'}
FPU_OPS={0x31,0x35,0x39,0x3d}
MIPS4=set('''add addi addiu addu and andi beq beql bgez bgezal bgezall bgezl bgtz bgtzl blez blezl bltz bltzal bltzall bltzl bne bnel break
cache dadd daddi daddiu daddu ddiv ddivu div divu dmult dmultu dsll dsll32 dsllv dsra dsra32 dsrav dsrl dsrl32 dsrlv dsub dsubu
j jal jalr jr lb lbu ld ldl ldr lh lhu ll lld lui lw lwl lwr lwu mfhi mflo mthi mtlo mult multu nor or ori sb sc scd sd sdl sdr sh
sll sllv slt slti sltiu sltu sra srav srl srlv sub subu sw swl swr sync syscall teq teqi tge tgei tgeiu tgeu tlt tlti tltiu tltu tne tnei xor xori
movn movz movf movt pref lwc1 ldc1 swc1 sdc1 mfc1 mtc1 dmfc1 dmtc1 cfc1 ctc1 bc1f bc1t bc1fl bc1tl
abs.s abs.d add.s add.d sub.s sub.d mul.s mul.d div.s div.d sqrt.s sqrt.d mov.s mov.d neg.s neg.d
cvt.s.d cvt.s.w cvt.s.l cvt.d.s cvt.d.w cvt.d.l cvt.w.s cvt.w.d cvt.l.s cvt.l.d round.w.s round.w.d trunc.w.s trunc.w.d ceil.w.s ceil.w.d floor.w.s floor.w.d
round.l.s round.l.d trunc.l.s trunc.l.d ceil.l.s ceil.l.d floor.l.s floor.l.d
c.f.s c.un.s c.eq.s c.ueq.s c.olt.s c.ult.s c.ole.s c.ule.s c.sf.s c.ngle.s c.seq.s c.ngl.s c.lt.s c.nge.s c.le.s c.ngt.s
c.f.d c.un.d c.eq.d c.ueq.d c.olt.d c.ult.d c.ole.d c.ule.d c.sf.d c.ngle.d c.seq.d c.ngl.d c.lt.d c.nge.d c.le.d c.ngt.d
madd.s madd.d msub.s msub.d nmadd.s nmadd.d nmsub.s nmsub.d recip.s recip.d rsqrt.s rsqrt.d movf.s movf.d movt.s movt.d movn.s movn.d movz.s movz.d
lwxc1 ldxc1 swxc1 sdxc1 mfc0 mtc0 tlbr tlbwi tlbwr tlbp eret nop move b bal beqz bnez bgezal negu neg not li'''.split())

class Raw:
    id=0; mnemonic='.word'; op_str=''
    def __init__(self,a): self.address=a

def sext16(v): return v-0x10000 if v&0x8000 else v
def san(n): return re.sub(r'[^A-Za-z0-9_]','_',n)

def gpr_written(w):
    """GPR index written by instruction word w, or None (approximate; enough for lui pairing)."""
    op=w>>26; rs=(w>>21)&31; rt=(w>>16)&31; rd=(w>>11)&31; f=w&0x3f
    if op==0:
        if f in (0x08,0x0c,0x0d,0x0f,0x11,0x13,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x30,0x31,0x32,0x33,0x34,0x36): return None  # jr syscall break sync mthi mtlo mult.. t*
        return rd
    if op==1: return 31 if (rt in (0x10,0x11,0x12,0x13)) else None   # bltzal/bgezal
    if op==3: return 31
    if op in (0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x18,0x19): return rt
    if op in (0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x30,0x34,0x1a,0x1b,0x37,0x38,0x3c): return rt  # loads, ll, lld, ldl/ldr, ld, sc, scd
    if op==0x11 and rs in (0,1,2): return rt     # mfc1 dmfc1 cfc1
    if op==0x10 and rs in (0,1): return rt       # mfc0 dmfc0
    if op==0x1c: return rd                       # special2 (mul etc.)
    return None

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('exe'); ap.add_argument('outdir')
    ap.add_argument('--functions'); ap.add_argument('--symbols'); ap.add_argument('--word-fallback'); ap.add_argument('--nosym')
    ap.add_argument('--names'); ap.add_argument('--modules')
    ap.add_argument('--code-end',type=lambda x:int(x,0),default=CODE_END)
    a=ap.parse_args()
    d=open(a.exe,'rb').read(); img=d[8:]; END=LOAD+len(img); code_end=a.code_end
    def rdset(path):
        try: return {int(x,16) for x in open(path).read().split()}
        except (FileNotFoundError,TypeError): return set()
    fallback=rdset(a.word_fallback); nosym=rdset(a.nosym); pad_from_modules=set()
    # ---- names ----
    fnames={}; dnames={}; used=set(); bstarts=set(); override={}
    if a.names:
        for l in open(a.names):
            if l.startswith('0x'): x,n=l.split()[:2]; override[int(x,16)]=n; used.add(n)
    if a.functions:
        for l in open(a.functions):
            if l.startswith('0x'):
                s,e,n=l.split()[:3]; s=int(s,16)
                if LOAD<=s<code_end:
                    bstarts.add(s)
                    if s in override: fnames[s]=override[s]
                    elif not n.startswith('sub_'):
                        n=san(n); n=n if n not in used else "%s_%08x"%(n,s); used.add(n); fnames[s]=n
    for x,n in override.items():
        if LOAD<=x<code_end: fnames[x]=n
    modules=[]
    if a.modules:
        for l in open(a.modules):
            if l.strip() and not l.startswith('#'):
                f=l.split(); rng=lambda i:(None if f[i]=='-' else (int(f[i],16),int(f[i+1],16)))
                modules.append(dict(name=f[0],src=f[1],profile=f[2],text=rng(3),rodata=rng(5),data=rng(7)))
    if a.symbols:
        for l in open(a.symbols):
            p=l.rstrip('\n').split('\t')
            if len(p)>3 and p[1]=='data' and not p[2].startswith('jump_table'):
                s=int(p[0],16); n=san(p[2])
                if code_end<=s<BSS_END and n not in used and s not in override: used.add(n); dnames[s]=n
    for x,n in override.items():
        if x>=code_end: dnames[x]=n
    # ---- decode ----
    md=Cs(CS_ARCH_MIPS, CS_MODE_MIPS32|CS_MODE_LITTLE_ENDIAN)
    words=struct.unpack('<%dI'%((code_end-LOAD)//4), img[:code_end-LOAD])
    ins=[]
    for k,A in enumerate(range(LOAD, code_end, 4)):
        got=list(md.disasm(img[A-LOAD:A-LOAD+4], A)); i=got[0] if got else Raw(A)
        if i.id and i.mnemonic not in MIPS4: i=Raw(A)
        ins.append(i)
    # ---- function starts / branch targets ----
    starts=set(fnames)|bstarts; btargets=set()
    for i in ins:
        if i.id==0 or i.address in fallback: continue
        if i.mnemonic=='jal':
            t=int(i.op_str,16)
            if LOAD<=t<code_end: starts.add(t)
        elif i.mnemonic=='j':
            t=int(i.op_str,16)
            if LOAD<=t<code_end: btargets.add(t)
        elif i.mnemonic.startswith('b') and i.mnemonic!='break':
            m=re.search(r'0x[0-9a-f]+$',i.op_str)
            if m:
                t=int(m.group(0),16)
                if LOAD<=t<code_end: btargets.add(t)
    starts.add(LOAD); starts=sorted(starts)
    # words between a C module's text end and the next region/module start are linker padding
    # (garbage bytes): never decode them as instructions or branch targets
    for m in modules:
        r=m['text']
        if not r: continue
        nxt=min([x for x in starts if x>r[1]]+[code_end])
        if nxt-r[1]<=16:
            for A in range(r[1],nxt,4): fallback.add(A)
    btargets=set(t for t in btargets if not any(i.address in fallback and i.id and re.search(r'0x%08x$'%t,i.op_str) for i in [ins[(A-LOAD)//4] for A in fallback]))
    region_of={}   # address -> region index
    bounds=[(s, starts[k+1] if k+1<len(starts) else code_end) for k,s in enumerate(starts)]
    for k,(s,e) in enumerate(bounds):
        for A in range(s,e,4): region_of[A]=k
    # ---- hi/lo pairing ----
    sym_hi={}; sym_lo={}; symaddrs=set()   # insn addr -> symbol expr
    for (s,e) in bounds:
        state={}   # reg -> (hi, lui_addr, base_sym_addr or None)
        for A in range(s,e,4):
            w=words[(A-LOAD)//4]; i=ins[(A-LOAD)//4]
            if i.id==0 or A in fallback: state.clear(); continue
            op=w>>26; rs=(w>>21)&31; rt=(w>>16)&31; rd=(w>>11)&31; f=w&0x3f
            if op==0x0f:                       # lui
                state[rt]=[w&0xffff,A,None]; continue
            if op in LO_OPS and rs in state and A not in nosym and state[rs][1] not in nosym:
                hi,luiA,base=state[rs]; addr=((hi<<16)+sext16(w&0xffff))&0xffffffff
                if LOAD<=addr<BSS_END:
                    if base is None:
                        base=addr; state[rs][2]=base; sym_hi[luiA]=base; symaddrs.add(base)
                    sym_lo[A]=(base,addr-base)
            if op==0 and f in (0x20,0x21,0x2c,0x2d) and rd in state and (rs==rd or rt==rd):
                continue                       # addu base,base,index keeps the lui live
            wr=gpr_written(w)
            if wr is not None and wr in state: del state[wr]
    # ---- symbol placement ----
    def tname(A):
        if A in fnames: return fnames[A]
        if A in starts: return "func_%08x"%A
        return None
    glob_labels=set()   # text addresses needing a global (non-.L) label
    for i in ins:
        if i.id==0 or i.address in fallback or not ((i.mnemonic.startswith('b') and i.mnemonic!='break') or i.mnemonic=='j'): continue
        m=re.search(r'0x[0-9a-f]+$',i.op_str)
        if m:
            t=int(m.group(0),16)
            if LOAD<=t<code_end and region_of.get(t)!=region_of.get(i.address): glob_labels.add(t)
    abs_syms={}
    def symname(addr):
        if LOAD<=addr<code_end:
            n=tname(addr)
            if n: return n
            if addr%4==0: glob_labels.add(addr); return "L_%08x"%addr
            abs_syms[addr]="D_%08x"%addr; return "D_%08x"%addr
        n=dnames.get(addr,"D_%08x"%addr)
        if addr>=END: abs_syms[addr]=n
        return n
    def label(addr):
        if not (LOAD<=addr<code_end): return "0x%08x"%addr
        n=tname(addr)
        if n: return n
        return ("L_%08x" if addr in glob_labels else ".L%08x")%addr
    for b in symaddrs: symname(b)
    for x,n in override.items():          # named BSS symbols are always defined, referenced or not
        if x>=END: abs_syms[x]=n
    # ---- emit code ----
    os.makedirs(os.path.join(a.outdir,'functions'),exist_ok=True); os.makedirs(os.path.join(a.outdir,'data'),exist_ok=True)
    files=[]; order=[]   # order: (addr, 'build/...o(section)') for the generated link order
    def module_at(addr,kind):
        for m in modules:
            r=m[kind]
            if r and r[0]<=addr<r[1]: return m
    for m in modules:
        r=m['text']
        if r: assert r[0] in starts, "module %s text range must start on a region boundary"%m['name']
    def emit_words(fn, s, e):
        """a run of raw words (e.g. the padding between a C module's end and the next region)"""
        with open(os.path.join(a.outdir,fn),'w') as f:
            f.write(".set noreorder\n.set noat\n.section .text\n")
            for A in range(s,e,4): f.write("\t.word 0x%08x\n"%words[(A-LOAD)//4])
    for (s,e) in bounds:
        m=module_at(s,'text')
        if m:
            if s==m['text'][0]: order.append((s,"build/%s.o(.text)"%m['src'][:-2]))
            if m['text'][1]<e:      # module ends inside this region: the rest is padding/unowned words
                fn="functions/%08x_pad.s"%m['text'][1]; files.append(fn); order.append((m['text'][1],"build/asm/%s.o(.text)"%fn[:-2]))
                emit_words(fn,m['text'][1],e)
            continue
        fn="functions/%08x_%s.s"%(s,label(s)); files.append(fn); order.append((s,"build/asm/%s.o(.text)"%fn[:-2]))
        with open(os.path.join(a.outdir,fn),'w') as f:
            f.write(".set noreorder\n.set noat\n.section .text\n")
            if s==LOAD: f.write(".globl _start\n_start:\n")
            f.write(".globl %s\n%s:\n"%(label(s),label(s)))
            for A in range(s,e,4):
                w=words[(A-LOAD)//4]; i=ins[(A-LOAD)//4]
                if A!=s and (A in btargets or A in glob_labels):
                    if A in glob_labels: f.write(".globl %s\n"%label(A))
                    f.write("%s:\n"%label(A))
                if A in fallback or i.id==0:
                    f.write("\t.word 0x%08x%s\n"%(w, ("  # "+i.mnemonic+" "+i.op_str) if i.id else "")); continue
                f.write("\t%s\n"%render(i,w,A,label,sym_hi,sym_lo,symname))
    # ---- emit data (byte-granular labels) ----
    labels=sorted(x for x in symaddrs|set(dnames) if code_end<=x<END)
    cuts=sorted({code_end,END}|{x for m in modules for k in ('rodata','data') if m[k] for x in m[k]})
    for cs,ce in zip(cuts,cuts[1:]):
        m=None
        for mm in modules:
            for k in ('rodata','data'):
                if mm[k] and mm[k][0]==cs: m,kind=mm,k
        if m:
            order.append((cs,"build/%s.o(%s)"%(m['src'][:-2],'.rodata .rdata' if kind=='rodata' else '.data'))); continue
        fn="data/data_%08x.s"%cs; files.append(fn); order.append((cs,"build/asm/%s.o(.data)"%fn[:-2]))
        with open(os.path.join(a.outdir,fn),'w') as f:
            al=0
            while al<3 and (cs>>(al+1))<<(al+1)==cs: al+=1     # alignment of the chunk's original address (max 8)
            f.write(".section .data\n.align %d\n"%al)
            off=cs-LOAD; li=0
            while off<ce-LOAD:
                A=LOAD+off
                while li<len(labels) and labels[li]<A: li+=1
                if li<len(labels) and labels[li]==A: f.write(".globl %s\n%s:\n"%(symname(A),symname(A))); li+=1
                nxt=min(labels[li] if li<len(labels) else END, ce)
                n=nxt-A
                if A%4 or n<4:
                    k=min(n, 4-(A%4) if A%4 else n); f.write("\t.byte "+",".join("0x%02x"%b for b in img[off:off+k])+"\n"); off+=k
                else:
                    k=min(n//4,8); f.write("\t.4byte "+",".join("0x%08x"%w for w in struct.unpack('<%dI'%k,img[off:off+4*k]))+"\n"); off+=4*k
    open(os.path.join(a.outdir,"files.txt"),'w').write("\n".join(files)+"\n")
    os.makedirs('build',exist_ok=True)
    with open('build/link_order.ld','w') as f:
        f.write("/* generated by tools/disasm.py: input sections in image order */\n")
        for addr,ent in sorted(order): f.write("    %s   /* 0x%08x */\n"%(ent,addr))
    with open('build/modules.mk','w') as f:
        f.write("# generated by tools/disasm.py\nC_OBJS :=")
        for m in modules: f.write(" build/%s.o"%m['src'][:-2])
        f.write("\n")
        for m in modules: f.write("build/%s.o: PROFILE := %s\n"%(m['src'][:-2],m['profile']))
    with open("config/symbols_abs.ld",'w') as f:
        f.write("/* generated by tools/disasm.py: symbols outside the image (BSS) or at odd addresses */\n")
        for addr in sorted(abs_syms): f.write("%s = 0x%08x;\n"%(abs_syms[addr],addr))
    print("wrote %d function files; %d insns; %d lui symbolized, %d lo16 symbolized, %d data labels, %d abs symbols"%(len(bounds),len(ins),len(sym_hi),len(sym_lo),len(labels),len(abs_syms)))

def render(i,w,A,label,sym_hi,sym_lo,symname):
    m=i.mnemonic; ops=i.op_str; op=w>>26; rs=(w>>21)&31; rt=(w>>16)&31; rd=(w>>11)&31
    if A in sym_hi: return "lui %s, %%hi(%s)"%(GPR[rt],symname(sym_hi[A]))
    if A in sym_lo:
        base,delta=sym_lo[A]; e=symname(base)+("+0x%x"%delta if delta>0 else "-0x%x"%-delta if delta<0 else "")
        if op==0x09: return "addiu %s, %s, %%lo(%s)"%(GPR[rt],GPR[rs],e)
        dst=("$f%d"%rt) if op in FPU_OPS else GPR[rt]
        return "%s %s, %%lo(%s)(%s)"%(LO_OPS[op],dst,e,GPR[rs])
    if m=='nop': return "nop"
    if m=='move':
        opn={0x25:'or',0x21:'addu',0x2d:'daddu'}[w&0x3f]; return "%s %s, %s, $zero"%(opn,GPR[rd],GPR[rs])
    if m in ('j','jal'): return "%s %s"%(m,label(int(ops,16)))
    if m.startswith('b') and m!='break':
        mm=re.search(r'0x[0-9a-f]+$',ops)
        if mm: ops=ops[:mm.start()]+label(int(mm.group(0),16))
    return "%s %s"%(m,ops)
main()
