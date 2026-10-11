#!/usr/bin/env python3
"""GCC 2.7.2 accepted raw newlines inside string literals (the SDK's inline asm blocks use
them); modern preprocessors do not. Rewrite such literals as line-wise concatenated strings.
usage: fixasmstr.py <in.c> <out.c>"""
import sys
src=open(sys.argv[1],'rb').read().decode('latin1').replace('\r','')
out=[]; i=0; instr=False; esc=False; incomment=None
while i<len(src):
    c=src[i]
    if incomment=='block':
        out.append(c)
        if src.startswith('*/',i): out.append('/'); i+=2; incomment=None; continue
        i+=1; continue
    if incomment=='line':
        out.append(c); 
        if c=='\n': incomment=None
        i+=1; continue
    if not instr:
        if src.startswith('/*',i): incomment='block'; out.append('/'); i+=1; continue
        if src.startswith('//',i): incomment='line'; out.append('/'); i+=1; continue
        if c=='"': instr=True
        out.append(c); i+=1; continue
    # inside a string literal
    if esc: out.append(c); esc=False; i+=1; continue
    if c=='\\': out.append(c); esc=True; i+=1; continue
    if c=='"': instr=False; out.append(c); i+=1; continue
    if c=='\n': out.append('\\n"\n"'); i+=1; continue
    out.append(c); i+=1
open(sys.argv[2],'w').write(''.join(out))
