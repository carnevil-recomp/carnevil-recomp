# Build: rebuilding GAME.EXE byte-exact

`make` rebuilds `build/GAME.EXE` from `asm/` and fails unless it is byte-identical to
`original/partition/GAME.EXE` (v1.0.3). Status: **byte-exact** since 2026-10-10.

```
make            # assemble asm/*.s -> link (config/game.ld) -> objcopy -> tools/exe.py pack -> cmp
make regen      # regenerate asm/ from the original with tools/disasm.py (asm/ is git-ignored: derived from the game binary)
make verify     # list differing words and add them to config/word_fallback.txt
python3 tools/bootstrap.py   # the full disasm/assemble/compare loop used to reach byte-exact
```

## Pieces
- `tools/exe.py` — the `.EXE` container: `u32 crc` + `u32 load address` + image. The CRC is
  the GOOSE `crc()` (crc.c) over everything after the first word; verified on GAME.EXE
  v1.0.1/v1.0.3 and DIAG.EXE. The BIOS (`EXEC.C`) checks it before jumping to the address.
- `tools/disasm.py` — splits the image into `asm/code_800c4000.s` (instructions with labels
  for every branch/jump target, Binary Ninja names for known functions) and `asm/data.s`
  (`.word` dump from 0x8016DC4C, the end of `.text` per the Binja sections). Mnemonics are
  restricted to MIPS IV; `move` is re-encoded from the raw word (`or` vs `addu`); anything in
  `config/word_fallback.txt` is emitted as `.word`.
- `config/game.ld` — flat link at 0x800C4000, `.text` then `.data`, debug sections discarded.
- `config/word_fallback.txt` — 60 words gas cannot reproduce from text: data inside `.text`
  (jump tables, the BIOS jump-table copy area, string fragments) that capstone decodes as
  instructions gas encodes differently or rejects.
- `tools/bootstrap.py` — iterates until exact; assembler errors are mapped back to addresses.

## What this gives us
Phase 0 exit criterion met: the repo reproduces the shipped binary and any edit that changes
a byte is caught by `make`. From here, matched C replaces regions of `asm/` incrementally:
a function compiled by `tools/sncc.sh` + `tools/sn2gas.py` is linked in place of its
instructions, and the link must still produce the identical image.

## Next
- Split `asm/code_800c4000.s` per function (needs global labels for cross-file branch targets)
  and symbolize `lui/addiu` and `lui/lw` pairs so relocations resolve through the linker —
  that is what allows C objects to be substituted without hand-patching addresses.
- Generate a DIAG.EXE build the same way (same container, load address 0x80100000).
