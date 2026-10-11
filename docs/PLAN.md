# CarnEvil (Midway, 1998) — Native PC Decompilation Plan

Goal: a 1:1 decompilation of CarnEvil's MIPS game executable into C, rebuilt byte-exact
against the original (`GAME.EXE`), and a native PC port of that source running on a thin
hardware-abstraction layer that reproduces the Seattle board's behaviour.

The plan is iterative: every phase produces something runnable or measurable, and later
phases never block on earlier ones being "100%".

---

## 0. What we actually have (survey results, 2026-10-10)

### 0.1 Hardware (confirmed from MAME `src/mame/williams/seattle.cpp`)

| Component | Detail |
|---|---|
| Board | Midway **Seattle** (no Widget board), config `seattle150` |
| CPU | NEC/IDT **R5000**, little-endian, 150 MHz (50 MHz system clock x3), MIPS IV ISA |
| System controller | Galileo **GT-64010** (RAM/PCI/timers/DMA/interrupts) |
| RAM | 8 MB (RAS0/1 mapped at phys 0x00000000) |
| Video | 3dfx **Voodoo 1** on PCI, 2 MB framebuffer, 1 TMU with 4 MB texture RAM, 640x480 ~57 Hz |
| Sound | **DCS2** board: ADSP-2115 DSP, 2 MB DRAM, sound ROM `sound102.u95`, polling offset 0x0af7 |
| I/O | Midway **IOASIC** (`SHUFFLE_CARNEVIL`, upper 469 = 25" cab / 486 = 39" cab), security PIC `486_carnevil.u96` (game id + serial) |
| Light guns | Custom gun board on GT-64010 CS3 at 0x00800000-0x1F (`carnevil_gun_r/w`), X/Y 8-bit split across nibbles |
| Storage | IDE hard disk (CHD `carnevil`, v1.0.3 / diag v3.4 Feb 1 1999), CMOS NVRAM |
| Boot ROM | `carnevil1_9.u32`, 512 KB, "Boot ROM v1.9" — present in `~/Downloads/carnevil/`, SHA1 matches MAME |
| Sound ROM | `sound102.u95`, 32 KB — present in `~/Downloads/carnevil/`, SHA1 matches MAME |
| Security PIC | `486_carnevil.u96` (8 KB) — **still missing**; MAME needs it (or a stub) to boot |
| Known emu gap | MAME comment: "Carnevil: lets you set the flash brightness; need to emulate that" |

### 0.2 Two versions, and the disk layout

| | v1.0.3 (**project target**) | v1.0.1 |
|---|---|---|
| CHD | `carnevil.chd`, MAME parent set `carnevil` | `carnevi1.chd`, MAME `carnevil1`, flagged BAD_DUMP |
| raw image | `~/Downloads/carnevil-chd/carnevil_103.raw` (chdman extractraw) | `~/Downloads/carnevil-chd/carn.raw` |
| partition dir | `~/Downloads/carnevil-chd/v103/partition/` (1523 files) | `v101/partition/` = the manual `out/_extracted_partition/` (1520 files) |
| GAME.EXE | 918,060 bytes, prints "CARNEVIL V1.0.3" | 915,212 bytes, prints "CARNEVIL V1.0.1" |
| Binary Ninja db | `GAME.EXE-2.bndb` is this build (bytes verified) | — |

The two builds differ in GAME.EXE, DIAG.EXE, the adjustment/audit scripts, the high-score
textures, two sound banks and a few textures (v1.0.3 adds `J.PRC`, drops `A.PRC`/`TIMEZONE.PRC`).

- Sector 0 holds a `TRAP` partition table (`tools/trap.py`, format from SDK `putil/lib/PD.H`):
  entry 0 spans the whole drive, entries 1–16 are the real partitions (start, length, type).
- Partition 1 (98.8 MB, type FATFS) = Midway **LFN32 flat filesystem** (Joe Linhoff's
  `FILESYS.C` in the BIOS source: no subdirectories, 16-bit FAT, 4 KB clusters, 24-byte
  directory entries with packed names, CRCs in `FILESYS.CHK` from `tools/genfchk`).
  `tools/lfn32.py` lists/extracts it and reproduces the manual extraction byte for byte.
- Partitions 3–16 (`part2`..`part16` in the manual dump) = raw **movie streams** written straight to partitions by `tools/movie`
  (frame = 194 x 512-byte blocks: 192 pixel blocks + 2 palette blocks; 386 blocks when a
  Z-buffer layer is included). `main.c`/`foo/` in this dir is the user's working decoder.
- Partition 2 (`part1`) = stray DJGPP environment junk, not game data.

### 0.3 Executable (`GAME.EXE`; numbers below are from the v1.0.1 survey, v1.0.3 is 918,060 bytes with the same layout)

- Format: 4-byte checksum/magic + 4-byte **load address 0x800C4000** + raw LE MIPS image
  (`tools/bin2exe`). `DIAG.EXE` is the same format, load address 0x80100000.
- ~228 K instructions, ~2,000 functions (2,320 `jr $ra`, 1,860 stack prologues).
- Entry sequence: clear BSS (0x801A3708..0x80800000), copy the **BIOS system jump table**
  from 0xA0000000 into the image at 0x800C408C, set FCSR = 0x01000000 (**flush-denormals-to-zero**),
  then libc init, `main`, exit.
- Statically linked inside it: 3dfx **Glide 2.03h3** (Midway-modified: has `grDrawTriangleDma`),
  DJGPP-derived libc + BSD/IDT portions, Midway **GOOSE** OS library, DCS **sound shell**.
- Instruction census worth noting: 6,069 single-precision FPU ops (float-heavy game logic),
  899 `ll/sc` (semaphores), 1,133 `cache` ops (DMA coherency), 287 `break` (GCC div-by-zero
  traps -> `div0hand.c`), 312 `syscall`, 107 COP0 accesses, some 64-bit ops (`-mips4` long long).

### 0.4 The SDK we already have (`../midwayvideo`, clone of github.com/historicalsource/midwayvideo)

This changes the project from "blind decomp" to "decomp with the OS source in hand":

| Path | Content |
|---|---|
| `lib/goose/*.c,*.S` | **Full source** of the GOOSE game OS library (process scheduler, objects, sprites, textures, CMOS, adjustments, fonts, semaphores, mthread, interrupt handlers, div0 handler) |
| `lib/sound/` | DCS sound shell source (`DCS_SHEL.C`, `sound.c`) + the same `OSYS.BIN/COMP.BIN/DM_*.BIN` DSP images found on the disk |
| `biosrom/bios/` | **Boot ROM source**: `carnbios.mak`, `IOASIC.C`, `DCS2DRV.C`, `idedrv.c`, `FILESYS.C`, `gt64010.c`, `cmosdrv.c`, `COINDRV.C`, `JTABLE.INC` (the syscall table GAME.EXE calls through) |
| `biosrom/start/` | `carncsu.s`, `carnrom.lnk`, `carnrom.mak` (CarnEvil-specific startup) |
| `include/` | libc headers, `goose/`, Glide 2.44 / 2.51 / 3.00 / Banshee headers |
| `tools/` | Source for `3df2wms` (textures), `bin2exe`, `genfchk`, `movie`, `buildrom`, `seq2ani`, `wan2seq`, `pack`, `tcomp`, `tgasplit`, etc. |
| `tools/bin/` | `CC1MIPS.EXE`, `CPPMIPS.EXE`, `CC1PLMIP.EXE`, `CWSDPMI.EXE` — DOS/DJGPP build of **GCC 2.7.2.SN16.3.3** (SN Systems Psy-Q MIPS) |

Build flags used for this library (from `goose.mak` / `goose.cf`), i.e. what the game was
almost certainly built with:
```
-mips4 -mcpu=r5000 -mhard-float -fomit-frame-pointer -G 0 -O -g -c
-fno-builtin -nostdinc -D__DJGPP__ -DSEATTLE -DRELEASE -Wall
```
Assembler/linker in the original flow: Psy-Q `asmmips` / `psylink` / `psylib` (NOT in the repo).
The makefiles also support `DEV_ENV=CYGNUS` using `mips64vr5000-elf-as/ar`, which means a
modern GNU binutils backend is a sanctioned path.

What is missing: the game-side source (none on the disk), the Glide 2.03h3 source (only
headers + two `.a` for the BIOS POST), the boot ROM binary, the sound ROM, and the PIC dump.

---

## 1. Guiding decisions

1. **Matching decomp is the source of truth.** The repo rebuilds `GAME.EXE` byte-exact. The PC
   port is the same C compiled with a modern compiler plus a HAL. Any divergence between the
   two builds is a bug in the HAL or a non-portable construct, never "port logic".
2. **Emulate, don't reinterpret, the proprietary bits.** Voodoo 1 rasterization quirks, DCS
   audio, IOASIC/PIC, gun timing and CMOS are reproduced behind the HAL. Faithfulness first;
   enhancements (resolution, filtering, widescreen) only as switchable layers added later.
3. **Reuse MAME as the oracle.** MAME already runs this game. Every milestone is validated by
   comparing against MAME traces (memory snapshots, input logs, frame hashes), not by eye.
4. **Assets stay out of the repo.** Tooling takes the user's CHD/`carn.raw` as input. The SDK
   is a public historical release; the game is not. Keep the project legally clean.
5. **Everything is scriptable and measurable.** Progress = % of bytes matched, % of functions
   in C, number of oracle checkpoints passing.

---

## 2. Phases

Each phase lists: goal, concrete steps, exit criteria. Phases 1-3 and 4 can run in parallel.

### Phase 0 — Project skeleton (week 1)

Goal: a repo where `make` produces a `GAME.EXE` from assembly and a test proves it matches.

Steps:
1. Create `carnevil-decomp/` (git) with this layout:
   ```
   docs/              this plan, hardware notes, file-format specs
   config/            splitter yaml, symbol tables (symbol_addrs.txt), linker script
   asm/               generated disassembly (nonmatching/ for WIP)
   src/               game C source   (src/game, src/goose, src/sound, src/libc, src/glide)
   include/           headers (ours + copied SDK headers with provenance noted)
   tools/             python: splitter, exe packer, filesystem/movie/asset decoders, progress, diff
   port/              PC HAL + platform main (SDL2 + OpenGL/Vulkan backend)
   test/              oracle traces, golden hashes, unit tests for decoders
   ```
   `assets/` and `original/` are git-ignored; a `setup.sh` extracts from the user's `carn.raw`.
2. Tool for the exe container: `tools/exe.py` pack/unpack (header = magic/checksum + load
   address). Determine how the 4-byte first word is computed (check `buildrom`/BIOS `EXEC.C`).
3. Write `tools/lfn32.py` (reader/writer for part0, using `FILESYS.C` as spec) and
   `tools/trap.py` (partition table) so extraction is reproducible from `carn.raw`.
4. ROMs: boot ROM and sound ROM are in `~/Downloads/carnevil/` and verified against MAME's
   SHA1s. Still needed: the security PIC dump `486_carnevil.u96` (or MAME run with a missing
   ROM override) for the oracle. Document SHA1s in `docs/`.
5. Fix the Binary Ninja MCP server venv (it currently fails to connect because the plugin's
   `.venv/bin/python3` path is missing) so Binja can be driven from here; also install Ghidra
   as the second opinion decompiler, and `capstone`, `m2c`, `objdiff`/`asm-differ`, `splat`
   (or our own flat-binary splitter), `mips64vr5000-elf` binutils (`brew`/crosstool).

Exit: `make` reassembles the raw disassembly and `sha1(GAME.EXE)` matches the original.
Progress script reports 0% C, 100% asm.
**Status 2026-10-10: done** — `make` rebuilds v1.0.3 GAME.EXE byte-exact (see `docs/build.md`);
`tools/exe.py`, `tools/trap.py`, `tools/lfn32.py` exist; toolchain reproduced (`docs/toolchain.md`).

### Phase 1 — Toolchain reproduction (weeks 1-3, parallel with Phase 2)

Goal: compile C with the *original* compiler so function matching is possible.

Steps:
1. Run `CC1MIPS.EXE` (DJGPP/CWSDPMI) under DOSBox-X or dosemu2 in a wrapper script
   (`tools/sncc.sh`) that takes preprocessed C in, assembly out. Pattern: the PS1 decomp
   community's Psy-Q wrappers (same GCC 2.7.2 SN lineage).
2. Assemble with modern GNU `as` (`-mips4 -mabi=32 -EL -G0`), as the SDK's `CYGNUS` path does.
   Verify the GNU-assembled output of SDK sources is byte-identical to Psy-Q-assembled
   output by matching against the copies linked inside `GAME.EXE` (the goose library is in
   there). If pseudo-op expansion differs, write a small post-processor.
3. Track down Psy-Q `asmmips`/`psylink` only if step 2 shows irreducible differences
   (section ordering, `.rdata` placement, literal pools).
4. Confirm the flag set against the binary: build `lib/goose/process.c` with the flags above
   and diff against the matching functions in `GAME.EXE` (find them via the
   `"$Revision: 19 $"` version strings every goose module embeds). Adjust `-O` level/`-g`
   until identical. Record the exact flag string as `config/cflags`.
5. Link step: GNU `ld` with a linker script that reproduces the original section order
   (`.text`, `.rdata`, `.data`, `.sdata`/`.sbss` with `-G 0`, `.bss`) and the load address.

Exit: an SDK library function compiled from source inserts into the build and the exe still
matches. Compiler wrapper runs on macOS/Linux in CI.

### Phase 2 — Static analysis and symbolization (weeks 1-4)

Goal: every function has a name, every known-library function is identified, data sections
are typed.

Starting point: the user's existing Binary Ninja database `~/Downloads/GAME.EXE-2.bndb`
(57 MB, Jan 2026; an older `GAME.EXE.bndb` from Jul 2025 also exists). Binary Ninja exposes
it over MCP at `http://127.0.0.1:24642/mcp` (no auth); `decomp-seed/binja_mcp.py` is a
working HTTP client for that endpoint and `decomp-seed/binja_symbols.txt` is the first
export. What the database already contains:
- 2,611 functions found by analysis; **509 user-named**, 243 of them with real prototypes.
- 294 named data items: level scripts (`hhouse_script`, `freakshow_script`, ...), per-enemy
  texmap/anim/spawn blobs, player stats, credit/CMOS structs, process and texture lists.
- 84 types, including Glide's public types and GOOSE's `process_node`, `texture_node`,
  `snd_bank_node`, `adjustment_info`, `spawn_struct`, `light_info`, `player_info`.
- Named regions already cover: startup/handlers, main/game/diag/coin processes, all
  `load_*/remove_*` asset loaders per enemy and level, movie player, high scores, CMOS,
  the GOOSE process/texture API, the Glide/sst1 init layer, and the whole `snd_*` API.
- Note: the view maps the file including its 8-byte header at 0x800C3FF8, so Binja addresses
  equal file offset + 0x800C3FF8 and `goose_init` at 0x800C3FF8 really starts at 0x800C4000.
How it feeds the project:
- `tools/symbols.py` converts the export into `config/symbol_addrs.txt`; re-export after each
  reversing session (additive merge, conflicts flagged). The bndb stays a working view; the
  repo's symbol files are canonical.
- The SDK source makes most of the 2,100 unnamed functions nameable mechanically (goose,
  sound, libc, BIOS jump table); Binja names are then reconciled against SDK names.
- Headless Python is unavailable (Personal license), so all automation goes through MCP.

Steps:
1. Auto-identify the four linked libraries by **signature matching**:
   - **GOOSE** and **sound shell**: compile the SDK sources (Phase 1) and match by bytes/relocs.
   - **libc** (DJGPP-derived, `biosrom/bios/*.C` + `include/libc`): same approach, since the
     BIOS build includes the libc that `sys_jtable` fronts.
   - **Glide 2.03h3**: no source. Use the Glide 2.4x headers plus public Glide 2.x source
     (3dfx released Glide under GPL in 1999; 2.x source for Voodoo 1 exists) to name
     functions by string references and structure (`grSstOpen`, `_GlideRoot`, triangle setup).
     Note `grDrawTriangleDma` is a Midway addition.
2. Reconstruct the **BIOS syscall table** from `JTABLE.INC`: the image at 0x800C408C is a copy
   of `sys_jtable`, so every `jal 0x800C40xx` maps to a named BIOS service. Name them all.
3. Name game functions from the ~3,000 strings (`load_model`, `load_animation`, `load_path`,
   `load_camera`, `Movie_load`, `get_object_data`, `create_texture`, `coin_volume_proc`,
   `ztream_fill_proc`, `grab_save_screen`, `main_init_sound`, ...). Carve the process table:
   `"Process name %s, start address %x"` means GOOSE keeps names for every process, so the
   process creation sites give us the top-level architecture (one process per
   enemy/level/UI element).
4. Type the data: adjustment/audit tables (`ADJUST.FMT`, `AUDITS.FMT` parsers in goose
   `adjust.c`), pricing (`A.PRC`, `TIMEZONE.PRC`), the file list string table (every asset
   name appears in the exe), level descriptor tables, object type table (`"add object TYPE %04X"`).
5. Produce `config/symbol_addrs.txt` and a splitter yaml so `asm/` is split per original
   source file (goose modules are separable by their revision strings; game modules by
   string clustering and file-order heuristics).

Exit: 100% of library functions named and matched to source; game functions named with
confidence tags; data segments split into typed files; ~30-40% of the exe already "matched"
via SDK source.

### Phase 3 — Matching decompilation of game code (months 2-8, the long tail)

Goal: all ~1,200 game-specific functions in C, byte-matching.

Workflow per function (standard decomp loop):
1. `m2c` first pass -> clean up with Ghidra/Binja output as reference.
2. Compile with `sncc.sh`, compare with `objdiff`/`asm-differ`, iterate to 100%.
3. Commit with the progress script updating `docs/progress.md`. Optionally mirror on a
   private decomp.me instance with the SN compiler preset for collaboration.

Order of attack (each step gives the port something to run):
1. `main`, init, the process scheduler glue, file loading (`load_*`, CRC + backup-file
   fallback — note `*.ZM2/ZA2/PT2/RW2/RA2` are byte-identical backups used when CRC fails).
2. Attract mode, movie player (`Movie_load`, `ztream_fill_proc` streams from partitions),
   coin/credit/pricing, adjustments & audits menus (text-driven, goose-heavy).
3. Rendering pipeline: texture manager (`TexMem size`), sprite lists (normal/alpha/back),
   lights (`PARALLEL LIGHTS`, falloff), model draw, camera paths (rail shooter).
4. Gun input, calibration (`invalid calib`), hit testing, screen flash.
5. Game objects: the object system (`add object`, ids, zdie list), enemies per level
   (Haunted House, Rickety Town, Freak Show, Big Top, plus Gallery), bosses, scoring,
   high-score autograph.
6. Sound command layer (DCS messages), audits, CMOS save, serial-number/PIC checks.
7. `DIAG.EXE` last (optional, but it is the operator-menu reference and shares code).

Exit: 100% matching `GAME.EXE`; all non-matching functions documented.

### Phase 4 — Data formats and asset pipeline (parallel, months 1-4)

Goal: documented, tested decoders (and encoders where tools exist) for every file type, so
the port loads original assets and we can prove our understanding.

| Format | Count | Source of truth | Deliverable |
|---|---|---|---|
| `.WMS` textures | 1144 | `tools/3df2wms/3DF2WMS.C`, goose `texture.c`, Glide texture formats | `wms.py` -> PNG + metadata; LOD/format table |
| `.ZM` models | 73 | `load_model` in exe (floats; "More vertices than I can handle") | `zm.py` -> OBJ/glTF for inspection |
| `.ZA` animations | 42 | `load_animation` ("%d frames, %d groups"), `tools/seq2ani`, `wan2seq` | `za.py` |
| `.PTH` camera paths | 26 | `load_path`, `load_camera` | `pth.py` |
| `.RAW/.RW2/.RA2` level data | 5 levels | exe loader; `GALL`=gallery | `lvl.py` |
| `.BNK` DCS 3.0F banks | 62 | MAME `dcs.cpp`, `DCS_SHEL.C` | `bnk.py` (index + ADPCM decode for listening) |
| `.FMT/.PRC/.REV/.CHK` | few | goose `adjust.c`, `cmos.c`, `genfchk` | parsers in `tools/` |
| movie partitions | 15 | `tools/movie/movie.c` (+Z-buffer variant) | finalize existing decoder from `main.c` |
| `CREDITS.RAW` | 1 | exe | decoder |

Exit: every file in part0 opens with a decoder without errors; round-trip tests for the
formats with an encoder (WMS, CHK).

### Phase 5 — Portable build and HAL (months 3-9, starts as soon as init + attract exist)

Goal: the decompiled C compiles with clang/gcc for x86-64/arm64 and runs on a HAL that
reproduces the board. This is the "port" and is layered to keep the game source untouched.

HAL modules (`port/`), each with a MAME-derived reference:

1. **CPU/memory semantics**: `uint32_t` pointers assumptions, KSEG0/KSEG1 aliasing (uncached
   writes to 0xA0xxxxxx), `cache` ops -> no-ops, `ll/sc` -> atomics, `break` div0 trap ->
   explicit check that mimics `div0hand.c`, FCSR FTZ -> `_MM_SET_FLUSH_ZERO_MODE` /
   `FPCR.FZ` on arm64, `-ffp-contract=off`, strict single precision.
2. **BIOS services** (`sys_jtable`): file I/O over an LFN32 image or extracted directory,
   date/time, CMOS as a file, `exec`, exception hooks. Source reference: `biosrom/bios/`.
3. **GT-64010**: timers/tick interrupt, DMA used by `grDrawTriangleDma` and movie streaming.
4. **IOASIC + PIC**: coins, service, start buttons, DIPs, serial number/game id; gun board
   register emulation (CS3) fed by mouse/gun input mapped through the same nibble layout.
5. **DCS2 audio**: port MAME's ADSP-2115 core + DCS2 glue (GPL/BSD-3 since MAME relicensed)
   running the real `OSYS/COMP` DSP code and `.BNK` data; audio output via SDL. This is the
   only honest 1:1 path for sound. (Alternative tier: pre-decoded WAV playback, flagged as
   non-faithful.)
6. **Glide 2.03h3 -> GPU**: implement the exact Glide 2.x API surface the game uses over
   OpenGL 4.x/Metal via a software-faithful path:
   - 16-bit RGB565 colour buffer and dithering, W/Z buffer modes, fog tables, chroma-key,
     alpha blend/combine modes, texture formats (incl. 8-bit palettised + NCC), LOD/mipmap,
     clamp/wrap, `grLfbLock` for movie blits and screenshots, `grGammaCorrection` for the
     screen-flash brightness adjustment.
   - First implementation: software rasterizer ported from MAME's `voodoo` core (pixel-exact
     oracle). Second: GPU implementation validated against the software one per frame.
7. **Frame pacing**: lock to the Voodoo-derived ~57 Hz vblank; scheduler tick identical.

Exit criteria per milestone:
- **P1** boots, shows Midway/3dfx logos and attract movie with sound.
- **P2** coin-up, gun calibration, Haunted House fully playable.
- **P3** all levels, bosses, gallery, high scores, operator menus, audits/CMOS persistence.

### Phase 6 — Verification harness (continuous from Phase 2)

1. **MAME Lua oracle**: scripts that boot `carnevil` in MAME, play back an input log, and dump
   (a) RAM hashes at fixed frame numbers, (b) selected structs (object list, RNG, score),
   (c) framebuffer hashes. Stored under `test/golden/`.
2. **Port replay**: same input log drives the port; compare (a)/(b) exactly, (c) exactly for
   the software rasterizer tier and within tolerance for the GPU tier.
3. **Build matching CI**: every commit reassembles `GAME.EXE` and fails on mismatch; progress
   badge from `tools/progress.py`.
4. **Decoder tests**: golden hashes per asset file.

### Phase 7 — Polish and optional enhancements (after P3)

Only after a 1:1 "original" mode exists: internal resolution scaling, texture filtering
toggles, 2-player on one machine with two mice/guns, save/load of CMOS, windowed mode,
input remapping. All behind flags; default is faithful.

---

## 3. Known quirks to expect (keep this list growing in `docs/quirks.md`)

- **FTZ floats**: FCSR 0x01000000 at boot; all float math must flush denormals or physics drift.
- **MIPS IV conditional moves / 64-bit ops** in `-mips4` code; port with explicit C, not intrinsics.
- **Div-by-zero trap emulation**: GCC emits `break 7`; `div0hand.c` catches it and returns a
  value ("INTEGER DIVIDE BY ZERO" strings). Reproduce the handler's result, not UB.
- **Uncached aliasing & DMA**: triangle lists and movie frames go through DMA with explicit
  cache flushes; ordering assumptions may hide in the code.
- **Backup files**: loader retries `*.?2` twins on CRC failure; harmless but keep the semantics.
- **Voodoo 1 colour**: 16-bit with dithering; gun hit-testing may read back the framebuffer
  (colour-keyed hit detection is common in Midway gun games) — verify, this would make
  rasterization fidelity gameplay-critical.
- **Screen flash brightness** uses a path MAME does not emulate yet (likely Voodoo gamma/LFB).
- **Gun sampling** is tied to raster timing on the real gun board; MAME just reads positions.
- **PIC serial / cabinet type** (469 vs 486) changes the game id and some adjustments.
- **CMOS layout** is generated from `AUDITS.FMT` (8000 bytes, 182 audits, 100 adjustments).
- **DCS timing**: the game polls the sound board; latency affects cue sync, not logic.
- **Heap/texture memory** limits (`TexMem size`, "Not enought memory" paths) must stay at the
  original sizes so the same load/unload decisions happen.

---

## 4. First two weeks, concretely

1. Create the repo and move `DECOMP_PLAN.md` there; add `setup.sh` that splits `carn.raw` via
   the TRAP table and extracts part0 with our own LFN32 reader (replacing the manual extraction).
2. Write `exe.py`, verify repack == original for `GAME.EXE` and `DIAG.EXE`.
3. Full disassembly to `asm/`, reassemble with GNU as, byte-match (Phase 0 exit).
4. Get `CC1MIPS.EXE` running in DOSBox-X; compile `lib/goose/sincos.c` and `crc.c`; find them in
   `GAME.EXE`; achieve first byte-matches (Phase 1 step 4). This single experiment tells us
   whether the toolchain hypothesis is right and sets the real difficulty of Phase 3.
5. Through the Binja MCP endpoint: name the BIOS jump table entries from `JTABLE.INC`, then
   all string-referencing functions; re-export `symbol_addrs.txt`.
6. Finish the movie decoder using `tools/movie/movie.c` as spec (Z-buffer variant included).
7. Write `wms.py` from `3DF2WMS.C` and dump all 1144 textures to PNG as a sanity check.
8. Get MAME running `carnevil` (boot + sound ROM in hand; sort out the PIC) with the Lua
   oracle script producing its first RAM hash log.

---

## 5. Open questions (answer early, they shape later phases)

- Does the game read back the framebuffer for gun hit detection? (grep for `grLfbReadRegion`
  / `grLfbLock` call sites in the hit-test path.)
- Exact meaning of the first 4 bytes of `GAME.EXE` (checksum algorithm; see BIOS `EXEC.C`).
- Is any Glide 2.03h3 source obtainable, or do we reconstruct from the Glide 2.x GPL release?
- (answered) The manual extraction was v1.0.1; the project targets v1.0.3, see 0.2.
- Psy-Q `asmmips`/`psylink` availability if GNU binutils cannot reproduce section layout.
