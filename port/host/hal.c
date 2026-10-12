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
#include "hal.h"
void port_texture_verbose(int);

const char	*hal_partition_root = ".";
const char	*hal_raw_image = 0;
int		hal_frame_limit = 0;
int		hal_fast = 0;
int		hal_headless = 0;
void render_set_dump(const char *, unsigned long);
unsigned long	hal_frame_count = 0;
static int	refresh_hz = 54;			/* 400x256 54Hz, the game's normal mode */
static int	(*int_handler[8])(void);
static void	(*sys_handler[8])(void);
static int	verbose = 0;
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
		else if(!strcmp(argv[i], "-v")) { verbose = 1; port_texture_verbose(1); }
		else { fprintf(stderr, "usage: %s [--partition DIR] [--raw IMAGE] [--frames N] [--fast] [--headless] [--dump frame.ppm | --dump-every prefix N] [-v]\n", argv[0]); exit(2); }
	}
}

int hal_refresh_hz(void) { return refresh_hz; }

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
static unsigned int sound_control;
int _ioctl(int dev, int req, int arg)
{
	int	*p = (int *)(intptr_t)arg;
	if(verbose) fprintf(stderr, "[hal] ioctl(%d, %c%d, %#x)\n", dev, req >> 8, req & 0xff, arg);
	if(dev == 4)
	{
		switch(req)
		{
			case FIOC('i', 0):		/* dipswitches */
			case FIOC('i', 1): case FIOC('i', 2): case FIOC('i', 3):	/* player inputs: nothing pressed */
				if(p) *p = 0xffffffff; return 0;
			case FIOC('i', 9):		/* sound status: idle, nothing in the FIFO */
				if(p) *p = 0x8000; return 0;
			case FIOC('i', 4): if(p) *p = sound_control; return 0;		/* get sound control */
			case FIOC('i', 5): sound_control = arg; return 0;			/* set sound control */
			default: return 0;
		}
	}
	if(dev == 5)
	{
		if(req == FIOC('t', 5) || req == FIOC('t', 7)) { if(p) *p = 0; }
		return 0;
	}
	return 0;
}
int ioctl(int dev, int req, int arg) { return _ioctl(dev, req, arg); }

/* ---- files: game paths resolve case-insensitively under the partition directory ---- */
static int map_path(const char *path, char *out, size_t n)
{
	char	name[256];
	const char *p = path;
	DIR	*d;
	struct dirent *e;
	if(p[1] == ':') p += 2;
	while(*p == '\\' || *p == '/') p++;
	snprintf(name, sizeof name, "%s", p);
	for(char *q = name; *q; q++) if(*q == '\\') *q = '/';
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

#define CMOS_SIZE	0x8000
static unsigned char	cmos[CMOS_SIZE];
static long		cmos_pos;
static int		cmos_loaded;
static const char	*cmos_file = "cmos.bin";
static void cmos_load(void)
{
	FILE *f;
	if(cmos_loaded) return;
	cmos_loaded = 1;
	memset(cmos, 0xff, sizeof cmos);
	if((f = fopen(cmos_file, "rb"))) { fread(cmos, 1, sizeof cmos, f); fclose(f); }
}
static void cmos_save(void)
{
	FILE *f = fopen(cmos_file, "wb");
	if(f) { fwrite(cmos, 1, sizeof cmos, f); fclose(f); }
}

static ssize_t (*real_read)(int, void *, size_t);
static ssize_t (*real_write)(int, const void *, size_t);
static off_t (*real_lseek)(int, off_t, int);
static FILE *(*real_fopen)(const char *, const char *);
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
	if(map_path(path, full, sizeof full)) path = full;
	if(verbose) fprintf(stderr, "[hal] open(%s)\n", path);
	return real_open(path, flags & ~0x8000 /* O_BINARY */, 0644);
}
FILE *fopen(const char *path, const char *mode)
{
	char	full[1024];
	resolve();
	if(map_path(path, full, sizeof full)) path = full;
	if(verbose) fprintf(stderr, "[hal] fopen(%s, %s)\n", path, mode);
	return real_fopen(path, mode);
}
ssize_t read(int fd, void *buf, size_t n)
{
	resolve();
	if(fd == CMOS_FD)
	{
		cmos_load();
		if(cmos_pos + (long)n > CMOS_SIZE) n = CMOS_SIZE - cmos_pos;
		memcpy(buf, cmos + cmos_pos, n); cmos_pos += n; return n;
	}
	return real_read(fd, buf, n);
}
ssize_t write(int fd, const void *buf, size_t n)
{
	resolve();
	if(fd == CMOS_FD)
	{
		cmos_load();
		if(cmos_pos + (long)n > CMOS_SIZE) n = CMOS_SIZE - cmos_pos;
		memcpy(cmos + cmos_pos, buf, n); cmos_pos += n; cmos_save(); return n;
	}
	return real_write(fd, buf, n);
}
off_t lseek(int fd, off_t off, int whence)
{
	resolve();
	if(fd == CMOS_FD)
	{
		if(whence == SEEK_SET) cmos_pos = off; else if(whence == SEEK_CUR) cmos_pos += off; else cmos_pos = CMOS_SIZE + off;
		return cmos_pos;
	}
	return real_lseek(fd, off, whence);
}
int close(int fd)
{
	resolve();
	if(fd == CMOS_FD || fd == 1000) return 0;
	return real_close(fd);
}

/* DJGPP findfirst(): the sound library only asks whether a bank file exists */
struct ffblk;
int findfirst(const char *path, struct ffblk *blk, int attr)
{
	char full[1024];
	(void)blk; (void)attr;
	return map_path(path, full, sizeof full) ? 0 : -1;
}
