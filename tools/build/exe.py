#!/usr/bin/env python3
"""Midway Seattle .EXE container: u32 crc, u32 load/entry address, raw image.
The CRC is the GOOSE crc32 (crc.c) over everything after the first word (BIOS EXEC.C).
usage: exe.py unpack <file.exe> <image.bin>      (prints load address)
       exe.py pack   <image.bin> <addr> <file.exe>
       exe.py check  <file.exe>
"""
import sys, struct
_tab=[]
for i in range(256):
    c=i<<24
    for _ in range(8): c=((c<<1)^0x04c11db7)&0xffffffff if c&0x80000000 else (c<<1)&0xffffffff
    _tab.append(c)
def goose_crc(data):
    crc=0
    for b in data: crc=((crc<<8)&0xffffffff)^_tab[(crc>>24)^b]
    n=len(data)
    while n: crc=((crc<<8)&0xffffffff)^_tab[(crc>>24)^(n&0xff)]; n>>=8
    return (~crc)&0xffffffff
def main():
    cmd=sys.argv[1]
    if cmd=='unpack':
        d=open(sys.argv[2],'rb').read(); addr=struct.unpack('<I',d[4:8])[0]
        open(sys.argv[3],'wb').write(d[8:]); print("0x%08x"%addr)
    elif cmd=='pack':
        img=open(sys.argv[2],'rb').read(); addr=int(sys.argv[3],0)
        body=struct.pack('<I',addr)+img
        open(sys.argv[4],'wb').write(struct.pack('<I',goose_crc(body))+body)
    elif cmd=='check':
        d=open(sys.argv[2],'rb').read(); ok=struct.unpack('<I',d[:4])[0]==goose_crc(d[4:])
        print("crc", "OK" if ok else "BAD", "load 0x%08x size %d"%(struct.unpack('<I',d[4:8])[0],len(d)-8)); sys.exit(0 if ok else 1)
main()
