#!/usr/bin/env python3
"""Name DIAG.EXE's functions from GAME.EXE's: both link the same GOOSE, Glide and libc objects.

Every named GAME.EXE function (config/functions_v103.txt + config/names.txt, sizes from the
function list) is turned into a masked word signature: the targets of j/jal and the immediates
of lui and of the lo16 users (addiu, loads, stores) are cleared, everything else (registers,
opcodes, branch offsets, constants in ori/andi/slti...) must agree. A DIAG function start
(a jal target) whose words match a whole signature gets that name. Functions whose code is
identical but whose names differ in GAME (aliases) are skipped when ambiguous.

usage: sigmatch.py GAME.EXE DIAG.EXE functions_v103.txt names.txt [--code-end-diag A] > config/diag/functions.txt
"""
import sys, struct, argparse, re
GAME_LOAD=0x800c4000; GAME_CODE_END=0x8016dc4c
LO={0x09,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2e,0x31,0x35,0x37,0x39,0x3d,0x3f}
def mask(w):
    op=w>>26
    if op in (2,3): return w&0xfc000000
    if op==0x0f or op in LO: return w&0xffff0000
    return w
def words(path):
    d=open(path,'rb').read(); load=struct.unpack('<I',d[4:8])[0]; img=d[8:]
    return load, struct.unpack('<%dI'%(len(img)//4), img[:len(img)//4*4])
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('game'); ap.add_argument('diag'); ap.add_argument('functions'); ap.add_argument('names')
    ap.add_argument('--code-end-diag',type=lambda x:int(x,0),default=0x80152170)
    a=ap.parse_args()
    gl,gw=words(a.game); dl,dw=words(a.diag)
    gl=GAME_LOAD   # the header says 0x800c3ff8+8: the image is mapped at the code start
    names={}
    for l in open(a.names):
        if l.startswith('0x'): x,n=l.split()[:2]; names[int(x,16)]=n
    funcs=[]
    for l in open(a.functions):
        if not l.startswith('0x'): continue
        s,e,n=l.split()[:3]; s=int(s,16); e=int(e,16)+1
        if s in names: n=names[s]
        if n.startswith('sub_') or not (GAME_LOAD<=s<GAME_CODE_END): continue
        funcs.append((s,e,n))
    for s,n in names.items():
        if GAME_LOAD<=s<GAME_CODE_END and not any(f[0]==s for f in funcs):
            pass   # no size known
    sigs={}
    for s,e,n in funcs:
        if e-s<16: continue
        sig=tuple(mask(gw[(A-gl)//4]) for A in range(s,e&~3,4))
        sigs.setdefault(sig,set()).add(n)
    # DIAG starts: jal targets
    dend=a.code_end_diag; starts=set([dl])
    for k in range((dend-dl)//4):
        w=dw[k]
        if w>>26==3:
            t=(dl&0xf0000000)|((w&0x3ffffff)<<2)
            if dl<=t<dend: starts.add(t)
    # functions only reached through pointers (process entry points, handlers, menu tables) and
    # leaf functions: whatever follows a jr $ra and its delay slot starts a function, unless a
    # branch of the function before it reaches past it (an early return). A word there that is
    # no instruction is the junk the linker leaves between two objects: the function starts after it.
    from capstone import Cs, CS_ARCH_MIPS, CS_MODE_MIPS32, CS_MODE_LITTLE_ENDIAN
    md=Cs(CS_ARCH_MIPS, CS_MODE_MIPS32|CS_MODE_LITTLE_ENDIAN)
    img=b''.join(struct.pack('<I',x) for x in dw[:(dend-dl)//4])
    def valid(k):
        g=list(md.disasm(img[4*k:4*k+4],dl+4*k))
        return bool(g) and g[0].mnemonic not in ('ldxc1','lwxc1','sdxc1','swxc1','cache','lwc2','swc2','ldc2','sdc2','pref','jalx') and not g[0].mnemonic.startswith(('c2','mtc2','mfc2','cop'))
    def btarget(k):
        w=dw[k]; op=w>>26
        if op in (4,5,6,7,0x14,0x15,0x16,0x17) or (op==1 and ((w>>16)&31) in (0,1,2,3,0x10,0x11,0x12,0x13)) or (op==0x11 and ((w>>21)&31)==8):
            o=w&0xffff; o=o-0x10000 if o&0x8000 else o; return k+1+o
        if op==2:      # a j inside the function (a loop, a goto), not a tail call into a known one
            t=(dl&0xf0000000)|((w&0x3ffffff)<<2)
            return None if t in calls else (t-dl)//4
        return None
    calls=set(starts)
    N=(dend-dl)//4; fstart=0; reach=0; k=0; padding=set()
    callk=set((c-dl)//4 for c in calls); import bisect; cl=sorted(callk)
    frame=lambda k: (dw[k]&0xffff8000)==0x27bd8000
    while k<N:
        if k in callk: reach=k          # a called address starts a function whatever came before
        t=btarget(k)
        # a C function's branches stay inside it: one that would pass a called address is a junk word
        if t is not None and t>reach and not (bisect.bisect_right(cl,k)<len(cl) and cl[bisect.bisect_right(cl,k)]<=t): reach=t
        if dw[k]==0x03e00008:
            j=k+2
            while j<N and dw[j]==0: j+=1
            if j<N and j>reach:
                if j+1<N and (not valid(j) or (btarget(j) is not None and (frame(j+1) or j+1 in callk))): padding.add(dl+4*j); j+=1
                starts.add(dl+4*j); fstart=j; reach=j
            k=j; continue
        k+=1
    # lui/addiu pairs that load a code address (function pointers in code)
    for k in range(N-1):
        w=dw[k]
        if w>>26==0x0f:
            rt=(w>>16)&31; hi=(w&0xffff)<<16
            for j in range(k+1,min(k+6,N)):
                v=dw[j]
                if v>>26==0x09 and (v>>21)&31==rt:
                    lo=v&0xffff; lo=lo-0x10000 if lo&0x8000 else lo; t=(hi+lo)&0xffffffff
                    if dl<=t<dend and t%4==0 and (dw[(t-dl)//4]&0xffff8000)==0x27bd8000: starts.add(t)
                    break
    starts=sorted(starts)
    by_len={}
    for sig,ns in sigs.items(): by_len.setdefault(len(sig),[]).append((sig,ns))
    lens=sorted(by_len,reverse=True)
    dmask=[mask(x) for x in dw[:(dend-dl)//4]]
    out=[]; used=set(); named=set()
    for s in starts:
        k=(s-dl)//4
        for L in lens:
            if k+L>len(dmask): continue
            seg=tuple(dmask[k:k+L])
            hit=[ns for sig,ns in by_len[L] if sig==seg]
            if hit:
                ns=hit[0]
                if len(ns)==1:
                    n=next(iter(ns))
                    if n not in used: out.append((s,s+4*L,n)); used.add(n); named.add(s)
                break
    print("# DIAG.EXE functions whose masked code equals a named GAME.EXE function (tools/diag/sigmatch.py)")
    print("# start end name (end inclusive, as functions_v103.txt); sub_ = not identified")
    allf=dict((s,(e,n)) for s,e,n in out)
    for k,s in enumerate(starts):
        if s not in allf: allf[s]=((starts[k+1] if k+1<len(starts) else dend),"sub_%08x"%s)
    for s in sorted(allf): print("0x%08x 0x%08x %s"%(s,allf[s][0]-1,allf[s][1]))
    with open('config/diag/padding.txt','w') as f:
        f.write("# words the linker left between two objects (file boundaries), found by tools/diag/sigmatch.py\n")
        for x in sorted(padding): f.write("0x%08x 0x%08x\n"%(x,dw[(x-dl)//4]))
    print("# %d of %d DIAG call targets named"%(len(out),len(starts)),file=sys.stderr)
main()
