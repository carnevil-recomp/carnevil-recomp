#!/usr/bin/env python3
"""Decompilation progress: how much of GAME.EXE's text and data is built from C (config/modules.txt)."""
import re
LOAD=0x800c4000; CODE_END=0x8016dc4c; END=0x801a4224
mods=[l.split() for l in open('config/modules.txt') if l.strip() and not l.startswith('#')]
ct=cd=0; n=0
for f in mods:
    n+=1
    if f[3]!='-': ct+=int(f[4],16)-int(f[3],16)
    if f[5]!='-': cd+=int(f[6],16)-int(f[5],16)
    if f[7]!='-': cd+=int(f[8],16)-int(f[7],16)
tt=CODE_END-LOAD; td=END-CODE_END
print("modules in C: %d"%n)
print("text: %7d / %7d bytes from C  (%.2f%%)"%(ct,tt,100*ct/tt))
print("data: %7d / %7d bytes from C  (%.2f%%)"%(cd,td,100*cd/td))
