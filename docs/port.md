# The host port

`make port` builds `build/host/carnevil`: the GOOSE library, the DCS sound library and the
game modules from `src/`, the same files the byte-exact MIPS build uses, compiled natively and
linked with the host layer in `port/`.

```
make port
build/host/carnevil --partition original/partition --raw original/carn.raw [--state DIR] [--wav FILE]
    [--press F:switch,...] [--frames N] [--fast] [--headless] [--dump f.ppm | --dump-every prefix N] [-v]
```
`--partition` is the directory with the extracted LFN32 partition (what `setup.sh` links as
`original/partition`; `CARNEVIL_PARTITION` works too), `--raw` the raw disk image the movies
stream from. `--frames N` stops after N display frames, `--fast` disables frame pacing,
`-v` logs file opens and sound-board traffic, twice also every ioctl. The README lists the
keyboard controls and the `--press` switch names.

## How the original code runs on a PC

**One source, two targets.** `src/` is ILP32 C for the SN GCC 2.7.2; under `-DPORT_HOST`
(`include/types.h`, `port/runtime/compat.h`) it compiles with clang. Nothing in `src/` is
rewritten for the port; host-only paths are `#ifdef PORT_HOST` blocks (initg's `$gp` stub,
lockup's BIOS vector install, switch's bus-timing spin).

**Pointers are ints.** The SDK passes pointers through `int` (`_ioctl(4, req, (int)&val)`, the
process-stack marker, `(int)verts & 0x1c`, process arguments). That is only correct if every
address the game sees fits in 32 bits, so `port/runtime/mem.c` replaces malloc/free/calloc/
realloc with an allocator on an arena mapped at 0x20000000, and every stack (the GOOSE main
loop's in `main.c`, each process' in `sched.c`) is carved from it. The arm64 macOS kernel
refuses executables that map below 4 GB, so on macOS the port is built as x86_64 and runs
under Rosetta (the trick Wine uses); on Linux it is native with `-no-pie`. libc fortification
trips on these idioms, so it is off (`PORT_FORTIFY=2` to turn it on while debugging).

**Scheduler.** `port/runtime/sched.c` is mthread.c's `process_dispatch()` with the hand-written
MIPS register switching replaced by ucontext: each process node gets a host stack and context
the first time the dispatcher resumes it (create_process() leaves `_proc_shell` as a marker in
the new stack, exactly as the MIPS code expects), `sleep()` ORs the tick count and yields,
`die()` kills and switches back. process.c, the lists and the frame loop order (gamma table,
vblank wait, DMA kick, pre-proc, processes, post-proc, draws, sort) are the original.

**Hardware.** `port/runtime/hal.c`: the extracted partition is the disk (paths resolve
case-insensitively; `pci:` and `pic:` are fake fds for the system controller and the
security PIC), fd 3 is the CMOS (`cmos.bin` in the state directory, one byte per word as
GOOSE expects), ioctl device 4 is the I/O ASIC (switch words from the keyboard or `--press`,
the sound port wired to the virtual DCS board), device 5 the timers; the light-gun port and
the gun board's control byte are mapped pages. Interrupt handlers are recorded and the
vblank one is called once per display refresh (54 Hz) by `hal_wait_frame()`.
`port/video/render_gl.c` is the Glide stand-in on OpenGL, `disk.c` the raw-partition reader
for the movies, `dcs_board.cpp` the sound board, `audio_*.c` the output. `port/stubs/game_stub.c`
holds what is still missing from the game code (the level assets and scripts, the game
proper, diagnostics) as printing stubs.

## Status
The port boots through GOOSE main() into the game's real main process (boot sequence
below), then the attract process (`src/game/core/attract.c`, the real `game_proc`) runs on the
decompiled object list, texmap, text, movie, sound and credits modules. Verify with

```
build/host/carnevil --partition original/partition --raw original/carn.raw --frames 1001 --fast --dump-every f 1000
```
which leaves `f.001000.ppm`, a frame of the intro movie.

The game flow runs through the real code: a start creates the main game process
(main.c, `main_game_proc`), which loads the level-select gallery (wave.c), lets the
countdown pick a level (no gun aiming yet, so Rickety Town first), loads it with its sound
banks, camera file, spawn table and music stream, and plays its movie under the level
script (movie_scripts.c, extracted by `tools/extract/extract_scripts.py`), lights (lights.c), the
light gun (shoot.c), the enemies the spawn table places (krinkle.c, term.c through model.c
and the sprite and clipper files) and their sounds. What the level still lacks is stubbed:
the other enemies' models and texmaps (`port/stubs/wave_stubs.c`), the script's per-level
callbacks (`port/stubs/script_stubs.c`, weak definitions a real one overrides), the HUD and
the per-level collision tests. Every stub reports the first time it is reached
(`[stub] name reached at vblank N`, `stub_reached` in hal.c) and the run ends with the
list, so nothing missing is a silent no-op; the hardware stand-ins (heap size, refresh rate,
texture memory statistics) are not stubs and stay quiet. A Rickety Town run reached 61
stubs on 2026-10-14, 53 by the end of that day, 39 the next morning (the weapons file,
the point sprites and the explosion object came in) 35 with the select screen, 29 with the jester, and 6 with the rest of Rickety Town (2026-10-14: the dinosaur, the girl, Smiling Bob, the teens, Krampus and the pickups, plus the model/animation/path removers, the limb-detach code, the breath puffs and the helpers they share). The six left are boot-time and high-score pieces. At game
over the console shows "Error, already have a camera!": the original prints it too (its
game-over path preloads the high-score wave before unloading the level). With `CARNEVIL_TRACE=1` the loaders report their steps; a headless run
through the first level:

```
./build/host/carnevil --partition original/partition --raw original/carn.raw \
    --headless --fast --frames 3600 --press 1100:coin,1110:coin,1150:start1
```
Matching status of the game modules (per function, `tools/match/trymatch.sh`, 2026-10-13):

| module | profile | matched |
|---|---|---|
| credits.c | game2 (-O2) | 28 of 32 at 100%; credit_info 99%, setup_pricing 98%, ccproc_proc 98%, draw_hbar 78%, start_proc 7% (a branch the compiler turns into `sltu`) |
| hwinfo.c | game2 | 2 of 2 |
| audits.c | game1 (-O) | 16 of 16, registered in modules.txt (text 0x800f18e0-0x800f1d84, data 0x8018ea60-0x8018ed48) |
| adjust.c | game2 | 2 of 3; game_setup_cmos 98% |
| hiscore.c | game0 (-O0, `register`) | 4 of 4 |
| soundio.c | game2 | 2 of 2 (main_init_sound, check_sound_os; the cue helpers moved to levelsnd.c) |
| levelsnd.c | game2 | 36 of 36, registered in modules.txt (text 0x800ebb18-0x800ec53c, rodata 0x80170f08-0x80170f40, data 0x8018e038-0x8018e050) |
| stream.c | game2 | 19 of 23; stream_select_wave 86%, stream_debug_display 96%, stream_jump 60%, stream_fill 52% |
| main.c | game2 | 2 of 2 (main_proc and utime_proc) |
| movie.c | game2 | 24 of 26 at 100% (every decoder, the draw pass, the loader set-up); loadfram_proc 74% (only the s0/s1/s2 assignment differs), decode_movie_rgb16 44% (an unused decoder, register names) |
| script.c | game2 | 10 of 10, registered in modules.txt (text 0x80110270-0x80110b04, rodata 0x80173318-0x8017345c) |
| shake.c | game2 | 8 of 8 (update_movie_scroll, request_scroll and the explosion sprite object 0x800f36b0-0x800f38c4: add_explosion, its draw, think, init, remove and place); the start of its file, 0x800f3420-0x800f3580, is not decompiled |
| attract.c | game2 | 18 of 18, registered in modules.txt (text 0x800caf80-0x800cbb6c, rodata 0x8016f600-0x8016f704, data 0x80179c50-0x80179d40) |
| scene.c | game2 | 10 of 10 (the switch sampler, the debug nudges, the heap display, init_game_state, game_init, load_level_assets); the rest of that file (0x800c8280-0x800ca4d0: screen markers, 3D helpers) is still assembly |
| spawn.c | game2 | 11 of 12 exact; add_die 38% (its failure return is laid out differently); the port runs spawn_wave_enemies with construct_and_spawn_object (0x800e9570, 3.8 KB) stubbed |
| objects.c | game2 | the list routines, draw_pass and think_pass exact; the shot tests (shot_hits_sphere 43%, find_submesh_hit 7%), the animation blends (start_hit_reaction exact, the rest 15-33%) and shoot_objects (5%) are transcriptions the port runs |
| wave.c | game2 | 19 of 21 exact (unload_wave, preload_wave, the bank lists, the per-level asset lists, the big top's act change); load_wave_proc 26% and btopload_proc 8% keep an address in a saved register that ours does not. The 25% transcription had put the music stream start outside the `loaded_wave & WAVE_LEVEL` test, so the attract and the gallery streamed the haunted house partition under their own cues (fixed 2026-10-13: only the four levels stream) |
| main.c (game) | game2 | init_main_game_proc 61%, print_select_countdown 63%, main_game_proc 7% (first transcription; the flow is right) |
| player.c | game2 | set_level_start_time, hurt_player (the damage, the hurt cues, the hit marks) exact, init_player_stats 92%, audit_level_time 92%, kill_player 90%, draw_player_status 81% (the score, pickup messages, the continue count and game over, the credit line), draw_hud 65%, init_player 25% (store order); the health bars and ammo gauges (draw_health_bar, draw_ammo_gauge and their mirrored forms, 20-35%) draw the right picture but their vertex stores are ordered differently. The whole file is 0x800e2958-0x800e4eac |
| shoot.c | game2 | 6 of 14 exact (set_player_gun, the accuracy flags, reload_gun, set_weapon, give_pistol); read_guns 13%, gun_trigger 10%, resolve_shot 14%, gun_pump 4%, use_ammo 36% (first transcriptions; the whole light-gun path runs in the port) |
| camera.c | game2 | 17 of 23 exact (update_camera, update_view_matrix, get_camera_pos, the file loader's callers, the gun pollers, kss_reset_w_calib_, take_calibration_request, default_calibration ...); set_field_of_view 67% and init_camera 12% / load_level_camera 5% (the original combines the field-of-view constants at run time where our compiler folds them: 67.74+10 is an `add.s` in the game), kss_reset_ 59%, gun_board_reset 89% (one scheduling swap), the two `&&`/`\|\|` gun tests 53-60% (register choice). Absorbs the old gun.c: the file at 0x800d4070-0x800d4f6c holds the camera and the light guns |
| vecmat.c | game2 | 23 of 23 exact (0x800cfb18-0x800d1370: 3x3 matrices, the twelve rotation-order products, fatan, ftan) plus screen_to_ray and screen_to_point exact; world_to_view and view_to_world are transcriptions (1-2%) |
| loaders.c | game2 | 3 of 4: remove_animation, remove_model and remove_path exact (until 2026-10-14 they were no-op stubs, so nothing a level loaded was ever freed); load_camera_file 4% (the retry loop's two counters are allocated differently; the port uses it as is) |
| model.c | game2 | init_groups exact; draw_model and the eight face drawers are first transcriptions (0-31%); the port draws every enemy through them. draw_submesh_at (0x800e06d8, one submesh at a given matrix: the held weapons) 19%, the register choice for the projection buffers |
| lights.c, fog.c | game0 (-O0) / game2 | lights.c 12 of 23 exact (the list operations, the ambient override, the fog switch); load_level_lights, add_light_group, update_lights, build_light_list and light_vertices are behaviourally complete transcriptions whose -O0 stack layouts do not match yet. The light tables are generated into lights_data.c by tools/extract/extract_lights.py. fog.c 4 of 4 |
| krinkle.c | game2 | load_krinkle_texmap, remove_krinkle_assets, krinkle_remove, krinkle_drop_arm_parts, krinkle_drop_head_parts exact; krinkle_steer 95%, load_krinkle 86%, krinkle_hit 76%, krinkle_setup 54%, krinkle_think 50%, krinkle_init 50%, krinkle_draw 31%, krinkle_end_path 26%, krinkle_die 2% (one saved register off). He walks, strikes, reacts to hits and dies in the port |
| effects.c | game2 | 4 of 5 exact (the effect list on an enemy: attach, draw, think, free); detach_effect 33% |
| rickity_script.c | game2 | 38 of 38 exact (0x80123f88-0x80124c68: the script entry, the 22 section gates the movie script calls between its replays, the unused timers and counters). Unregistered: its timers are interleaved with the other levels' in .bss |
| term.c | game2 | 7 of 12 exact (the loaders, term_remove, term_draw and term_hit after the debug-label and store-order fixes), term_die 94%, term_end_path 96%, term_steer 94%, term_init 59%, term_think 17%, term_setup 15% (case-tree shapes); the port runs all of it: the bugs follow their path and reach the screen |
| dino.c | game2 | the dinosaur (0x2040, 0x80124c68-0x80126410): 13 of 14 exact, dino_die 81% (an s1/s2 swap). Limbs shoot off at violence 3 |
| rickity_innocent.c | game2 | the Rickety Town innocent, a little girl (0x2043, "rinn", 0x801293b8-0x80129d54): 11 of 11 exact. Shooting her costs 350 health and shows the penalty notice; her holder's let-go routine (0x800fa1c4) is the Haunted House zombie's and still a stub |
| smilingbob.c | game2 | Smiling Bob (0x2044, 0x80129d58-0x8012b7d4): 12 of 14 exact, smilingbob_die and smilingbob_steer 55% |
| teen.c | game2 | the teens (0x2045, 0x8012b7d8-0x8012d4a0): 14 of 15 exact, teen_die 73%. They throw the food, presents and coal (weapons.c) |
| krampus.c | game2 | Krampus, the level's boss (0x2046, 0x8012d4a0-0x8012fa20, with his 15 level-script callbacks): 26 of 28 exact, krampus_init 97%, krampus_think 99% |
| pickup_*.c | game2 | the six pickups (shotgun, health, ammo, acid gun, machine gun, flame thrower; 0x801026b0-0x80105388, tables in pickups_data.c): 53 of 54 exact, draw_flamethrower_pickup 34% |
| enemy.c | game2 | 9 of 14 exact (health by adjustment, the path helpers, the reach test, set_goal_near_camera); step_toward_goal and _xz 50%, move_cd 16% (the per-level collision tests are stubs), face_camera 3%; the thrown objects' helpers near_camera, group_point, aim_at_camera, aim_at_point, knock_sideways and lob_toward all exact |
| sprites.c | game2 | the screen and world sprite lists (0x800db148-0x800dbf58: muzzle flashes, hit marks, the dust of a miss, drops, sparks): the list resets exact, the creators 31-55%, the draw/update loops 58-64%, the reapers 10% (loop shape). The kind tables are in the source (0x8017d230, 0x8017d3b0) |
| draw2d.c | game2 | draw_quad and draw_quad_mirrored exact, the alpha quads 94%; the four billboards (0x800da0f0-0x800dac0c) are transcriptions (0-4%: the original keeps the corners in 16-byte slots and inlines everything); the five point sprites (0x800d9550-0x800d9eb4: a screen-aligned textured quad at a world point, sized by its distance; the flame and the explosion use them) 97-98%, draw_life_bar 23%, the two flat quads exact, the rotated quads 69% and 58% |
| texmaps.c | game2 | load_misc_texmaps exact, the per-level loaders 70-91% (the violence choice is `t = noblood; if(violence_level >= 2) t = blood;`); the 25 tables are generated into texmaps_data.c by tools/extract/extract_texmaps.py |
| effects.c | game2 | the whole effects file (0x800fd990-0x801007a0): 23 of 43 exact (the effect list, every init and free, burst_blood, the spray and flame set-up, add_snow_around, blow_snow 92%, spray_think 92%, draw_blood_drops); the other particle draw loops 28-74% and draw_smoke 4% (the two cross quads' vertex stores). The chunk model's tables are in the source. The host stands in for the flat depth triangle (0x800d2dd4); the flame draws through draw_flame_sprite (draw2d.c) |
| particle.c | game2 | draw_particle (0x800ecd20, one textured triangle at a world point, 30%); the rest of its file (0x800ec540-0x800ed030) is not decompiled |
| jester.c | game2 | the jester (0x801072e0-0x80109688): the loading-screen dancer, the level introductions and the enemy that shatters into 2530 shards. 21 of 31 exact (the loaders, the cues, the sounds, the name plate, the removal); init_jester 94%, jester_die 90%, jester_take_damage 89% (dead code in the original), jester_hit 81%; the switch dispatch trees of jester_think (5%) and set_jester_state (22%) and the shard loops (draw_jester_shards 1%, get_jester_shards 18%: the original strength-reduces the shard fields through a second, biased pointer) are the open problems. Tables generated into jester_data.c |
| select.c | game2 | the level-select gallery's targets (0x80101908-0x801026b0): the draw, the loaders, the init, the removal, the think and the light reset exact; select_target_hit 80% (the shared next_wave store's layout), aim_select_light 20% (the off paths' layout). Tables generated into select_data.c |
| weapons.c | game2 | the enemies' weapons, one original file (0x8010d818-0x80110270): the held objects (an effect in a hand group: attach_held_object, draw_held_object with its 0x44-way switch, all exact), the thrown objects (game object 0x1900: the five throwers, the draw, the hit, the die and the removal exact, the think 93%: the shot case's float register choice), the twelve loaders exact. The tables (texmaps and submesh tables for fifteen models, 0x80193ad0-0x80194650) are generated into weapons_data.c by tools/extract/extract_texmaps.py. Kinds and states in weapons.h |
| clip.c | game2 | the three edge interpolators exact; draw_world_tri (0x8010bf30, near-plane clip and projection) and clip_screen_tri (0x8010cd54, the screen edges, recursive) are complete transcriptions at 1-10%: the original assigns the vertex pointers to the saved registers in a different order and does not interleave the quarter-pixel snaps |

A module is registered in `config/modules.txt` once every function in its source file's
address range matches and its data is laid out in the file's order (the audit file was the
first: its text ends at a stray literal word at 0x800f1d84, and the adjustment table that
follows its tables in `.data` belongs to the next file). The main/credits file
(0x800c4f18-0x800c82xx) and the high-score file still contain undecompiled functions.
The movie player is now three source files like the original: movie.c (0x800e7188-0x800e9140,
blocked on loadfram_proc and decode_movie_rgb16), shake.c (0x800f3580, a file whose other
functions are not decompiled) and script.c (registered). The objects file is large
(0x800cbb70-0x800cd4ac and beyond: the per-frame draw pass, 3D object drawing, hit tests),
so objects.c stays unregistered until that code is decompiled.

Function names are ours and describe what the code does; where they replace a Binary
Ninja guess (`decompress_movie` -> `decode_movie_8pal`, `decoode_movie2` ->
`decode_movie_8pal_depth`, `show_debug_frame_data` -> `movie_draw_frame`,
`load_fram_wrapper` -> `movie_init`) the override lives in `config/names.txt` and the
original guess stays in `config/functions_v103.txt`.

## The light gun (`src/game/core/shoot.c`, `objects.c`, `port/runtime/hal.c`)

The original samples the guns from the I/O ASIC every vblank (`read_guns`): a 12-bit x and a
10-bit y per gun, and a bit saying the gun sees the screen. The calibration (eight CMOS
adjustments in triplicate, `cmos_calibration_`, factory values when the CMOS is blank) turns
them into a screen position, 1..399 by 1..255 with y counted from the top. `gun_vblank` runs
the trigger and pump edges: a pull fires at once when the gun sees the screen, otherwise it
waits with the screen held bright until it does (the off-screen reload); the draw pass then
resolves the shot (`resolve_shot`): `shoot_objects` builds the aim ray from the screen
position, reads the movie's depth under it for the hit point, runs every object's hit
callback and the hit object's die callback. The sphere and submesh tests extend the ray to
the target's depth and refuse a target the movie is nearer than.

In the port the mouse is player 1's gun: the window converts its position to game-screen
units (`video_to_game`), `port_gun_raw` folds it back through the player's calibration into
the raw readings, and the left button or left control is the trigger. Headless runs aim at
the screen centre, or at `CARNEVIL_AIM=x,y` (y from the bottom), and `--press N:trigger`
pulls the trigger. `CARNEVIL_TRACE=1` prints `[gun]`, `[shot]` and `[krinkle]` lines.

## Depth
The Voodoo w-buffers: a vertex carries 1/w and the nearer fragment (the larger 1/w) wins
under GR_CMP_LESS. The port keeps a GL depth buffer in the window mapping 0 = nearest,
1 = farthest: a vertex's z is `2 * min(1, oow) - 1`, which frame_setup's `glOrtho(.., -1, 1)`
(it negates z into clip space) lands in the window as `1 - oow`; the movie's 16-bit depth
plane is written straight into the depth buffer with the same `1 - oow` (port_movie_blit,
glDrawPixels), so the enemies are hidden by the scenery the way the hardware hid them. Until
2026-10-14 the vertex z was `1 - oow` before the negation, so the farther of two models won
the depth test: a Krinkle on the balcony drew over the one in front of the camera.

The movie's depth words are the Voodoo's own w-buffer values, and the hardware compares them
as integers with the word it computes from each pixel's 1/w (MAME's voodoo emulation: 1/w in
16.32 fixed point, a 4-bit count of leading zeros and the 12 inverted bits after the leading
one, plus 1). That is linear in 1/w within each exponent. The port first decoded the words
with the curve of the game's depth_word_to_float (scene.c, the gun's shot test), which reads
them as a distance linear within each exponent; the two agree only at the ends of an exponent,
so the scenery sat up to 12% farther than on the cabinet and about 7% of the enemy vertices
the hardware hides showed through (corn dog throwers in front of what should cover them, feet
through the floor). Since 2026-10-14 `build_depth_table` stores, for each word, 1 minus the
smallest 1/w that encodes to it, which makes GL's LEQUAL give the hardware's answer to within
four words (about 0.1% of the distance; the 24-bit GL buffer cannot resolve finer).

## In-place vector helpers
The game calls `mat_vec_mul(v, m, v)` in place (project_vertices, angles_to, the
billboards). The MIPS routine loads v before it stores, so that is safe on the hardware; the
host build needs the same order written in the C (locals), or every drawn vertex lands away
from where the hit tests put the enemy. See docs/toolchain.md.

## Lighting and the colour combine

Every enemy draw builds its own light list (`build_light_list`, 0x800d8198): each live point
light within ten times its table range becomes a direction scaled by `1 - d^2/range^2`, each
directional light is taken as it is, and `light_vertices` shades every normal from a base of 5
plus the positive cosines, clamped to 254. The live lights come from the level's table
(lights_data.c): `update_lights` switches each group on at its start frame and off at its end
frame, steps the colour fades and counts timed lights down, and one group may be the camera
light that follows the view point. With no lights an enemy is nearly black, which is what the
port showed before this was decompiled.

The drawers hand the renderer the game's own vertex layouts (`port_draw_tri` in render_gl.c:
x, y, 1/w, optional s/w, t/w, then the iterated colour and alpha where the layout keeps them),
and the iterated-colour combine modes now feed GL per-vertex colours. `grColorCombine` maps
the Glide tuple back to the mode the state keeps; `draw_faces_tex` (group flag 0x10) asks for
the inverted output, which GL gets as `INTERPOLATE(1 - colour, white, texture)`.

## Screen origin and texture orientation

GOOSE opens the Voodoo with `grSstOpen(2, 9, 0, 1, 1, 2)`: origin 1 is `GR_ORIGIN_LOWER_LEFT`,
so screen y grows upward and Glide's t = 0 is the first row stored in a WMS file. The port
mirrors this with a bottom-up `glOrtho` and uploads textures unflipped. (An earlier version
flipped the rows and used an upper-left origin; full-screen quads looked right either way, but
the font glyph tables index rows from the file top and came out blank.)

## Text

`src/game/render/text.c` is the game's own glyph renderer (it never uses GOOSE fonts/ostrings):
three glyph tables for FONT0001.WMS, `draw_string`, `draw_string_color`, `draw_string_at`,
`draw_string_centered`, `string_width`. The attract "play mode" screen (`do_play_mode` in
attract.c) draws the LEGAL1 texmap plus the credit lines with it. The per-frame buffer clear
comes from the game's draw pass (`draw_pass` in objects.c, 0x800cbbb4: the movie frame via
movie_draw_frame, then the level's 3D, every game object's draw function, the overlays and
the debug displays); the port runs that pass with the level routines stubbed.

## Movies (`src/game/movie/movie.c`, `port/runtime/disk.c`)

The attract sequences, the intro and ending, and every level's background are "movies":
raw TRAP partitions of fixed-size frames (0xd0, 0xc9, 0x12e or 0x135 sectors). `movie_load(id)`
picks the partition, the frame decoder and a script; the `loadfram` process reads one frame
per tick with the BIOS `_SecReads` into three rotating disk buffers; `movie_draw_frame`
(first thing in the draw pass) decodes the oldest one into a 400x256 buffer of LFB words
(RGB555 low half, depth high half) and DMAs it into the Voodoo frame buffer with
`grLfbWriteMode(13)`; `do_script` walks the movie script (play-until, seek, goto, call,
hold, sound cues, end) to pick the next frame. Level movies carry a per-pixel depth plane so
3D objects sort against the background.

The port needs the whole disk image for this: `--raw IMAGE` (or `CARNEVIL_RAW`, default
`original/carn.raw`, which `setup.sh` links). `port/runtime/disk.c` parses the TRAP table and
serves `_SecReads` synchronously, firing the game's completion callback before returning;
`port_movie_blit` in render_gl.c draws the frame as a texture (row 0 is the bottom line, a
positive scroll pushes the picture up). Without an image the frames are black and the
sequence still runs. The attract scripts are in `movie_scripts.c` (generated from
0x801130b8-0x80113218); the level scripts (hhouse, freakshow, rickity, bigtop) are not
extracted yet.

## Sound (`port/audio/dcs_board.cpp`, `port/third_party/dcsdecoder`, `src/game/sound/soundio.c`)

CarnEvil's DCS2 board is RAM based: the game's own sound library (src/sound/sound.c, byte-exact)
resets it through the I/O ASIC sound control register, uploads the DCS operating system
(`F_OSYS.BIN`, `F_COMP.BIN`, `F_DM_EXT.BIN`, `F_DM_INT.BIN` from the partition) word by word
through the sound data port, builds a 4096-entry sound-call table in the board's DRAM, uploads
each `*.BNK` bank behind it, and then sends sound calls as three words (call, volume/pan,
priority). `port/audio/dcs_board.cpp` is the board: it answers the monitor and OS protocol
(checksums, 0x0A/0x0C acks, version 0x0223), keeps the DRAM image, runs the bank track programs
(DCSExplorer's byte code: play stream, stop, levels, loops...) and uses Michael Roberts'
native DCS decoder (vendored, BSD-3) as the codec. Output is mono 16-bit at 31250 Hz through
CoreAudio (`audio_macos.m`); `--wav FILE` instead renders in lock step with the frame clock
and writes the soundtrack of a headless run. The level music is a second path: raw stereo 16-bit little-endian PCM in the disk's raw
partitions (3, 5, 7, 9; the level waves select one in `stream_select_wave`), read in 32 KB
pieces into a 96 KB ring by the movie player's disk process and pushed 512 words at a time
into the I/O ASIC's sound FIFO from the ASIC's FIFO interrupt (`src/game/core/stream.c`,
0x8013f508-0x8014012c); the board plays the FIFO after command 0x55D6. The HAL raises that
interrupt between frames while the board's FIFO (a few frames deep here, 512 words on the
hardware) has room, and the port's `FIFO_WRITE` seam replaces the register store. The
attract mode never starts this stream (load_wave_proc only selects and starts it for the four
level waves; the gallery and the high-score entry run on bank cues); `CARNEVIL_STREAM_TEST=0x1040000`
starts the haunted house music under the attract so the path can be heard.

The board side was checked against the real sound OS: `tools/analysis/adsp21xx_dasm.py` (MAME's ADSP-2100
disassembler in Python) lists F_OSYS.BIN (PM 0) and F_COMP.BIN (PM 0x2800). The host command
dispatch is at PM 0x3633; 0x55D6/0x55D7 set and clear the stream-on word (DM 0x381C), 0x55D8
clears the 13 stream buffers (480 words each, states at DM 0x382E, free count 0x381B), 0x55E7
selects FIFO mode (DM 0x381D) and 0x55B3 stores the stream volume (DM 0x3818). The FIFO
interrupt (PM 0x34C3) copies 512 words from DM 0x0407 into the next free buffer; the per-frame
mixer (PM 0x3961) plays a full buffer whenever the stream is on, scaled by the stream volume
and the master gain, with no track involved, which is what `dcs_board.cpp` does.

Tracks: the board has six tracks (= mixer channels). The host's priority word picks the track
(goose/sound.h: 0x8000 forces the track in bits 7-9 and by default overrides what plays there,
0x4000 makes it check that track's priority first, 0x2000 keeps an equal priority from
interrupting, `SND_PRI_SET_NOINT` = 0xE000; otherwise the board allocates a free track; low
7 bits are the priority), and a
bank program's channel numbers are relative to that track (950 of the 953 programs in the
game's banks are written for "channel 1 / stream 0"). The game runs the attract and level
music on track 0 (0x8000/0x8010) and speech and effects on tracks 3-5 (next_cue_priority), so
a coin sound no longer replaces the intro music. A program is {u16 wait, op, args}... and the
op follows its wait: reading another wait after one expires ran the interpreter off the end
of the coin program into the neighbouring weapon sounds of generic1 (fixed 2026-10-12).

A lower priority value is the more important sound. The header does not say so, but the game
code does: every enemy sound is `prio_base - n` on the enemy's one forced track with
`SND_PRI_SET_NOINT`, with n = 0 for walk and taunt calls, 2 for hurt calls and 8 for the death
or removal call, and a Krinkle starts with a sound that loops forever at `prio_base + 2` that
only its death or removal call can end. The board first had the comparison the other way
round, so a Krinkle's loop held its track against its own hurt and death calls (no hurt
sounds), and the indefinite wait 0xFFFF that ends those programs counted as 65535 frames, so
the tracks stayed taken after the Krinkles were gone (silence after a few of them). Fixed
2026-10-14, together with two rules from the vendored decoder's reading of the firmware
(DCSDecoderNative::ExecTrack): a stream repeat count of 0 loops forever (the board had played
it once), and opcode 0x00 ends the track and stops the stream on its own channel (the board let
it run on, which made sounds linger). A hurt program plays its cry, waits the cry's length
and goes back to the Krinkle's loop.

`src/game/sound/soundio.c` has the game's sound start-up (main_init_sound) and the cue helpers; the
attract wave's banks (select1, midway, intro4, attract) are loaded by the port's preload_wave
stand-in until load_level_sounds is decompiled. The HAL now serves the BIOS timers in
nanoseconds, a word-per-byte CMOS at a real address (GOOSE CRCs it directly), findfirst with
file sizes, and defines a plain `_fopen` on macOS so the SDK-built objects hit the path mapping.
Adjustments come from GOOSE's factory defaults when the CMOS is blank (attract sounds on, volume
127); the game's own adjustment table and CMOS setup are not decompiled yet.

### Load time (a test calibration, not game code)

The jester's level introduction has no fixed length: it plays, then loops its last stretch
until `load_wave_proc` finishes loading. Most of that load is the sound library streaming the
banks to the board (`snd_download_bank_playing`): before each word it polls the board's
"realtime ready" status bit, and after every 8192 words it sleeps one frame. The port keeps
those sleeps exactly but answers "ready" at once and reads the disk image at once. On the
cabinet the DCS2 firmware on the sound DSP sets that bit between its own audio work, and the
IDE drive takes real time. Neither cost is in the game code and the port does not emulate the
DSP, so the real figure is unknown. With the sleeps alone a Rickety Town load lasts about 285
frames while the introduction needs about 690, so it was cut short.

As a test, `port/runtime/hal.c` (`load_time_word`) stretches every load made while sound plays
by `--load-time X` (default 2.8, a trial value; 690 / 285 = 2.4 is the bare minimum for the introduction): each bank word adds (X - 1) / 8192 of a
frame, waited out with `sleep()` so the game keeps running. The boot load of the attract banks
goes through the same routine and lengthens too (it ends at frame 955 instead of 391).
`--load-time 1` restores the instant hardware. The value should be replaced by a timing taken
from a real cabinet, or by emulating the DSP firmware.

## Inputs, coins and credits (`src/game/core/credits.c`, `port/runtime/hal.c`)

The cabinet switches reach the game exactly as on the board: GOOSE's `scan_switches` (called
from the game's own vertical-retrace handler, 0x800c6030, now in `game_stub.c`) reads the
I/O ASIC words through ioctl 'i'0 (dips), 'i'1 (coins, starts, service, test, volume, slam),
'i'2/'i'3 (player buttons), all active low, and calls the handlers `main_proc` installed.
`src/game/core/credits.c` is that module (0x800c6a64-0x800c8198): coin and service-credit
handlers, `add_coin` with the per-slot units, bonus credits and the max-credit clamp, the
credit counter process ("ccproc", meter pulses through ioctl 'i'5), the slam lockout, the
volume buttons and bar, `init_credits` (free play from dip or adjustment, pricing from the
continent/country/price adjustments, the custom table or the dip switches; the 80 pricing
records, continent tables, dip table and currency names are generated into
`src/game/core/pricing_data.c` by `tools/extract/gen_pricing.py`), `credit_info` (the "CREDITS: n" /
"INSERT COIN" / "PRESS START" lines) and `start_proc`, which takes the credits and starts a
player (the game proper, `init_player_stats` / `init_main_game_proc`, is still a stub that
prints "GAME START requested"). Credits live in audit 10 in the CMOS (`cmos.bin` in the
working directory, now loaded before GOOSE's first direct access, so adjustments survive a
restart), as on the cabinet: credits you insert are still there on the next run.

Keyboard (window build): `5` or `c` coin, `1`/`2` start, `9` service credit, `F2` test (stub),
`-`/`=` volume down/up (hold: the bar and "VOLUME n" appear), space pump, left ctrl or the
mouse button trigger. Scripted presses for headless tests:
`--press 1000:coin,1010:coin,1050:start1` (frame:name, each held 6 frames; names in
`name_to_bits` in hal.c). The light-gun port (0x16800000, kseg1 0xb6800000, MAME's
`carnevil_gun_r` layout) is a mapped page the HAL refreshes every frame with the trigger and
pump bits and a centred position; aiming comes later.

## Boot sequence (`src/game/core/main.c`, `hwinfo.c`, `adjust.c`, `hiscore.c`, `gun.c`)

The port now runs the game's real main process (0x800c50ac). In order: game init, the
security PIC's serial number through the BIOS `pic:` device (the HAL answers like MAME's
simulated PIC: serial 123456, game number 486, made 12/11/98, since the PIC is not dumped),
the hardware info screen as a draw object, the CMOS layout (`setup_cmos` writes `audits.fmt`
and configures GOOSE's CMOS), the audit groups and the game's adjustments 0x30-0x3d (factory
restore when the CMOS is blank, which also resets the high scores and copies the default
high-score photos HSTDDFnn.WMS over HSTDnn.WMS), high scores, pricing, sound, the switch
handlers, the level assets (stubs), the gun board reset (control byte at 0xb7f00000, a
mapped page) and finally the attract process, whose first `remove_all_objects` takes the
info screen down. So the boot shows "CARNEVIL V1.0.3 / SERIAL NUMBER / MANUFACTURE DATE /
CMOS: OK / COINAGE: FROM DIPSWITCH / GUN: OK / DCS2: 0223" while the attract assets load,
as the cabinet does; the BIOS's own power-on screen comes before GAME.EXE and is not part
of the decompilation.

Everything the game writes (its CMOS image, `audits.fmt`, the high-score photos, screen
grabs) goes to the state directory (`--state DIR`, `CARNEVIL_STATE`, default `state/` in
the working directory), which is also searched before the partition on reads, so the
extracted partition is never modified. Delete it for a factory-fresh boot.
