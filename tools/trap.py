#!/usr/bin/env python3
"""Midway 'TRAP' hard-disk partition table (see SDK tools/putil/lib/PD.H).

Sector 0: 'TRAP', u32 count, then count x {u32 starting_block, u32 num_blocks, u32 type}.
Entry 0 always covers the whole drive (raw access); types: 0 raw, 1 FAT filesystem (the
Midway LFN32 filesystem of biosrom/bios/FILESYS.C), 2 ext2.

usage: trap.py list <disk.raw>
       trap.py extract <disk.raw> <index> <out.bin>
"""
import sys, struct
SECTOR=512
def read_table(f):
    f.seek(0); h=f.read(SECTOR)
    if h[:4]!=b'TRAP': raise SystemExit("no TRAP signature")
    n=struct.unpack('<I',h[4:8])[0]
    return [struct.unpack('<III',h[8+12*i:20+12*i]) for i in range(n)]
def main():
    cmd=sys.argv[1]; f=open(sys.argv[2],'rb'); t=read_table(f)
    if cmd=='list':
        print("idx  start_block  num_blocks   bytes        type")
        for i,(s,n,ty) in enumerate(t): print("%-4d %-12d %-12d %-12d %s"%(i,s,n,n*SECTOR,{0:'raw',1:'fatfs(LFN32)',2:'ext2'}.get(ty,ty)))
    elif cmd=='extract':
        s,n,ty=t[int(sys.argv[3])]; f.seek(s*SECTOR); out=open(sys.argv[4],'wb'); left=n*SECTOR
        while left>0:
            b=f.read(min(left,1<<24)); out.write(b); left-=len(b)
            if not b: break
        print("wrote",sys.argv[4])
main()
