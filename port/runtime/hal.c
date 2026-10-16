/*
 * hal.c - host stand-ins for the Seattle BIOS and board: files on the extracted partition,
 * the CMOS device (fd 3), ioctl devices (4 = I/O ASIC, 5 = timers), interrupt handler
 * installation and frame pacing. Everything here is a first approximation that lets the
 * GOOSE library and the game run; each device gets fleshed out as the port needs it.
 */
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#endif
#include "hal.h"
#include "dcs_board.h"
#include "audio.h"
void port_texture_verbose(int);

const char	*hal_partition_root = 0;		/* --partition; otherwise chosen by choose_partition() */
const char	*hal_state_root = "state";		/* CMOS image and the files the game writes */
unsigned char	*hal_gun_control;			/* the gun board control byte at 0xb7f00000 */
signed char	nss_control;				/* the same byte as the variable camera.c's kss_ routines use */
/* cabinet switches: the I/O ASIC words are active low; keys and --press set bits here */
static unsigned int	coin_held;		/* bits of the coin/start word currently closed */
static unsigned int	player_held;		/* bits of the player 1/2 word currently closed */
static unsigned int	gun_held;		/* bit 0/1 = P1 trigger/pump, 4/5 = P2 */
static unsigned int	dipswitches = 0xffff;	/* all off */
unsigned char	*hal_gun_regs;			/* the light-gun port at 0xb6800000 (0x16800000 kseg1) */
struct press { unsigned long at; unsigned int coin, player, gun; int len; };
static struct press	presses[64];
static int		npresses;
static void		parse_presses(const char *spec);
static void		gun_update(void);
static float	mouse_gx = 200.0f, mouse_gy = 128.0f;	/* CARNEVIL_AIM=x,y fixes the aim (tests) */
static int	aim_fixed, mouse_seen;
/* tests: CARNEVIL_AIM is "x,y" (a fixed aim) or a schedule "FRAME:x,y;FRAME:x,y;..." (the aim
   moves to each point at its frame) */
#define MAX_AIMS	32
static struct { unsigned long frame; float x, y; } aims[MAX_AIMS];
static int	num_aims;
static void padsim_parse(const char *spec);
static void aim_parse(const char *s)
{
	unsigned long	f;
	float		x, y;

	aim_fixed = 1; mouse_seen = 1;
	if(!strchr(s, ':')) { sscanf(s, "%f,%f", &mouse_gx, &mouse_gy); return; }
	while(*s && num_aims < MAX_AIMS && sscanf(s, "%lu:%f,%f", &f, &x, &y) == 3)
	{
		aims[num_aims].frame = f; aims[num_aims].x = x; aims[num_aims].y = y; num_aims++;
		s = strchr(s, ';');
		if(!s) break;
		s++;
	}
}
static void aim_update(void)
{
	int	i;
	extern unsigned long hal_frame_count;

	for(i = 0; i < num_aims; i++)
		if(hal_frame_count >= aims[i].frame) { mouse_gx = aims[i].x; mouse_gy = aims[i].y; }
}
static unsigned int	sound_control = 1, main_control;
unsigned		dcs_fifo_space(void);
void			dcs_fifo_reset(void);
static int		fifo_int_pending(void);
const char	*hal_raw_image = 0;
int		hal_frame_limit = 0;
int		hal_fast = 0;
int		hal_headless = 0;
double		hal_load_time = 2.8;	/* --load-time: see load_time_word() */
void render_set_dump(const char *, unsigned long);
void audio_wav_close(void);
unsigned long	hal_frame_count = 0;
static int	refresh_hz = 54;			/* 400x256 54Hz, the game's normal mode */
static int	(*int_handler[8])(void);
static void	(*sys_handler[8])(void);
static int	verbose = 0;
static const char *wav_path;
int		port_trace = 0;

/* ---- stubs: every stand-in for game code the port lacks reports the first time it is
   reached, and the run ends with the list, so a missing function is never a silent no-op ---- */
static const char *stubs_seen[512];
static int	nstubs_seen;
static void stubs_report(void)
{
	int	i;
	if(!nstubs_seen) return;
	fprintf(stderr, "[stub] %d stubbed functions were reached this run:", nstubs_seen);
	for(i = 0; i < nstubs_seen; i++) fprintf(stderr, "%s %s", i ? "," : "", stubs_seen[i]);
	fprintf(stderr, "\n");
}
void stub_reached(const char *name)
{
	if(!nstubs_seen) atexit(stubs_report);
	if(nstubs_seen < 512) stubs_seen[nstubs_seen++] = name;
	fprintf(stderr, "[stub] %s reached at vblank %lu (not in C yet: a no-op in the port)\n", name, hal_frame_count);
}

/* ---- data the library expects the surrounding image to define ---- */
int		gt_fd;
int		hwconfig[64];
float		hres, vres;
unsigned char	*dma_buffer1, *dma_buffer2;
int		free_mem_left;
int		D_801de8ec;
unsigned int	D_801a37b0[20] = { 0x19, 0x27c, 5, 0x1b2, 0x5a, 0x23, 400, 256, 0x60, 0, 0x100, 0x100, 8, 0x17, 1, 1 };

#define CMOS_FD		3
/* the C library calls the HAL stands in for: defined under their own names on macOS and Linux
   (the executable's definition wins), as __wrap_NAME on Windows (the link's --wrap=NAME) */
#ifdef _WIN32
#define HAL_FN(name)	__wrap_##name
#else
#define HAL_FN(name)	name
#endif
/* a path the port itself built (absolute): passed to the host untouched */
static int is_host_path(const char *p)
{
#ifdef _WIN32
	if(p[0] == '\\' || (p[0] && p[1] == ':')) return 1;
#endif
	return p[0] == '/';
}
/* a page at a fixed address the game reads as hardware (the light-gun ports) */
static void *map_fixed_page(unsigned long addr)
{
#ifdef _WIN32
	void *p = VirtualAlloc((void *)addr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	return p ? p : (void *)-1;
#else
#ifdef MAP_FIXED_NOREPLACE			/* never over something already mapped there */
	return mmap((void *)addr, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED_NOREPLACE, -1, 0);
#else
	void *p = mmap((void *)addr, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if(p != MAP_FAILED && (unsigned long)p != addr) { munmap(p, 0x1000); return MAP_FAILED; }
	return p;
#endif
#endif
}
static void resolve(void);

/* Where the game's files come from: --partition (or CARNEVIL_PARTITION) if given; else the
   repository's original/partition (setup.sh); else the disk image itself: its file system
   (partition 1) is unpacked once into <state>/partition by lfn32.c, so a player only needs the
   raw image and no Python. The movies and music are read from the image as raw sectors anyway. */
extern int lfn32_extract(const char *image, int index, const char *outdir);

/* --adjust N=V and --violence V: operator adjustments set the way the test menu would, through
   GOOSE's set_adjustment into the CMOS image (so they persist in the state directory), just
   before the game reads them (main_proc -> load_game_adjustments). N is the number in the game's
   ADJUST.FMT (52 VIOLENCE: 0 none, 1 green blood, 2 red blood, 3 full gore; 58 BIG BABY; ...). */
#define MAX_ADJUST	16
static struct { int num, val; } adjust_req[MAX_ADJUST];
static int	num_adjust;
static void add_adjust(int num, int val)
{
	if(num_adjust < MAX_ADJUST) { adjust_req[num_adjust].num = num; adjust_req[num_adjust].val = val; num_adjust++; }
}
void port_apply_adjustments(void)
{
	extern int set_adjustment(int, int);
	int	i;
	for(i = 0; i < num_adjust; i++)
	{
		int r = set_adjustment(adjust_req[i].num, adjust_req[i].val);
		fprintf(stderr, "[hal] adjustment %d = %d%s\n", adjust_req[i].num, adjust_req[i].val, r ? " (refused)" : "");
	}
}
static void make_absolute(const char *path, char *out, size_t n)
{
	char	cwd[1024];
	if(is_host_path(path) || !getcwd(cwd, sizeof cwd)) snprintf(out, n, "%s", path);
	else snprintf(out, n, "%s/%s", cwd, path);
}
static void choose_partition(void)
{
	static char	dir[1024], exe[1100], raw[1024];
	int		files;

	if(hal_partition_root) return;
	if(access("original/partition/GAME.EXE", F_OK) == 0) { hal_partition_root = "original/partition"; return; }
	if(!hal_raw_image) hal_raw_image = "original/carn.raw";
	snprintf(dir, sizeof dir, "%s/partition", hal_state_root);
	snprintf(exe, sizeof exe, "%s/GAME.EXE", dir);
	hal_partition_root = dir;
	if(access(exe, F_OK) == 0) return;
	if(access(hal_raw_image, R_OK) != 0)
	{
		fprintf(stderr, "[hal] no game files: give --raw IMAGE (your CarnEvil v1.0.3 disk image, chdman extractraw) or --partition DIR\n");
		exit(1);
	}
	mkdir(hal_state_root, 0755);
	make_absolute(hal_raw_image, raw, sizeof raw);
	{
		char	adir[1024];
		make_absolute(dir, adir, sizeof adir);
		fprintf(stderr, "[hal] unpacking the game's files from %s into %s (once) ...\n", hal_raw_image, dir);
		files = lfn32_extract(raw, 1, adir);
	}
	if(files <= 0 || access(exe, F_OK) != 0)
	{
		fprintf(stderr, "[hal] %s does not hold CarnEvil's file system (partition 1)\n", hal_raw_image);
		exit(1);
	}
	fprintf(stderr, "[hal] %d files unpacked\n", files);
}
static int (*real_open)(const char *, int, ...);
static int (*real_close)(int);
void hal_init(int argc, char **argv)
{
	int	i;
	const char *env = getenv("CARNEVIL_PARTITION");
	int	nul;
	if(env) hal_partition_root = env;
	if(getenv("CARNEVIL_RAW")) hal_raw_image = getenv("CARNEVIL_RAW");
	if(getenv("CARNEVIL_STATE")) hal_state_root = getenv("CARNEVIL_STATE");
	if(getenv("CARNEVIL_TRACE")) port_trace = 1;
	if(getenv("CARNEVIL_AIM")) aim_parse(getenv("CARNEVIL_AIM"));
	if(getenv("CARNEVIL_PADSIM")) padsim_parse(getenv("CARNEVIL_PADSIM"));
	/* fd 3 is the BIOS CMOS device: park /dev/null there so no real file ever gets it */
	resolve();
#ifdef _WIN32
	nul = real_open("NUL", O_RDONLY, 0);
#else
	nul = real_open("/dev/null", O_RDONLY, 0);
#endif
	if(nul >= 0 && nul != CMOS_FD) { dup2(nul, CMOS_FD); real_close(nul); }
	for(i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "--partition") && i + 1 < argc) hal_partition_root = argv[++i];
		else if(!strcmp(argv[i], "--raw") && i + 1 < argc) hal_raw_image = argv[++i];
		else if(!strcmp(argv[i], "--frames") && i + 1 < argc) hal_frame_limit = atoi(argv[++i]);
		else if(!strcmp(argv[i], "--fast")) hal_fast = 1;
		else if(!strcmp(argv[i], "--crosshair") && i + 1 < argc) port_crosshair = atoi(argv[++i]);
		else if(!strcmp(argv[i], "--fullscreen")) { extern int video_fullscreen; video_fullscreen = 1; }
		else if(!strcmp(argv[i], "--borderless")) { extern int video_borderless; video_borderless = 1; }
		else if(!strcmp(argv[i], "--hires")) { extern int port_hires; port_hires = 1; }	/* window-resolution 24-bit drawing instead of the cabinet's 400x256 RGB565 */
		else if(!strcmp(argv[i], "--load-time") && i + 1 < argc) { hal_load_time = atof(argv[++i]); if(hal_load_time < 1.0) hal_load_time = 1.0; }
		else if(!strcmp(argv[i], "--headless")) hal_headless = 1;
		else if(!strcmp(argv[i], "--violence") && i + 1 < argc) add_adjust(52, atoi(argv[++i]));
		else if(!strcmp(argv[i], "--adjust") && i + 1 < argc) { int n, v; if(sscanf(argv[++i], "%d=%d", &n, &v) == 2) add_adjust(n, v); }
		else if(!strcmp(argv[i], "--dump") && i + 1 < argc) render_set_dump(argv[++i], 0);
		else if(!strcmp(argv[i], "--dump-every") && i + 2 < argc) { render_set_dump(argv[i+1], atoi(argv[i+2])); i += 2; }
		else if(!strcmp(argv[i], "--wav") && i + 1 < argc) wav_path = argv[++i];
		else if(!strcmp(argv[i], "--press") && i + 1 < argc) parse_presses(argv[++i]);
		else if(!strcmp(argv[i], "--state") && i + 1 < argc) hal_state_root = argv[++i];
		else if(!strcmp(argv[i], "-v")) { verbose++; port_texture_verbose(1); dcs_set_verbose(verbose); }
		else { fprintf(stderr, "usage: %s [--partition DIR] [--raw IMAGE] [--state DIR] [--wav FILE] [--press F:switch,...] [--frames N] [--fast] [--headless] [--fullscreen] [--borderless] [--hires] [--crosshair 0|1] [--load-time X] [--violence 0-3] [--adjust N=V] [--dump frame.ppm | --dump-every prefix N] [-v]\n", argv[0]); exit(2); }
	}
	choose_partition();
	hal_gun_regs = map_fixed_page(0xb6800000);
	if(hal_gun_regs == (void *)-1) { fprintf(stderr, "[hal] cannot map the gun port at 0xb6800000\n"); hal_gun_regs = 0; }
	else gun_update();
	hal_gun_control = map_fixed_page(0xb7f00000);
	if(hal_gun_control == (void *)-1) { fprintf(stderr, "[hal] cannot map the gun control byte at 0xb7f00000\n"); hal_gun_control = 0; }
	if(wav_path) { audio_wav_open(wav_path); atexit(audio_wav_close); }
	else if(!hal_headless && !audio_start()) fprintf(stderr, "[audio] no audio output\n");
}

int hal_refresh_hz(void) { return refresh_hz; }

#ifdef _WIN32
/* GOOSE's randper.c calls the C library's random() (31 random bits on DJGPP, BSD and glibc);
   the Windows runtime has none, so the port supplies an xorshift generator with that range */
static unsigned int random_state = 2463534242u;
long random(void)
{
	random_state ^= random_state << 13; random_state ^= random_state >> 17; random_state ^= random_state << 5;
	return (long)(random_state & 0x7fffffff);
}
void srandom(unsigned int seed) { random_state = seed ? seed : 2463534242u; }
/* strsep (BSD/glibc) for --press parsing */
char *strsep(char **sp, const char *delim)
{
	char	*s = *sp, *e;
	if(!s) return 0;
	e = strpbrk(s, delim);
	if(e) { *e = 0; *sp = e + 1; } else *sp = 0;
	return s;
}
#endif

/* ---- switches: switch ids follow goose/switch.h (coin word: 0 left coin, 1 right coin, 2 P1 start,
   3 slam, 4 test, 5 P2 start, 6 service credit, 7 center coin, 8 extra coin, 11/12 volume
   down/up, 14 coin door, 15 bill validator; player word: P1 A-D bits 4-7, P2 bits 12-15) ---- */
static int name_to_bits(const char *name, unsigned int *coin, unsigned int *player, unsigned int *gun)
{
	static const struct { const char *n; int word; unsigned int bit; } t[] = {
		{ "coin", 0, 1 << 0 }, { "coin2", 0, 1 << 1 }, { "start1", 0, 1 << 2 }, { "start2", 0, 1 << 5 },
		{ "slam", 0, 1 << 3 }, { "test", 0, 1 << 4 }, { "service", 0, 1 << 6 }, { "coin3", 0, 1 << 7 },
		{ "coin4", 0, 1 << 8 }, { "voldown", 0, 1 << 11 }, { "volup", 0, 1 << 12 }, { "bill", 0, 1 << 15 },
		{ "p1a", 1, 1 << 4 }, { "p1b", 1, 1 << 5 }, { "p1c", 1, 1 << 6 }, { "p1d", 1, 1 << 7 },
		{ "p2a", 1, 1 << 12 }, { "p2b", 1, 1 << 13 },
		{ "trigger", 2, 1 }, { "pump", 2, 2 }, { "trigger2", 2, 0x10 }, { "pump2", 2, 0x20 } };
	size_t i;
	*coin = *player = *gun = 0;
	for(i = 0; i < sizeof t / sizeof t[0]; i++)
		if(!strcmp(t[i].n, name)) { (t[i].word == 0 ? coin : t[i].word == 1 ? player : gun)[0] = t[i].bit; return 1; }
	return 0;
}
/* keyboard: 5 = coin, 1/2 = start, 9 = service credit, F2 = test, - and = volume, space = pump,
   left ctrl / mouse button = trigger (the gun itself comes later) */
void hal_key(const char *name, int down)
{
	unsigned int c, p, g;
	if(!name_to_bits(name, &c, &p, &g)) return;
	if(down) { coin_held |= c; player_held |= p; gun_held |= g; }
	else { coin_held &= ~c; player_held &= ~p; gun_held &= ~g; }
}
/* the mouse stands in for player 1's gun: its position in game-screen units (x 0..400 from the
   left, y 0..256 from the bottom, the Glide origin), set by the window; the raw gun values the
   game's reader expects are computed back through its calibration (port_gun_raw, game_stub.c) */
static int	mouse_on = 1;				/* headless runs shoot at the centre */
void port_gun_raw(int player, float sx, float sy, int *rx, int *ry);
/* Gamepads stand in for the light guns: the left stick moves each player's aim point across the
   picture (the light gun has no on-screen cursor, so the renderer draws a crosshair for a
   pad-aimed player), the buttons close the same switches as the keyboard. Pad 0 is player 1
   (sharing with the mouse: whichever moved last aims), pad 1 player 2. Reloading is what the
   cabinet does: the gun is pointed off the screen and fired; PAD_RELOAD does both for a moment. */
#define PAD_SPEED	7.0f			/* game pixels per frame at full tilt (the picture is 400 wide) */
#define PAD_DEADZONE	0.2f
#define PAD_RELOAD_FRAMES 8
static struct {
	float		x, y;			/* aim, game-screen units, y from the bottom */
	int		used;			/* this pad aims for its player (it moved or fired since the mouse did) */
	unsigned int	buttons, prev;
	int		reload;			/* frames left of the off-screen shot */
	float		lx, ly;
} pads[2] = { { 200.0f, 128.0f }, { 200.0f, 128.0f } };
static unsigned int	pad_coin, pad_gun;	/* switch bits the pads hold closed */
/* tests: CARNEVIL_PADSIM="FRAME:PAD:LX,LY,BUTTONS;..." sets a pad's stick and buttons (hal.h PAD_*
   bits, hex or decimal) from that vblank on, standing in for a controller */
static struct padsim { unsigned long at; int pad; float lx, ly; unsigned int b; } padsim[64];
static int	npadsim;
static void padsim_parse(const char *spec)
{
	char buf[2048], *s, *tok;
	snprintf(buf, sizeof buf, "%s", spec);
	for(s = buf; (tok = strsep(&s, ";")) && npadsim < 64; )
	{
		struct padsim *q = &padsim[npadsim];
		if(sscanf(tok, "%lu:%d:%f,%f,%i", &q->at, &q->pad, &q->lx, &q->ly, (int *)&q->b) == 5) npadsim++;
	}
}
void hal_mouse(float gx, float gy, int on)
{
	if(aim_fixed) return;
	if(gx != mouse_gx || gy != mouse_gy) pads[0].used = 0;	/* the mouse takes player 1 back */
	mouse_gx = gx; mouse_gy = gy; mouse_on = on; mouse_seen = 1;
}
void hal_pad(int pad, float lx, float ly, unsigned int buttons)
{
	if(pad < 0 || pad > 1) return;
	pads[pad].lx = lx; pads[pad].ly = ly; pads[pad].buttons = buttons;
}
static float pad_axis(float v)
{
	float a = v < 0 ? -v : v;
	if(a < PAD_DEADZONE) return 0.0f;
	a = (a - PAD_DEADZONE) / (1.0f - PAD_DEADZONE);
	return (v < 0 ? -a * a : a * a) * PAD_SPEED;		/* fine near the centre, fast at the edge */
}
static void pads_update(void)
{
	int	i;
	pad_coin = pad_gun = 0;
	for(i = 0; i < npadsim; i++)
		if(hal_frame_count == padsim[i].at) hal_pad(padsim[i].pad, padsim[i].lx, padsim[i].ly, padsim[i].b);
	for(i = 0; i < 2; i++)
	{
		unsigned int b = pads[i].buttons, pressed = b & ~pads[i].prev;
		float dx = pad_axis(pads[i].lx), dy = pad_axis(pads[i].ly);
		if(dx != 0.0f || dy != 0.0f || pressed) pads[i].used = 1;
		pads[i].x += dx; pads[i].y += dy;
		if(pads[i].x < 0.0f) pads[i].x = 0.0f; else if(pads[i].x > 400.0f) pads[i].x = 400.0f;
		if(pads[i].y < 0.0f) pads[i].y = 0.0f; else if(pads[i].y > 256.0f) pads[i].y = 256.0f;
		if(pressed & PAD_RELOAD) pads[i].reload = PAD_RELOAD_FRAMES;
		if(pads[i].reload > 0) pads[i].reload--;
		if(b & PAD_TRIGGER || pads[i].reload > PAD_RELOAD_FRAMES / 2) pad_gun |= i ? 0x10 : 0x01;
		if(b & PAD_PUMP) pad_gun |= i ? 0x20 : 0x02;
		if(b & PAD_START) pad_coin |= i ? (1 << 5) : (1 << 2);	/* start2 / start1 */
		if(b & PAD_COIN) pad_coin |= i ? (1 << 1) : (1 << 0);	/* right / left coin slot */
		pads[i].prev = b;
	}
}
int	port_crosshair = 1;		/* --crosshair 0 hides them (the cabinet had none; MAME draws them) */
int hal_crosshair(int player, float *gx, float *gy)
{
	if(!port_crosshair || player < 0 || player > 1) return 0;
	if(player == 0 && !pads[0].used)
	{
		if(!mouse_seen || !mouse_on) return 0;
		*gx = mouse_gx; *gy = mouse_gy;
		return 1;
	}
	if(!pads[player].used) return 0;
	*gx = pads[player].x; *gy = pads[player].y;
	return 1;
}
static void gun_update(void)
{
	int rx = 128 << 4, ry = 128 << 4, rx2 = 128 << 4, ry2 = 128 << 4;
	if(!hal_gun_regs) return;
	unsigned int held = gun_held | pad_gun;
	int on1 = pads[0].used ? pads[0].reload == 0 : mouse_on, on2 = pads[1].reload == 0;
	if(pads[0].used) port_gun_raw(0, pads[0].x, pads[0].y, &rx, &ry);
	else port_gun_raw(0, mouse_gx, mouse_gy, &rx, &ry);
	port_gun_raw(1, pads[1].x, pads[1].y, &rx2, &ry2);
	/* MAME carnevil_gun_r layout, one byte per 32-bit slot: X low, X high | buttons | 0x40 (sees the screen), Y low, Y high; P2 at +0x10 */
	hal_gun_regs[0x00] = rx & 0xff; hal_gun_regs[0x04] = ((rx >> 8) & 0xf) | ((held & 3) << 4) | (on1 ? 0x40 : 0);
	hal_gun_regs[0x08] = ry & 0xff; hal_gun_regs[0x0c] = (ry >> 8) & 3;
	hal_gun_regs[0x10] = rx2 & 0xff; hal_gun_regs[0x14] = ((rx2 >> 8) & 0xf) | (held & 0x30) | (on2 ? 0x40 : 0);
	hal_gun_regs[0x18] = ry2 & 0xff; hal_gun_regs[0x1c] = (ry2 >> 8) & 3;
}
static void presses_update(void)
{
	int i;
	for(i = 0; i < npresses; i++)
	{
		struct press *q = &presses[i];
		if(hal_frame_count == q->at) { coin_held |= q->coin; player_held |= q->player; gun_held |= q->gun; if(q->coin | q->player | q->gun) fprintf(stderr, "[hal] frame %lu: press %#x/%#x/%#x\n", hal_frame_count, q->coin, q->player, q->gun); }
		if(hal_frame_count == q->at + q->len) { coin_held &= ~q->coin; player_held &= ~q->player; gun_held &= ~q->gun; }
	}
	gun_update();
}
static void parse_presses(const char *spec)		/* --press 300:coin,320:coin,400:start1 */
{
	char buf[1024], *s, *tok;
	snprintf(buf, sizeof buf, "%s", spec);
	for(s = buf; (tok = strsep(&s, ",")); )
	{
		char *colon = strchr(tok, ':');
		struct press *q;
		if(!colon || npresses >= 64) continue;
		*colon = 0; q = &presses[npresses];
		q->at = strtoul(tok, 0, 10); q->len = 6;
		if(!name_to_bits(colon + 1, &q->coin, &q->player, &q->gun)) { fprintf(stderr, "[hal] unknown switch %s\n", colon + 1); continue; }
		npresses++;
	}
}

/* ---- frame pacing: one display refresh = one vblank interrupt ---- */
void hal_wait_frame(int frames)
{
	static struct timespec next;
	struct timespec now;
	long	period_ns = 1000000000L / refresh_hz;
	int	i;
	for(i = 0; i < frames; i++)
	{
		if(!hal_fast)
		{
			clock_gettime(CLOCK_MONOTONIC, &now);
			if(next.tv_sec == 0) next = now;
			next.tv_nsec += period_ns;
			while(next.tv_nsec >= 1000000000L) { next.tv_nsec -= 1000000000L; next.tv_sec++; }
			if(next.tv_sec > now.tv_sec || (next.tv_sec == now.tv_sec && next.tv_nsec > now.tv_nsec))
			{
				struct timespec d = { next.tv_sec - now.tv_sec, next.tv_nsec - now.tv_nsec };
				if(d.tv_nsec < 0) { d.tv_nsec += 1000000000L; d.tv_sec--; }
				nanosleep(&d, NULL);
			}
			else next = now;		/* fell behind: resynchronise */
		}
		hal_frame_count++;
		pads_update();
		presses_update();
		aim_update();
		audio_wav_frame();
		/* the I/O ASIC FIFO interrupt (handler 3): the game pushes 512 words per interrupt */
		for(int guard = 0; guard < 32 && int_handler[3] && fifo_int_pending(); guard++)
		{
			unsigned before = dcs_fifo_space();
			int_handler[3]();
			if(dcs_fifo_space() == before) break;
		}
		if(int_handler[0]) int_handler[0]();	/* vertical retrace */
		if(hal_frame_limit && hal_frame_count >= (unsigned long)hal_frame_limit)
		{
			fprintf(stderr, "[hal] frame limit %d reached\n", hal_frame_limit);
			exit(0);
		}
	}
}

/* ---- BIOS handler installation ---- */
void install_interrupt_handler(int n, int (*f)(void)) { if(n >= 0 && n < 8) int_handler[n] = f; }
void install_sys_handler(int n, void (*f)(void)) { if(n >= 0 && n < 8) sys_handler[n] = f; }
void install_fpu_handler(int n, void (*f)(int *)) { (void)n; (void)f; }
void install_int_div0_handler(void (*f)()) { (void)f; }
void install_int_divo_handler(void (*f)()) { (void)f; }
void install_sys_handlers(void) { }
void int_div0_handler(void) { }
void int_divo_handler(void) { }
void do_dma_timeout_reboot(void) { fprintf(stderr, "[hal] DMA timeout reboot requested\n"); exit(1); }
void fp_div0_reboot(void) { fprintf(stderr, "[hal] FP divide-by-zero reboot requested\n"); exit(1); }

/* ---- ioctl: device 4 = I/O ASIC (switches, sound port), device 5 = timers ---- */
#define FIOC(c, n)	(((c) << 8) | (n))
/* the ASIC raises its FIFO interrupt while the sound FIFO has room for a 512-word block */
static int fifo_int_pending(void) { return (main_control & 8) && (main_control & 1) && dcs_fifo_space() >= 512; }
static FILE *(*real_fopen)(const char *, const char *);
#define CMOS_WORDS	0x2000				/* 32 KB of battery RAM, one byte stored per word */
static unsigned int	cmos[CMOS_WORDS];
static long		cmos_pos;
static int		cmos_loaded;
static char		cmos_file[1024];
static void cmos_load(void)
{
	FILE *f;
	if(cmos_loaded) return;
	cmos_loaded = 1;
	mkdir(hal_state_root, 0755);
	snprintf(cmos_file, sizeof cmos_file, "%s/cmos.bin", hal_state_root);
	memset(cmos, 0xff, sizeof cmos);
	if((f = real_fopen(cmos_file, "rb"))) { if(fread(cmos, 1, sizeof cmos, f) != sizeof cmos) memset(cmos, 0xff, sizeof cmos); fclose(f); }
}
static void cmos_save(void)
{
	FILE *f = real_fopen(cmos_file, "wb");
	if(f) { fwrite(cmos, 1, sizeof cmos, f); fclose(f); }
}

/*
 * load_time_word() - PORT TIMING, NOT GAME CODE, AND NOT A MEASURED VALUE.
 *
 * When the game loads a level it sends each sound bank to the sound board one 16-bit word
 * at a time (snd_download_bank_playing in src/sound/sound.c). Before every word the game polls
 * the I/O ASIC sound status until the board's "realtime ready" bit is set, and after every
 * 8192 words it sleeps one frame. The port reproduces those sleeps exactly, but it answers
 * "ready" at once (dcs_status) and reads the disk image at once. On the cabinet the bit is set
 * by the DCS2 firmware running on the sound DSP, which empties the mailbox between its own
 * audio work, and the IDE drive takes real time too. Neither cost is in the game code, and the
 * port does not emulate the DSP, so the true cost is unknown.
 *
 * It matters because the jester's level introduction has no fixed length: it plays, then
 * loops its last stretch until load_wave_proc finishes. With only the sleeps a Rickety Town
 * load lasts ~285 frames, but the introduction needs ~690 to play through, which is how it is
 * remembered on the cabinet. As a TEST we stretch the load by --load-time (default 2.8, a
 * trial value; 690 / 285 = 2.4 is the bare minimum): each bank word sent during play adds
 * (load_time - 1) / 8192 of a frame, waited out with sleep() so the rest of the game keeps
 * running. This is a calibration guess to be replaced by a timing from real hardware or by
 * emulating the DSP firmware. --load-time 1 turns it off.
 */
static void load_time_word(void)
{
	extern int	sched_in_process(void);
	static double	owed;

	if(hal_load_time <= 1.0 || !sched_in_process()) return;
	owed += (hal_load_time - 1.0) / 8192.0;
	while(owed >= 1.0)
	{
		owed -= 1.0;
		sleep(1);
	}
}

int _ioctl(int dev, int req, int arg)
{
	int	*p = (int *)(intptr_t)arg;
	if(verbose > 1) fprintf(stderr, "[hal] ioctl(%d, %c%d, %#x)\n", dev, req >> 8, req & 0xff, arg);
	if(dev == 4)
	{
		switch(req)
		{
			case FIOC('i', 0): if(p) *p = dipswitches; return 0;
			case FIOC('i', 1): if(p) *p = 0xffff & ~(coin_held | pad_coin); return 0;		/* coins, starts, service, test, volume: active low */
			case FIOC('i', 2): if(p) *p = 0xffff & ~player_held; return 0;	/* player 1 and 2 buttons */
			case FIOC('i', 3): if(p) *p = 0xffff; return 0;			/* players 3 and 4: none */
			case FIOC('i', 4): if(p) *p = 0; return 0;			/* coin meters */
			case FIOC('i', 5): return 0;
			case FIOC('i', 6):						/* sound control: bit 0 low->high resets the board, bits 1-3 reset the FIFO */
				if((arg & 1) && !(sound_control & 1)) dcs_reset();
				if((arg & 0xe) && !(sound_control & 0xe)) dcs_fifo_reset();
				sound_control = arg; return 0;
			case FIOC('i', 7): if(p) *p = sound_control; return 0;
			case FIOC('i', 8):						/* host -> sound data */
				if(dcs_loading_while_playing()) { dcs_write(arg); load_time_word(); }
				else dcs_write(arg);
				return 0;
			case FIOC('i', 9): if(p) *p = dcs_status(); return 0;
			case FIOC('i', 10): if(p) *p = dcs_read(); return 0;
			case FIOC('i', 14): if(p) *p = fifo_int_pending() ? 9 : 0; return 0;	/* main status: FIFO interrupt */
			case FIOC('i', 15): main_control = arg; return 0;
			case FIOC('i', 16): if(p) *p = main_control; return 0;
			case FIOC('i', 17): dcs_clear_sth(); return 0;
			default: return 0;
		}
	}
	if(dev == 1001)					/* the security PIC, as MAME generates it: serial 123456, game 486, made 12/11/98 */
	{
		switch(req)
		{
		case FIOC('p', 0): if(p) *p = 123456; return 0;
		case FIOC('p', 1): if(p) *p = 486; return 0;
		case FIOC('p', 2): if(p) *p = (1998 - 1980) * 372 + (12 - 1) * 31 + 11; return 0;
		default: return -1;
		}
	}
	if(dev == 3)					/* CMOS: GOOSE works with int pointers into the word-per-byte RAM */
	{
		switch(req)
		{
		case FIOC('c', 0): cmos_load(); cmos_pos = (int)(arg - (int)(intptr_t)cmos) / 4; return 0;
		case FIOC('c', 1): cmos_load(); if(p) *p = (int)(intptr_t)cmos; return 0;	/* GOOSE CRCs the words directly */
		case FIOC('c', 2): if(p) *p = CMOS_WORDS; return 0;
		default: return 0;
		}
	}
	if(dev == 5)					/* GT-64010 timers: started, read back in nanoseconds, stopped */
	{
		static struct timespec start[4];
		int	n = (req & 0xff) & 3;
		struct timespec now;
		switch((req & 0xff) >> 2)
		{
		case 0: clock_gettime(CLOCK_MONOTONIC, &start[n]); return 0;
		case 1:
			clock_gettime(CLOCK_MONOTONIC, &now);
			if(p) *p = (int)((now.tv_sec - start[n].tv_sec) * 1000000000L + (now.tv_nsec - start[n].tv_nsec));
			return 0;
		default: return 0;
		}
	}
	return 0;
}
int ioctl(int dev, int req, int arg) { return _ioctl(dev, req, arg); }

/* ---- files: game paths resolve case-insensitively under the partition directory; files the
   game writes (audits.fmt, high-score photos, screen grabs) go to the state directory, which
   is also searched first, so the extracted partition is never modified ---- */
static void game_name(const char *path, char *name, size_t n)
{
	const char *p = path;
	if(p[1] == ':') p += 2;
	while(*p == '\\' || *p == '/') p++;
	snprintf(name, n, "%s", p);
	for(char *q = name; *q; q++) if(*q == '\\') *q = '/';
}
static void state_path(const char *path, char *out, size_t n)
{
	char	name[256];
	game_name(path, name, sizeof name);
	mkdir(hal_state_root, 0755);
	snprintf(out, n, "%s/%s", hal_state_root, name);
}
static int map_path(const char *path, char *out, size_t n)
{
	char	name[256];
	DIR	*d;
	struct dirent *e;
	game_name(path, name, sizeof name);
	snprintf(out, n, "%s/%s", hal_state_root, name);
	if(access(out, F_OK) == 0) return 1;
	snprintf(out, n, "%s/%s", hal_partition_root, name);
	if(access(out, F_OK) == 0) return 1;
	d = opendir(hal_partition_root);
	if(!d) return 0;
	while((e = readdir(d)))
	{
		if(!strcasecmp(e->d_name, name)) { snprintf(out, n, "%s/%s", hal_partition_root, e->d_name); closedir(d); return 1; }
	}
	closedir(d);
	return 0;
}

static ssize_t (*real_read)(int, void *, size_t);
static ssize_t (*real_write)(int, const void *, size_t);
static off_t (*real_lseek)(int, off_t, int);
#ifdef _WIN32
static off_t win_lseek(int fd, off_t off, int whence) { return (off_t)_lseeki64(fd, off, whence); }
#endif
static void resolve(void)
{
	if(real_open) return;
#ifdef _WIN32
	/* the CRT's own functions (compat.h renames only calls written open(...), not addresses) */
	{
		extern int __real_open(const char *, int, ...), __real_close(int);
		extern FILE *__real_fopen(const char *, const char *);
		real_open = __real_open; real_read = (ssize_t (*)(int, void *, size_t))_read;
		real_write = (ssize_t (*)(int, const void *, size_t))_write; real_lseek = win_lseek;
		real_close = __real_close; real_fopen = __real_fopen;
	}
#else
	real_open = dlsym(RTLD_NEXT, "open"); real_read = dlsym(RTLD_NEXT, "read");
	real_write = dlsym(RTLD_NEXT, "write"); real_lseek = dlsym(RTLD_NEXT, "lseek");
	real_close = dlsym(RTLD_NEXT, "close"); real_fopen = dlsym(RTLD_NEXT, "fopen");
#endif
}

int HAL_FN(open)(const char *path, int flags, ...)
{
	char	full[1024];
	resolve();
	if(!strcmp(path, "pci:")) return 1000;		/* the system controller device */
	if(!strcmp(path, "pic:")) return 1001;		/* the security PIC (serial number) */
	if(is_host_path(path)) { }			/* a host path from the port itself */
	else if(flags & (O_WRONLY | O_RDWR | O_CREAT)) state_path(path, full, sizeof full), path = full;
	else if(map_path(path, full, sizeof full)) path = full;
	if(verbose) fprintf(stderr, "[hal] open(%s)\n", path);
#ifdef _WIN32
	return real_open(path, flags | _O_BINARY, 0644);	/* never text mode (CR/LF translation) */
#else
	return real_open(path, flags & ~0x8000 /* O_BINARY */, 0644);
#endif
}
FILE *HAL_FN(fopen)(const char *path, const char *mode)
{
	char	full[1024];
	resolve();
	if(is_host_path(path)) { }			/* a host path from the port itself */
	else if(strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+')) state_path(path, full, sizeof full), path = full;
	else if(map_path(path, full, sizeof full)) path = full;
	if(verbose) fprintf(stderr, "[hal] fopen(%s, %s)\n", path, mode);
#ifdef _WIN32
	if(!strchr(mode, 'b'))					/* never text mode (CR/LF translation) */
	{
		static char	bmode[8];
		snprintf(bmode, sizeof bmode, "%sb", mode);
		mode = bmode;
	}
#endif
	return real_fopen(path, mode);
}
/* the game's remove() and rename() (DJGPP libc on the BIOS file system; the high-score file
   moves its photos down a place and deletes before rewriting): they act on the state directory,
   the writable layer over the partition. remove deletes the state copy; rename moves a state
   file, or copies a file that exists only in the partition into the state directory under the
   new name (the extracted partition is never modified). Host paths ("/...") pass through. */
static int (*real_remove)(const char *);
static int (*real_rename)(const char *, const char *);
int HAL_FN(remove)(const char *path)
{
	char	full[1024];

#ifdef _WIN32
	if(!real_remove) { extern int __real_remove(const char *); real_remove = __real_remove; }
#else
	if(!real_remove) real_remove = dlsym(RTLD_NEXT, "remove");
#endif
	if(is_host_path(path)) return real_remove(path);
	state_path(path, full, sizeof full);
	if(verbose) fprintf(stderr, "[hal] remove(%s)\n", full);
	return real_remove(full);
}
int HAL_FN(rename)(const char *from, const char *to)
{
	char	src[1024], dst[1024], buf[4096];
	FILE	*in, *out;
	size_t	n;

#ifdef _WIN32
	if(!real_rename) { extern int __real_rename(const char *, const char *); real_rename = __real_rename; }
#else
	if(!real_rename) real_rename = dlsym(RTLD_NEXT, "rename");
#endif
	if(is_host_path(from) || is_host_path(to)) return real_rename(from, to);
	resolve();
	state_path(to, dst, sizeof dst);
	if(!map_path(from, src, sizeof src)) return -1;
	if(verbose) fprintf(stderr, "[hal] rename(%s, %s)\n", src, dst);
	if(!strncmp(src, hal_state_root, strlen(hal_state_root)) && src[strlen(hal_state_root)] == '/') return real_rename(src, dst);
	in = real_fopen(src, "rb");
	if(!in) return -1;
	out = real_fopen(dst, "wb");
	if(!out) { fclose(in); return -1; }
	while((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
	fclose(in); fclose(out);
	return 0;
}
#ifdef __APPLE__
/* this file sees macOS's stdio.h, which renames fopen to fopen$DARWIN_EXTSN; the game and
   library objects are built against the SDK headers and call plain _fopen, so define that too */
FILE *fopen_plain(const char *path, const char *mode) __asm__("_fopen");
FILE *fopen_plain(const char *path, const char *mode) { return fopen(path, mode); }
#endif
ssize_t HAL_FN(read)(int fd, void *buf, size_t n)
{
	resolve();
	if(fd == CMOS_FD)					/* one CMOS byte per 32-bit word */
	{
		size_t i;
		cmos_load();
		if(cmos_pos + (long)n > CMOS_WORDS) n = CMOS_WORDS - cmos_pos;
		for(i = 0; i < n; i++) ((unsigned char *)buf)[i] = (unsigned char)cmos[cmos_pos + i];
		cmos_pos += n; return n;
	}
	return real_read(fd, buf, n);
}
ssize_t HAL_FN(write)(int fd, const void *buf, size_t n)
{
	resolve();
	if(fd == CMOS_FD)
	{
		size_t i;
		cmos_load();
		if(cmos_pos + (long)n > CMOS_WORDS) n = CMOS_WORDS - cmos_pos;
		for(i = 0; i < n; i++) cmos[cmos_pos + i] = ((const unsigned char *)buf)[i];
		cmos_pos += n; cmos_save(); return n;
	}
	return real_write(fd, buf, n);
}
off_t HAL_FN(lseek)(int fd, off_t off, int whence)
{
	resolve();
	if(fd == CMOS_FD)
	{
		if(whence == SEEK_SET) cmos_pos = off; else if(whence == SEEK_CUR) cmos_pos += off; else cmos_pos = CMOS_WORDS + off;
		return cmos_pos;
	}
	return real_lseek(fd, off, whence);
}
#ifdef _WIN32
/* with _FILE_OFFSET_BITS=64 MinGW's headers call lseek64/fopen64: the same HAL entries */
off_t __wrap_lseek64(int fd, off_t off, int whence) { return __wrap_lseek(fd, off, whence); }
FILE *__wrap_fopen64(const char *path, const char *mode) { return __wrap_fopen(path, mode); }
#endif
int HAL_FN(close)(int fd)
{
	resolve();
	if(fd == CMOS_FD || fd == 1000 || fd == 1001) return 0;
	return real_close(fd);
}

/* DJGPP findfirst(): the sound library uses it to check a file exists and to size it (ff_fsize
   sits at byte 26 of the SDK's packed struct ffblk, ff_name right after it) */
struct ffblk;
int findfirst(const char *path, struct ffblk *blk, int attr)
{
	char	full[1024];
	struct stat st;
	unsigned char *b = (unsigned char *)blk;
	(void)attr;
	if(!map_path(path, full, sizeof full) || stat(full, &st))
	{
		if(verbose) fprintf(stderr, "[hal] findfirst(%s): not found\n", path);
		return -1;
	}
	if(verbose) fprintf(stderr, "[hal] findfirst(%s) -> %s (%lld bytes)\n", path, full, (long long)st.st_size);
	if(b)
	{
		unsigned long size = (unsigned long)st.st_size;	/* ff_fsize is an unsigned long: 8 bytes on the host */
		memset(b, 0, 26 + sizeof size);
		memcpy(b + 26, &size, sizeof size);
		strncpy((char *)b + 26 + sizeof size, strrchr(full, '/') ? strrchr(full, '/') + 1 : full, 259);
	}
	return 0;
}
