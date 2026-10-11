# The host port

`make port` builds `build/host/carnevil`: the GOOSE library, the DCS sound library and the
game modules from `src/`, the same files the byte-exact MIPS build uses, compiled natively and
linked with the host layer in `port/host/`.

```
make port
build/host/carnevil --partition original/partition [--frames N] [--fast] [-v]
```
`--partition` is the directory with the extracted LFN32 partition (what `setup.sh` links as
`original/partition`; `CARNEVIL_PARTITION` works too). `--frames N` stops after N display
frames, `--fast` disables frame pacing, `-v` logs every ioctl and file open.

## How the original code runs on a PC

**One source, two targets.** `src/` is ILP32 C for the SN GCC 2.7.2; under `-DPORT_HOST`
(`include/types.h`, `port/host/compat.h`) it compiles with clang. Nothing in `src/` is
rewritten for the port; host-only paths are `#ifdef PORT_HOST` blocks (initg's `$gp` stub,
lockup's BIOS vector install, switch's bus-timing spin).

**Pointers are ints.** The SDK passes pointers through `int` (`_ioctl(4, req, (int)&val)`, the
process-stack marker, `(int)verts & 0x1c`, process arguments). That is only correct if every
address the game sees fits in 32 bits, so `port/host/mem.c` replaces malloc/free/calloc/
realloc with an allocator on an arena mapped at 0x20000000, and every stack (the GOOSE main
loop's in `main.c`, each process' in `sched.c`) is carved from it. The arm64 macOS kernel
refuses executables that map below 4 GB, so on macOS the port is built as x86_64 and runs
under Rosetta (the trick Wine uses); on Linux it is native with `-no-pie`. libc fortification
trips on these idioms, so it is off (`PORT_FORTIFY=2` to turn it on while debugging).

**Scheduler.** `port/host/sched.c` is mthread.c's `process_dispatch()` with the hand-written
MIPS register switching replaced by ucontext: each process node gets a host stack and context
the first time the dispatcher resumes it (create_process() leaves `_proc_shell` as a marker in
the new stack, exactly as the MIPS code expects), `sleep()` ORs the tick count and yields,
`die()` kills and switches back. process.c, the lists and the frame loop order (gamma table,
vblank wait, DMA kick, pre-proc, processes, post-proc, draws, sort) are the original.

**Hardware.** `port/host/hal.c`: the extracted partition is the disk (paths resolve
case-insensitively, `pci:` is a fake fd), fd 3 is the CMOS (a `cmos.bin` file in the working
directory), ioctl devices 4 (I/O ASIC: switches, sound port) and 5 (timers) answer with idle
values for now, interrupt handlers are recorded and the vblank one is called once per display
refresh (54 Hz) by `hal_wait_frame()`. `port/host/glide_stub.c` stubs 3dfx Glide; the real
renderer and `.3df` texture loader go there. `port/host/game_stub.c` stands in for the game's
own functions GOOSE main() needs and supplies a placeholder `main_proc` until the game code is
decompiled.

## Status
The port boots through GOOSE main() (banner, DMA buffers, switch/sprite init, graphics init,
handler installation), creates the main process and runs the dispatcher at the display rate;
processes are created, sleep and die correctly. Nothing is drawn or heard yet: textures,
rendering, sound and the game's `main_proc` are the next milestones, in that order with the
game-code decompilation.
