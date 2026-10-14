#!/usr/bin/env python3
"""Generate the library sources from the SDK checkout: for each line of config/sdk_sources.txt
(<our path> <path under original/sdk>) copy the SDK file with every CR byte stripped and apply
patches/<our path minus src/>.patch when it exists. Used by make; the generated files are not
committed (the SDK is the source of truth, the patches are the changes CarnEvil's revisions need).
usage: sdksrc.py [--check]   (--check: report files that differ from what would be generated)"""
import sys, os, subprocess, tempfile
HERE=os.path.dirname(os.path.abspath(__file__)); ROOT=os.path.dirname(os.path.dirname(HERE)); os.chdir(ROOT)
check='--check' in sys.argv; bad=0
for line in open('config/sdk_sources.txt'):
    line=line.split('#')[0].strip()
    if not line: continue
    dst,rel=line.split()
    src=os.path.join('original/sdk',rel)
    if not os.path.exists(src):
        sys.exit("sdksrc: %s is missing: ./setup.sh --sdk /path/to/midwayvideo"%src)
    data=open(src,'rb').read().replace(b'\r',b'')     # the SDK files carry CR (some CR CR) line endings
    patch='patches/'+dst[4:]+'.patch' if dst.startswith('src/') else None
    if patch and os.path.exists(patch):
        with tempfile.NamedTemporaryFile(delete=False,suffix=os.path.basename(dst)) as t: t.write(data); tmp=t.name
        r=subprocess.run(['patch','-s','--no-backup-if-mismatch',tmp,patch],capture_output=True,text=True)
        if r.returncode!=0: sys.exit("sdksrc: %s does not apply to %s:\n%s%s"%(patch,src,r.stdout,r.stderr))
        data=open(tmp,'rb').read(); os.remove(tmp)
    if check:
        cur=open(dst,'rb').read() if os.path.exists(dst) else None
        if cur!=data: print("differs:",dst); bad+=1
        continue
    if not os.path.exists(dst) or open(dst,'rb').read()!=data:
        os.makedirs(os.path.dirname(dst),exist_ok=True); open(dst,'wb').write(data); print("generated",dst)
if check: sys.exit(1 if bad else 0)
