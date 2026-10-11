/*
 * initg.c - Mother GOOSE initialization and main() as linked into CarnEvil
 * ($Revision: 8 $ of the GOOSE library, older than the SDK's INITG.C: no
 * goose_init_info struct, text goes to stderr, the game supplies main_proc).
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "types.h"

typedef struct {
	u32	hSyncOn, hSyncOff, vSyncOn, vSyncOff, hBackPorch, vBackPorch;
	u32	xDimension, yDimension, memOffset, unk9;
	u32	memFifoEntries_1MB, memFifoEntries_2MB, memFifoEntries_4MB, vFifoThreshold;
	u32	video24BPP, unk15;
	float	clkFreq16BPP, clkFreq24BPP;
} sst1VideoTimingStruct;

#define DMA_BUFFER_SIZE	0xe0000
#ifdef PORT_HOST
#define PHYS(p)	(p)					/* the host HAL hands out plain buffers */
#else
#define PHYS(p)	((unsigned char *)((int)(p) & 0x1FFFFFFF))	/* KSEG0 -> physical for the Galileo DMA */
#endif

/* externs resolved by the surrounding build (names from config/names.txt) */
extern sst1VideoTimingStruct	D_801a37b0;		/* default 400x256 timing (glide table) */
extern int	D_801de8ec;
extern int	is_low_res;
extern int	gt_fd;
extern unsigned char	*dma_buffer1, *dma_buffer2;
extern float	hres, vres;
extern int	hwconfig[];
extern int	free_mem_left;

extern int	func_801476e8(void);
extern int	func_80157004(void);
extern void	func_80146c60(void), func_8014cd88(void), func_8013f5b4(void), func_800ca4a8(void);
extern void	install_sys_handlers(void);
extern void	graphics_init(void);
extern void	grSstVidMode(int, sst1VideoTimingStruct *);
extern void	grGlideInit(void);
extern int	grSstQueryHardware(int *);
extern void	grSstSelect(int);
extern int	grSstOpen(int, int, int, int, int, int);
extern void	grBufferClear(u32, int, int);		/* grBufferClear */
extern void	grTexCombineFunction(int, int);
extern void	func_801669ac(int), func_8015f86c(int), grDepthBufferFunction(int), grDepthMask(int);
extern void	install_interrupt_handler(int, int (*)(void));
extern void	install_sys_handler(int, void (*)(void));
extern void	install_fpu_handler(int, void (*)(int *));
extern void	install_int_div0_handler(void (*)());
extern void	install_int_divo_handler(void (*)());
extern int	vblank_handler(void), interrupt_ioasic_handler(void);
extern void	game_dma0_handler(void), galileo_dma0_handler(void);
extern void	div0_handler(int *), unimplemented_handler(int *);
extern void	int_div0_handler(), int_divo_handler();
extern void	main_proc(int *);
extern void	*qcreate(char *, int, void (*)(), int, int, int, int);
extern void	disable_lockup(void), process_dispatch(void);

static sst1VideoTimingStruct	default_timing =
	{ 0x19, 0x27c, 5, 0x1b2, 0x5a, 0x23, 512, 384, 0x60, 0, 0x100, 0x100, 8, 0x17, 1, 1, 16.5161f, 33.0323f };
static sst1VideoTimingStruct	lores_timing =
	{ 0x19, 0x27c, 5, 0x1b2, 0x5a, 0x63, 512, 256, 0x60, 0, 0x100, 0x100, 8, 0x17, 1, 1, 16.5161f, 33.0323f };
sst1VideoTimingStruct	*cur_timing = 0;
int	gfx_scale = 100;
char	*banner[] =
{
	"\n\n",
	"                           Mother GOOSE - R5000",
	"                              $Revision: 8 $",
	"\n",
	"                   Copyright (c) 1997 by Midway Games Inc.",
	"                 Portions Copyright (c) 1994 by DJ Delorie.",
	"            Portions Copyright (c) 1996 by 3DFx Interactive Inc.",
	"      Portions Copyright (c) 1996 by Integrated Device Technologies Inc.",
	"   Portions Copyright (c) 1983 by Regents of the University of California.",
	"\n\n"
};

/*
 * graphics_init() is entered through a two-instruction stub that loads $gp with the DMA
 * buffer pointer (the draw code uses $gp as the live DMA write pointer from here on) and
 * falls through into the C body. The original was almost certainly written this way: no
 * compiler switch produces a $gp load ahead of the prologue.
 */
#ifndef PORT_HOST
asm("	.text\n"
    "	.globl	graphics_init\n"
    "graphics_init:\n"
    "	lui	$gp,%hi(dma_buffer1)\n"
    "	lw	$gp,%lo(dma_buffer1)($gp)\n"
    "	.rdata");		/* back to the section the compiler believes it is in */
#define graphics_init_body graphics_init_body
#else
#define graphics_init_body graphics_init
#define main goose_main			/* the host port has its own main() */
#endif

static void print_banner(void)
{
	unsigned int	i;

	for(i = 0; i < 10; i++)
	{
		fprintf(stderr, "%s\n", banner[i]);
	}
}

void graphics_init_body(void)
{
	sst1VideoTimingStruct	*vt;
	int	res;
	int	ok;

	D_801de8ec = 0;
	func_801476e8();
	fprintf(stderr, "Setting ");
	fflush(stdout);
	if(getenv("SIM_LORES") == NULL)
	{
		fprintf(stderr, "400x256 54Hz");
		fflush(stdout);
		vt = &D_801a37b0;
		res = 0x36;
	}
	else
	{
		fprintf(stderr, "512x256 57Hz LO RES SIMULATION");
		fflush(stdout);
		vt = &lores_timing;
		is_low_res = 1;
		res = 0x39;
	}
	gfx_scale = res;
	grSstVidMode(0, vt);
	grGlideInit();
	if(!grSstQueryHardware(hwconfig))
	{
		fprintf(stderr, "\ngraphics_init(): Graphics Hardware initialization failure\n");
		exit(0);
	}
	grSstSelect(0);
	ok = grSstOpen(2, 9, 0, 1, 1, 2);
	hres = (float)vt->xDimension;
	vres = (float)vt->yDimension;
	if(!ok)
	{
		fprintf(stderr, "\ngraphics_init(): ERROR graphics subsystem initialization failure\n");
		exit(0);
	}
	fprintf(stderr, " screen resolution\n");
	fflush(stdout);
	grBufferClear(0xff000000, 0, 0xffff);
	grTexCombineFunction(0, 1);
	func_801669ac(1);
	func_8015f86c(2);
	grDepthBufferFunction(1);
	grDepthMask(1);
	gfx_scale = func_80157004();
}

void main(void)
{
	gt_fd = open("pci:", O_RDWR | O_BINARY);
	if(gt_fd == -1)
	{
		fprintf(stderr, "Can not open the system controller device\n");
		exit(0);
	}
	print_banner();
	func_800ca4a8();
	dma_buffer1 = malloc((DMA_BUFFER_SIZE * 2) + 32);
	if(dma_buffer1 == NULL)
	{
		fprintf(stderr, "Can not allocate memory for DMA buffers\n");
		exit(0);
	}
	while(((int)dma_buffer1 & 0x1F) != 0)
	{
		dma_buffer1++;
	}
	dma_buffer2 = dma_buffer1 + DMA_BUFFER_SIZE;
	dma_buffer1 = PHYS(dma_buffer1);
	dma_buffer2 = PHYS(dma_buffer2);
	func_80146c60();
	func_8014cd88();
	graphics_init();
	install_interrupt_handler(0, vblank_handler);
	install_sys_handler(4, game_dma0_handler);	/* the game's own handler, replaced below by the library's */
	func_8013f5b4();
	install_interrupt_handler(3, interrupt_ioasic_handler);
	install_fpu_handler(5, unimplemented_handler);
	install_sys_handler(4, galileo_dma0_handler);
	install_fpu_handler(3, div0_handler);
	install_int_div0_handler(int_div0_handler);
	install_int_divo_handler(int_divo_handler);
	qcreate("main", 0, main_proc, 0, 0, 0, 0);
	disable_lockup();
	install_sys_handlers();
	disable_lockup();
	fprintf(stderr, "Starting process dispatcher\n");
	process_dispatch();
	fprintf(stderr, "process_dispatch returned\n");
	exit(0);
}
