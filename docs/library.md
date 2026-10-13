# GOOSE / sound library status

Every SDK library source and what CarnEvil v1.0.3 links. "rev" is the `$Revision$` string the
game carries (the SDK clone is newer); differences are commented in the module source.

The library sources are not committed: `make` generates `src/goose/*.c`, `src/goose/pshell.s`
and `src/sound/sound.c` from your SDK checkout (`config/sdk_sources.txt` maps each file;
`tools/sdksrc.py` strips the CR line endings and applies `patches/<file>.patch`). The patches
are the differences between the SDK's revision of a file and the one CarnEvil links, plus
the build fixes; three files (adjust.c, cmos.c, sincos.c) need none. `python3 tools/sdksrc.py
--check` reports a generated file that was edited by hand: edit the patch instead
(`make sdksrc` regenerates), or regenerate the patch from the edited file with difflib.

| SDK source | game rev | SDK rev | module (profile) | notes |
|---|---|---|---|---|
| adjust.c | - | - | src/goose/adjust.c (goose) | |
| anim2d.c | 14 | 14 | src/goose/anim2d.c (goose) | |
| bgnd.c | 7 | - | src/goose/bgnd.c (goose) | verts always malloc'd |
| cmos.c | - | - | src/goose/cmos.c (goose) | |
| crc.c | - | - | src/goose/crc.c (goose) | |
| div0hand.c | 4 | - | src/goose/div0hand.c (goose) | |
| draw.c | 2 | - | src/goose/draw.c (goose) | |
| fonts.c | 7 | - | src/goose/fonts.c (goose) | |
| galhand.c | 4 | - | src/goose/galhand.c (goose) | |
| GFADER.C | 6 | 9 | src/goose/gfader.c (goose) | no fade_complete |
| INITG.C | - | - | src/goose/initg.c (game) | main + graphics_init |
| inthand.c | 3 | 3 | src/goose/inthand.c (goose) | uptime counts gfx_scale ticks |
| LOCKUP.C | 16 | 30 | src/goose/lockup.c (goose) | 13 commands, install_vector asm in .data |
| mthread.c | 13 | - | src/goose/mthread.c (noopt) | context switch, transcribed asm |
| object.c | 9 | - | src/goose/object.c (goose) | |
| ostrings.c | 19 | 24 | src/goose/ostrings.c (goose) | no change_string_cc |
| process.c | 17 | - | src/goose/process.c (noopt) | |
| prochook.c | 2 | - | src/goose/prochook.c (goose) | |
| randper.c | 3 | 4 | src/goose/randper.c (goose) | no zero guard |
| sincos.c | - | - | src/goose/sincos.c (goose) | |
| sprites.c | 20 | 32 | src/goose/sprites.c (goose) | see file header |
| switch.c | 14 | - | src/goose/switch.c (goose) | |
| texture.c | 12 | 16 | src/goose/texture.c (goose) | |
| TRANS.C | 3 | 4 | src/goose/trans.c (goose) | |
| unimhand.c | 7 | - | src/goose/unimhand.c (goose) | |
| PSHELL.S | 2 | 2 | src/goose/pshell.s (asm) | GNU as transcription |
| sound/sound.c | 34 | 42 | src/sound/sound.c (o2r5k) | built at -O2 (sound.mak CC2) |

Not linked by the game (no trace of their code, strings or version strings in GAME.EXE):
gext.c (Glide-3 TMU manager for Vegas), sema.c, versions.c (goose and sound), TEXTOVLY.C,
FONT8X8.C, QHOOK.S, INTSUP.S (`if 0`), INSTHAND.S, GETCXSP.S, sound/DCS_SHEL.C.
SETCMOS.C is a per-game template; CarnEvil's own `setup_cmos` (0x800f2a28) belongs to the game
code phase.

What remains between the library modules is DJGPP libc (qsort, random, vsprintf, strtol,
strnicmp, ...), 3dfx Glide 2.x (0x80152310-0x80169450 apart from TRANS.C) and the texture
memory manager (`get_mm_frag`, `show_tmu_map`); none of these have source in the SDK.
