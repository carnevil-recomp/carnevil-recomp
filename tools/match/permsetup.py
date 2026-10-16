#!/usr/bin/env python3
"""Set up a decomp-permuter run (https://github.com/simonlindholm/decomp-permuter) for one function.
usage: permsetup.py <file.c> <profile> <function>      -> build/perm/<function>/
then:  $PERMUTER/permuter.py -j8 build/perm/<function>   (PERMUTER: a checkout, run with its deps)
base.c is the preprocessed file with the other functions' bodies removed, target.o the original
function assembled from asm/functions/, compile.sh the SN cc1 + sn2gas + gas pipeline with the
profile's flags (tools/match/trymatch.sh). The permuter only proposes source: a candidate is
kept when it reads like code someone wrote, and is then checked with trymatch.sh and make."""
import sys, os, re, subprocess, glob
HERE=os.path.dirname(os.path.abspath(__file__)); ROOT=os.path.abspath(os.path.join(HERE,"..",".."))
BIN=os.path.join(ROOT,"tools","mipsel-elf-binutils","bin")
src,prof,fn=sys.argv[1:4]
flags=subprocess.check_output(["sh","-c",
    "sed -n 's/^  %s) F=\"\\([^\"]*\\)\".*/\\1/p' %s/tools/match/trymatch.sh"%(prof,ROOT)]).decode().strip()
if not flags: sys.exit("unknown profile "+prof)
hazard = "--fpu-hazard" if prof.startswith("game") else ""
asmf=glob.glob(os.path.join(ROOT,"asm","functions","*_%s.s"%fn))
if len(asmf)!=1: sys.exit("no single asm/functions/*_%s.s (is it named in config/names.txt?)"%fn)
d=os.path.join(ROOT,"build","perm",fn); os.makedirs(d,exist_ok=True)
inc=" ".join("-I"+p.rstrip("/") for p in sorted(glob.glob(os.path.join(ROOT,"src","game","*/"))))
pre=subprocess.check_output("clang -E -P -Wno-nonportable-include-path -nostdinc -I%s/original/sdk/include -I%s/include %s "
    "-D__DJGPP__ -DSEATTLE -DRELEASE -x c %s"%(ROOT,ROOT,inc,src),shell=True,cwd=ROOT).decode()
sys.path.insert(0,os.environ.get("PERMUTER",os.path.join(ROOT,"..","decomp-permuter")))
from strip_other_fns import strip_other_fns
open(os.path.join(d,"base.c"),"w").write(strip_other_fns(pre,fn))
# the target: the original function on its own (branches out of it and data become relocations)
s=open(asmf[0]).read()
open(os.path.join(d,"target.s"),"w").write(s)
subprocess.check_call([os.path.join(BIN,"mipsel-elf-as"),"-EL","-mips4","-mabi=32","-G0","-o",os.path.join(d,"target.o"),os.path.join(d,"target.s")])
open(os.path.join(d,"compile.sh"),"w").write("""#!/bin/sh
# compile.sh in.c -o out.o (the permuter's interface)
set -e
R=%s
T=$(mktemp -d "${TMPDIR:-/tmp}/perm.XXXXXX")
cp "$1" $T/in.i
CC1FLAGS="%s" $R/tools/build/sncc.sh $T/in.i $T/out.sn.s 2>/dev/null
python3 $R/tools/build/sn2gas.py $T/out.sn.s $T/out.s %s
$R/tools/mipsel-elf-binutils/bin/mipsel-elf-as -EL -mips4 -mabi=32 -G0 -o "$3" $T/out.s 2>/dev/null
rm -rf $T
"""%(ROOT,flags,hazard))
os.chmod(os.path.join(d,"compile.sh"),0o755)
open(os.path.join(d,"settings.toml"),"w").write('func_name = "%s"\ncompiler_type = "gcc"\nobjdump_command = "%s/mipsel-elf-objdump -drz -m mips:4300"\n'%(fn,BIN))
print(d)
