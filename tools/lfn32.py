#!/usr/bin/env python3
"""Midway LFN32 flat filesystem reader (Joe Linhoff's FILESYS.C in the Seattle BIOS source).

Layout, in 512-byte sectors relative to the partition start:
  sector 2        info sector: magic 0x34011280, sectors/cluster (8), first FAT sector,
                  root dir sector, FAT type (16), boot CHS/len/adr/sum, header sum
  FAT             16-bit entries, two per word; entry = next cluster, 0 = end of chain;
                  clusters 0..2 reserved, cluster 3 = root directory
  cluster c       sector = rootsector + (c-3)*8  (4096-byte clusters)
  directory       array of 24-byte entries {u32 nm[3] packed big-endian ASCII, u32 size in
                  32-bit words, u32 date yr.6(+1990):mon.4:day.5:hr.5:min.6:sec.6, u32 flag.8:0.8:cluster.16};
                  nm[0]==0 means deleted; last word of the cluster links the next dir cluster
No subdirectories exist.

usage: lfn32.py list <partition.bin|disk.raw[@partition_index]>
       lfn32.py extract <same> <outdir>
"""
import sys, os, struct
SECTOR=512; SPC=8; CLUSTER=SECTOR*SPC; MAGIC=0x34011280
class FS:
    def __init__(self, f, base=0):
        self.f=f; self.base=base
        info=self.sectors(2,1)
        w=struct.unpack('<12I',info[:48])
        if w[0]!=MAGIC: raise SystemExit("bad LFN32 magic 0x%08x"%w[0])
        if w[1]!=SPC: raise SystemExit("unexpected sectors/cluster %d"%w[1])
        self.fatsector=w[2]; self.rootsector=w[3]; self.fattype=w[4]
        if self.fattype!=16: raise SystemExit("unexpected FAT type %d"%self.fattype)
        nfat=self.rootsector-self.fatsector
        fat=self.sectors(self.fatsector,nfat)
        self.fat=struct.unpack('<%dH'%(len(fat)//2),fat)
    def sectors(self,s,n):
        self.f.seek(self.base+s*SECTOR); return self.f.read(n*SECTOR)
    def cluster(self,c): return self.sectors(self.rootsector+(c-3)*SPC,SPC)
    def next_cluster(self,c): return self.fat[c-3]   # FAT entry 0 belongs to cluster 3 (fsClusterToFATSector)
    def entries(self):
        c=3; seen=set()
        while c and c not in seen:
            seen.add(c); buf=self.cluster(c)
            for i in range((CLUSTER-4)//24):
                e=buf[i*24:(i+1)*24]; nm=struct.unpack('<III',e[:12])   # char 0 is in bits 31..24 of word 0
                if nm[0]==0: continue
                size,date,fc=struct.unpack('<III',e[12:24])
                name=struct.pack('>III',*nm).rstrip(b'\0').decode('latin1')
                yield dict(name=name,size=size*4,date=date,flags=fc>>24,cluster=fc&0xffff)
            c=struct.unpack('<I',buf[-4:])[0]
    def read(self,ent):
        out=bytearray(); c=ent['cluster']; left=ent['size']; seen=set()
        while c and left>0 and c not in seen:
            seen.add(c); out+=self.cluster(c)[:min(CLUSTER,left)]; left-=CLUSTER; c=self.next_cluster(c)
        return bytes(out)
def fmtdate(d):
    return "%04d-%02d-%02d %02d:%02d:%02d"%(1990+(d>>26),(d>>22)&15,(d>>17)&31,(d>>12)&31,(d>>6)&63,d&63)
def open_fs(spec):
    if '@' in spec:
        path,idx=spec.rsplit('@',1); f=open(path,'rb'); f.seek(0); h=f.read(SECTOR)
        s,n,ty=struct.unpack('<III',h[8+12*int(idx):20+12*int(idx)]); return FS(f,s*SECTOR)
    return FS(open(spec,'rb'))
def main():
    cmd=sys.argv[1]; fs=open_fs(sys.argv[2]); ents=list(fs.entries())
    if cmd=='list':
        print("%-13s %10s  %-19s flags cluster"%("name","bytes","date"))
        for e in ents: print("%-13s %10d  %s  0x%02x  %d"%(e['name'],e['size'],fmtdate(e['date']),e['flags'],e['cluster']))
        print(len(ents),"files")
    elif cmd=='extract':
        os.makedirs(sys.argv[3],exist_ok=True)
        for e in ents:
            open(os.path.join(sys.argv[3],e['name']),'wb').write(fs.read(e))
        print("extracted",len(ents),"files to",sys.argv[3])
main()
