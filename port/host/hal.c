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
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "hal.h"
#include "dcs_board.h"
#include "audio.h"
void port_texture_verbose(int);

const char	*hal_partition_root = ".";
const char	*hal_state_root = "state";		/* CMOS image and the files the game writes */
unsigned char	*hal_gun_control;			/* the gun board control byte at 0xb7f00000 */
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
static unsigned int	sound_control = 1, main_control;
unsigned		dcs_fifo_space(void);
void			dcs_fifo_reset(void);
static int		fifo_int_pending(void);
const char	*hal_raw_image = 0;
int		hal_frame_limit = 0;
int		hal_fast = 0;
int		hal_headless = 0;
void render_set_dump(const char *, unsigned long);
void audio_wav_close(void);
unsigned long	hal_frame_count = 0;
static int	refresh_hz = 54;			/* 400x256 54Hz, the game's normal mode */
static int	(*int_handler[8])(void);
static void	(*sys_handler[8])(void);
static int	verbose = 0;
static const char *wav_path;
int		port_trace = 0;

/* ---- data the library expects the surrounding image to define ---- */
int		gt_fd;
int		hwconfig[64];
float		hres, vres;
unsigned char	*dma_buffer1, *dma_buffer2;
int		free_mem_left;
int		D_801de8ec;
unsigned int	D_801a37b0[20] = { 0x19, 0x27c, 5, 0x1b2, 0x5a, 0x23, 400, 256, 0x60, 0, 0x100, 0x100, 8, 0x17, 1, 1 };

#define CMOS_FD		3
static void resolve(void);
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
	/* fd 3 is the BIOS CMOS device: park /dev/null there so no real file ever gets it */
	resolve();
	nul = real_open("/dev/null", O_RDONLY, 0);
	if(nul >= 0 && nul != CMOS_FD) { dup2(nul, CMOS_FD); real_close(nul); }
	for(i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "--partition") && i + 1 < argc) hal_partition_root = argv[++i];
		else if(!strcmp(argv[i], "--raw") && i + 1 < argc) hal_raw_image = argv[++i];
		else if(!strcmp(argv[i], "--frames") && i + 1 < argc) hal_frame_limit = atoi(argv[++i]);
		else if(!strcmp(argv[i], "--fast")) hal_fast = 1;
		else if(!strcmp(argv[i], "--headless")) hal_headless = 1;
		else if(!strcmp(argv[i], "--dump") && i + 1 < argc) render_set_dump(argv[++i], 0);
		else if(!strcmp(argv[i], "--dump-every") && i + 2 < argc) { render_set_dump(argv[i+1], atoi(argv[i+2])); i += 2; }
		else if(!strcmp(argv[i], "--wav") && i + 1 < argc) wav_path = argv[++i];
		else if(!strcmp(argv[i], "--press") && i + 1 < argc) parse_presses(argv[++i]);
		else if(!strcmp(argv[i], "--state") && i + 1 < argc) hal_state_root = argv[++i];
		else if(!strcmp(argv[i], "-v")) { verbose++; port_texture_verbose(1); dcs_set_verbose(verbose); }
		else { fprintf(stderr, "usage: %s [--partition DIR] [--raw IMAGE] [--state DIR] [--wav FILE] [--press F:switch,...] [--frames N] [--fast] [--headless] [--dump frame.ppm | --dump-every prefix N] [-v]\n", argv[0]); exit(2); }
	}
	hal_gun_regs = mmap((void *)0xb6800000, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
	if(hal_gun_regs == MAP_FAILED) { fprintf(stderr, "[hal] cannot map the gun port at 0xb6800000\n"); hal_gun_regs = 0; }
	else gun_update();
	hal_gun_control = mmap((void *)0xb7f00000, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
	if(hal_gun_control == MAP_FAILED) { fprintf(stderr, "[hal] cannot map the gun control byte at 0xb7f00000\n"); hal_gun_control = 0; }
	if(wav_path) { audio_wav_open(wav_path); atexit(audio_wav_close); }
	else if(!hal_headless && !audio_start()) fprintf(stderr, "[audio] no audio output\n");
}

int hal_refresh_hz(void) { return refresh_hz; }

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
static void gun_update(void)
{
	if(!hal_gun_regs) return;
	/* MAME carnevil_gun_r layout, one byte per 32-bit slot: X low, X high | buttons | 0x40, Y low, Y high; P2 at +0x10 */
	hal_gun_regs[0x00] = (128 << 4) & 0xff; hal_gun_regs[0x04] = (128 >> 4) | ((gun_held & 3) << 4) | 0x40;
	hal_gun_regs[0x08] = (128 << 4) & 0xff; hal_gun_regs[0x0c] = 128 >> 4;
	hal_gun_regs[0x10] = (128 << 4) & 0xff; hal_gun_regs[0x14] = (128 >> 4) | (gun_held & 0x30) | 0x40;
	hal_gun_regs[0x18] = (128 << 4) & 0xff; hal_gun_regs[0x1c] = 128 >> 4;
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
		presses_update();
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

int _ioctl(int dev, int req, int arg)
{
	int	*p = (int *)(intptr_t)arg;
	if(verbose > 1) fprintf(stderr, "[hal] ioctl(%d, %c%d, %#x)\n", dev, req >> 8, req & 0xff, arg);
	if(dev == 4)
	{
		switch(req)
		{
			case FIOC('i', 0): if(p) *p = dipswitches; return 0;
			case FIOC('i', 1): if(p) *p = 0xffff & ~coin_held; return 0;		/* coins, starts, service, test, volume: active low */
			case FIOC('i', 2): if(p) *p = 0xffff & ~player_held; return 0;	/* player 1 and 2 buttons */
			case FIOC('i', 3): if(p) *p = 0xffff; return 0;			/* players 3 and 4: none */
			case FIOC('i', 4): if(p) *p = 0; return 0;			/* coin meters */
			case FIOC('i', 5): return 0;
			case FIOC('i', 6):						/* sound control: bit 0 low->high resets the board, bits 1-3 reset the FIFO */
				if((arg & 1) && !(sound_control & 1)) dcs_reset();
				if((arg & 0xe) && !(sound_control & 0xe)) dcs_fifo_reset();
				sound_control = arg; return 0;
			case FIOC('i', 7): if(p) *p = sound_control; return 0;
			case FIOC('i', 8): dcs_write(arg); return 0;			/* host -> sound data */
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
static void resolve(void)
{
	if(real_open) return;
	real_open = dlsym(RTLD_NEXT, "open"); real_read = dlsym(RTLD_NEXT, "read");
	real_write = dlsym(RTLD_NEXT, "write"); real_lseek = dlsym(RTLD_NEXT, "lseek");
	real_close = dlsym(RTLD_NEXT, "close"); real_fopen = dlsym(RTLD_NEXT, "fopen");
}

int open(const char *path, int flags, ...)
{
	char	full[1024];
	resolve();
	if(!strcmp(path, "pci:")) return 1000;		/* the system controller device */
	if(!strcmp(path, "pic:")) return 1001;		/* the security PIC (serial number) */
	if(path[0] == '/') { }			/* a host path from the port itself */
	else if(flags & (O_WRONLY | O_RDWR | O_CREAT)) state_path(path, full, sizeof full), path = full;
	else if(map_path(path, full, sizeof full)) path = full;
	if(verbose) fprintf(stderr, "[hal] open(%s)\n", path);
	return real_open(path, flags & ~0x8000 /* O_BINARY */, 0644);
}
FILE *fopen(const char *path, const char *mode)
{
	char	full[1024];
	resolve();
	if(path[0] == '/') { }			/* a host path from the port itself */
	else if(strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+')) state_path(path, full, sizeof full), path = full;
	else if(map_path(path, full, sizeof full)) path = full;
	if(verbose) fprintf(stderr, "[hal] fopen(%s, %s)\n", path, mode);
	return real_fopen(path, mode);
}
#ifdef __APPLE__
/* this file sees macOS's stdio.h, which renames fopen to fopen$DARWIN_EXTSN; the game and
   library objects are built against the SDK headers and call plain _fopen, so define that too */
FILE *fopen_plain(const char *path, const char *mode) __asm__("_fopen");
FILE *fopen_plain(const char *path, const char *mode) { return fopen(path, mode); }
#endif
ssize_t read(int fd, void *buf, size_t n)
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
ssize_t write(int fd, const void *buf, size_t n)
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
off_t lseek(int fd, off_t off, int whence)
{
	resolve();
	if(fd == CMOS_FD)
	{
		if(whence == SEEK_SET) cmos_pos = off; else if(whence == SEEK_CUR) cmos_pos += off; else cmos_pos = CMOS_WORDS + off;
		return cmos_pos;
	}
	return real_lseek(fd, off, whence);
}
int close(int fd)
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
