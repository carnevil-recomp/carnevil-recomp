#!/usr/bin/env python3
"""Extract an enemy's .data tables from GAME.EXE as C initializers: its texmap_t table and
its submesh_t table (see model.h), or a plain table of floats (a path: COUNT points of PER floats,
x, y, z and three angles for the enemy paths). usage:
  extract_tables.py texmap NAME ADDR COUNT   |   extract_tables.py submesh NAME ADDR COUNT
  extract_tables.py floats NAME ADDR COUNT [PER]"""
import struct, glob, os, sys
here = os.path.dirname(os.path.abspath(__file__)) + '/../..'
BASE = 0x800c4000
exe = None
for p in glob.glob(here + '/original/partition/*'):
    if p.lower().endswith('game.exe'): exe = open(p, 'rb').read()
kind, name, addr, count = sys.argv[1], sys.argv[2], int(sys.argv[3], 16), int(sys.argv[4], 0)
per = int(sys.argv[5], 0) if len(sys.argv) > 5 else 6
def f(x):
    s = '%.9g' % x
    if 'e' not in s and '.' not in s: s += '.0'
    return s + 'f'
o = addr - BASE + 8
if kind == 'texmap':
    print('texmap_t\t%s[%d] =\t\t/* 0x%08x */\n{' % (name, count, addr))
    for i in range(count):
        key, flags, param, index = struct.unpack('<iiii', exe[o:o + 16]); nm = exe[o + 16:o + 32].split(b'\0')[0].decode()
        print('\t{ (int)0x%08x, %d, (int)0x%08x, %d, "%s" },' % (key & 0xffffffff, flags, param & 0xffffffff, index, nm)); o += 32
    print('};')
elif kind == 'submesh':
    print('submesh_t\t%s[%d] =\t\t/* 0x%08x */\n{' % (name, count, addr))
    for i in range(count):
        fs, fe, anim, vs, ve, ns, ne, r, px, py, pz = struct.unpack('<hhIhhhhffff', exe[o:o + 32])
        print('\t{ %d, %d, 0, %d, %d, %d, %d, %s, { %s, %s, %s } },' % (fs, fe, vs, ve, ns, ne, f(r), f(px), f(py), f(pz))); o += 32
    print('};')
elif kind == 'floats':
    print('float\t%s[%d * %d] =\t\t/* 0x%08x */\n{' % (name, count, per, addr))
    for i in range(count):
        v = struct.unpack('<%df' % per, exe[o:o + 4 * per])
        print('\t' + ', '.join(f(x) for x in v) + ','); o += 4 * per
    print('};')
