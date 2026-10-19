#!/usr/bin/env python3
"""Iterate disasm -> assemble -> link -> compare until build/game.bin == original image.
Assembler errors and differing words are added to config/word_fallback.txt (emitted as .word).
--diag does the same for DIAG.EXE (config/diag/, asm_diag/, build/diag/)."""
import subprocess, re, os, sys, struct
BIN='tools/mipsel-elf-binutils/bin/' if sys.platform=='darwin' else 'tools/mipsel-elf-binutils-linux/bin/'
ASFLAGS=['-EL','-mips4','-mabi=32','-G0','-no-pad-sections']
if '--diag' in sys.argv:
    ORIG='original/partition/DIAG.EXE'; C='config/diag/'; FB=C+'word_fallback.txt'; NS=C+'nosym.txt'; LOAD=0x80100000
    ASM='asm_diag'; B='build/diag'; LDS=C+'diag.ld'
    DIS=['--functions',C+'functions.txt','--names',C+'names.txt','--modules',C+'modules.txt',
         '--load','0x80100000','--code-end','0x80152170','--build-dir',B,'--abs-out',C+'symbols_abs.ld']
else:
    ORIG='original/partition/GAME.EXE'; FB='config/word_fallback.txt'; NS='config/nosym.txt'; LOAD=0x800c4000
    ASM='asm'; B='build'; LDS='config/game.ld'
    DIS=['--functions','config/functions_v103.txt','--symbols','config/binja_symbols.txt','--names','config/names.txt','--modules','config/modules.txt']
def rdset(p):
    try: return {int(x,16) for x in open(p).read().split()}
    except FileNotFoundError: return set()
def fallback(): return rdset(FB)
def save(fb,p=FB): open(p,'w').write("\n".join("0x%08x"%a for a in sorted(fb))+"\n")
def regen():
    subprocess.run(['rm','-rf',ASM]); subprocess.check_call(['python3','tools/build/disasm.py',ORIG,ASM]+DIS+['--word-fallback',FB,'--nosym',NS])
def line_addr(path,lineno):
    """address of the instruction on a given line: 4 bytes per instruction/.word line"""
    base=int(re.search(r'/([0-9a-f]{8})_',path).group(1),16); a=base
    for k,l in enumerate(open(path),1):
        if k==lineno: return a
        t=l.split('#')[0].strip()
        if t and not t.endswith(':') and not t.startswith(('.set','.section','.globl','#')): a+=4 if not t.startswith('.word') else 4*(t.count(',')+1)
    return None
def assemble(fb):
    os.makedirs('build/'+ASM,exist_ok=True); bad=set(); objs=[]
    for fn in open(ASM+'/files.txt').read().split():
        src=ASM+'/'+fn; obj='build/'+ASM+'/'+fn[:-2]+'.o'; objs.append(obj); os.makedirs(os.path.dirname(obj),exist_ok=True)
        r=subprocess.run([BIN+'mipsel-elf-as']+ASFLAGS+['-o',obj,src],capture_output=True,text=True)
        if r.returncode:
            for m in re.finditer(r'^(\S+\.s):(\d+): Error',r.stderr,re.M):
                a=line_addr(m.group(1),int(m.group(2)))
                if a is not None: bad.add(a)
            if not bad: print(r.stderr[:800]); sys.exit(1)
    return bad,objs
for it in range(12):
    fb=fallback(); regen(); bad,objs=assemble(fb)
    if bad: print("iter %d: %d assembler errors -> fallback"%(it,len(bad))); save(fb|bad); continue
    r=subprocess.run([BIN+'mipsel-elf-ld','-EL','-T',LDS,'-o',B+'/image.elf']+objs,capture_output=True,text=True)
    if r.returncode: print(r.stderr[:1500]); sys.exit(1)
    subprocess.check_call([BIN+'mipsel-elf-objcopy','-O','binary',B+'/image.elf',B+'/image.bin'])
    orig=open(ORIG,'rb').read()[8:]; built=open(B+'/image.bin','rb').read()
    diffs=[LOAD+o for o in range(0,min(len(orig),len(built))-3,4) if orig[o:o+4]!=built[o:o+4]]
    print("iter %d: size orig %d built %d, differing words %d"%(it,len(orig),len(built),len(diffs)))
    if not diffs and len(orig)==len(built): print("BYTE-EXACT. fallback words: %d"%len(fb)); break
    if not diffs: sys.exit("size mismatch with no word diffs; check linker script")
    # a differing word that is a symbolized hi/lo -> keep it numeric (nosym); anything else -> .word
    symd=set()
    for fn in open(ASM+'/files.txt').read().split():
        if not fn.startswith('functions/'): continue
        a=int(re.search(r'/([0-9a-f]{8})_',fn).group(1),16)
        for l in open(ASM+'/'+fn):
            t=l.split('#')[0].strip()
            if t and not t.endswith(':') and not t.startswith(('.set','.section','.globl')):
                if '%hi(' in t or '%lo(' in t: symd.add(a)
                a+=4 if not t.startswith('.word') else 4*(t.count(',')+1)
    ns=set(d for d in diffs if d in symd); wd=set(diffs)-ns
    print("   -> %d to nosym, %d to .word"%(len(ns),len(wd)))
    save(rdset(NS)|ns,NS); save(fb|wd)
