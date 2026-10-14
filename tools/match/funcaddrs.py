#!/usr/bin/env python3
"""Print name=0xaddr for every function defined in a C file, for tools/match/trymatch.sh.
The address comes from config/names.txt, config/binja_symbols.txt or the "/* 0x800..." comment
above the definition. usage: funcaddrs.py src/game/file.c"""
import re, sys, os
here = os.path.dirname(os.path.abspath(__file__)) + '/../..'
names = {}
for line in open(here + '/config/binja_symbols.txt'):
    f = line.split('\t')
    if len(f) >= 3 and f[1] == 'func': names[f[2]] = int(f[0], 16)
for line in open(here + '/config/names.txt'):
    f = line.split()
    if len(f) >= 2 and f[0].startswith('0x'): names[f[1]] = int(f[0], 16)
src = open(sys.argv[1]).read().split('\n')
out = []
last_comment_addr = None
for i, l in enumerate(src):
    m = re.search(r'/\* (0x800[0-9a-f]{5})', l)
    if m: last_comment_addr = int(m.group(1), 16)
    m = re.match(r'^(?:static )?(?:inline )?[a-zA-Z_][\w]*(?:\s+\*?|\s*\*\s*)(\w+)\(', l)
    if m and ';' not in l and not l.startswith(('extern', 'typedef')):
        name = m.group(1)
        if name in names: addr = names[name]
        elif name.startswith('func_'): addr = int(name[5:], 16)
        elif last_comment_addr is not None: addr = last_comment_addr
        else: continue
        out.append('%s=0x%08x' % (name, addr)); last_comment_addr = None
print(' '.join(out))
