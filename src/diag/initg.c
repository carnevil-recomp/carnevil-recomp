/*
 * initg.c - Mother GOOSE initialization and main() as linked into DIAG.EXE: text
 * 0x801002d8-0x801009ac (then the linker's junk padding word), rodata 0x80152170-0x8015255b,
 * data 0x8015cba0-0x8015cc68, no bss. Built at -O3 with signed chars (profile diag3): the
 * static print_banner() is inlined into main(); unsigned chars lose main()'s lb loads.
 *
 * $Revision: 21 $ of the GOOSE library's INITG.C ("Phoenix Seattle" in the banner), between
 * the game's revision 8 (src/goose/initg.c) and the SDK's goose_init(): still a main() with
 * the text on stderr, but DIAG's own start-up in it: the volume bank name from the
 * environment, the audit and adjustment formats (AUDITS.FMT, ADJUST.FMT), the guns'
 * calibration, CARN_TEST for the resolution, the bad-hardware screen for a revision A board,
 * and the main process created with the BIOS arguments.
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

/* a light gun's record, as guntest.c's */
typedef struct gun
{
	int	raw_x, raw_y;
	int	on_screen;
	float	scale_x, scale_y;
	float	off_x, off_y;
	int	screen_x, screen_y;
} gun_t;

#define DMA_BUFFER_SIZE	0x180000
#define PHYS(p)	((unsigned char *)((int)(p) & 0x1FFFFFFF))	/* KSEG0 -> physical for the Galileo DMA */

extern int	guns_calibrated;		/* 0x802ef4e4 (guntest.c) */
extern gun_t	guns[];				/* 0x802f03b0 (guntest.c) */
extern int	get_adjustment(int, int *);

extern sst1VideoTimingStruct	D_80171e00;	/* glide's 400x256 54Hz timing (SST_VREZ_400X256_54) */
extern int	D_802ef43c;			/* glide: sst1InitGrxClk_Called */
extern int	D_801725d0[];			/* the GrHwConfiguration */
extern int	font_set;			/* 0x801694cc */
extern float	hres, vres;
extern int	D_802ef430;			/* gt_fd */
extern unsigned char	*D_802ef480, *D_802ef488;	/* dma_buffer1, dma_buffer2 */
extern char	*snd_bank_name;			/* 0x802ef5b4 */
extern char	volume_call;			/* 0x802fdc08 */
extern int	cmos_config[];			/* 0x802ef4f0 */

extern void	grSstVidMode(int, sst1VideoTimingStruct *);
extern void	grGlideInit(void);
extern int	func_8013d284(int *);		/* grSstQueryHardware */
extern void	grSstSelect(int);
extern int	func_8013c8bc(int, int, int, int, int, int);	/* grSstOpen */
extern void	func_8013a118(u32, int, int);	/* grBufferClear */
extern void	grTexCombineFunction(int, int);
extern void	guColorCombineFunction(int);
extern void	func_8013ac48(int);		/* grDepthBufferMode */
extern void	func_8013ab94(int);		/* grDepthBufferFunction */
extern void	func_8013ad14(int);		/* grDepthMask */
extern int	is_revision_a(void);
extern void	func_801396c4(char *);
extern void	func_8013a360(int);
extern int	func_8014bf0c(void);
extern void	switch_init(void), init_sprites(void);
extern void	graphics_init(void);
extern void	install_interrupt_handler(int, int (*)(void));
extern void	install_sys_handler(int, void (*)(void));
extern int	interrupt_handler(void);
extern void	galileo_dma0_handler(void);
extern int	load_audits(char *), load_adjustments(char *);
extern void	config_cmos(int *);
extern void	main_proc(int *);
extern void	*func_8012b704(char *, int, int *, void (*)(int *), int);	/* create_process */
extern void	func_80131538(void);		/* process_dispatch */

int	gfx_scale = 100;			/* 0x8015cba0 */
int	D_8015cba4 = 0;				/* the BIOS asked for the short menus (no CLEAR AUDITS) */
static sst1VideoTimingStruct	default_timing =
	{ 0x19, 0x27c, 5, 0x1b2, 0x5a, 0x23, 512, 384, 0x60, 0, 0x100, 0x100, 8, 0x17, 1, 1, 16.5161f, 33.0323f };
static sst1VideoTimingStruct	lores_timing =
	{ 0x19, 0x27c, 5, 0x1b2, 0x5a, 0x63, 512, 256, 0x60, 0, 0x100, 0x100, 8, 0x17, 1, 1, 16.5161f, 33.0323f };
sst1VideoTimingStruct	*cur_timing = 0;

/* 0x801002d8: the guns' calibration from the CMOS, when its magic word says there is one.
   get_float_adjustment() has no prototype here: the defaults go as doubles in a2/a3 (one,
   285.529419, is even written without the f) */
void func_801002d8(void)
{
	int	magic;

	gun_board_reset();
	guns_calibrated = 0;
	if(!get_adjustment(0x52, &magic))
	{
		fprintf(stderr, "got adjustment!!!\n");
		if(magic == 0x23a4729)
		{
			fprintf(stderr, "GUN_CALIB_VALID is good!!!\n");
			guns_calibrated = 1;
			get_float_adjustment(0x53, &guns[0].scale_x, 4.02353f);
			get_float_adjustment(0x56, &guns[0].scale_y, 0.947368f);
			get_float_adjustment(0x59, &guns[0].off_x, 285.529419);
			get_float_adjustment(0x5c, &guns[0].off_y, 25.052631f);
			get_float_adjustment(0x5f, &guns[1].scale_x, 4.011765f);
			get_float_adjustment(0x62, &guns[1].scale_y, 0.894737f);
			get_float_adjustment(0x65, &guns[1].off_x, 271.764709f);
			get_float_adjustment(0x68, &guns[1].off_y, 39.105263f);
		}
	}
}

/*
 * 0x8010042c: graphics_init() loads $gp with the DMA buffer pointer ahead of the prologue and
 * falls through into the C body, as in the game's revision (see src/goose/initg.c). cc1 still
 * believes it is in .text after func_801002d8 and switches to .rdata for the strings itself.
 */
asm("	.text\n"
    "	.globl	graphics_init\n"
    "graphics_init:\n"
    "	lui	$gp,%hi(D_802ef480)\n"
    "	lw	$gp,%lo(D_802ef480)($gp)");

void graphics_init_body(void)
{
	sst1VideoTimingStruct	*vt;
	int	ok;

	D_802ef43c = 0;
	fprintf(stderr, "Setting ");
	fflush(stdout);
	if(getenv("CARN_TEST"))
	{
		fprintf(stderr, "512x384 57Hz");
		vt = &default_timing;
		gfx_scale = 57;
		font_set = 1;
	}
	else
	{
		fprintf(stderr, "400x256 54Hz");
		vt = &D_80171e00;
		gfx_scale = 54;
		font_set = 1;
	}
	grSstVidMode(0, vt);
	grGlideInit();
	if(!func_8013d284(D_801725d0))
	{
		fprintf(stderr, "\ngraphics_init(): Graphics Hardware initialization failure\n");
		exit(0);
	}
	grSstSelect(0);
	ok = func_8013c8bc(2, 9, 0, 1, 1, 2);
	/* CARN_TEST: the 512x384 timing, but laid out for 400x256 */
	if(getenv("CARN_TEST"))
	{
		hres = 400.0f;
		vres = 256.0f;
	}
	else
	{
		hres = vt->xDimension;
		vres = vt->yDimension;
	}
	if(!ok)
	{
		fprintf(stderr, "\ngraphics_init(): ERROR graphics subsystem initialization failure\n");
		exit(0);
	}
	fprintf(stderr, " screen resolution\n");
	fflush(stdout);
	func_8013a118(0xff000000, 0, 0xffff);
	grTexCombineFunction(0, 1);
	guColorCombineFunction(1);
	func_8013ac48(2);
	func_8013ab94(1);
	func_8013ad14(1);
	if(!is_revision_a())
	{
		func_801396c4("badhard.wms");
		func_8013a360(0);
		while(1) ;
	}
	gfx_scale = func_8014bf0c();
}

char	*banner[] =
{
	"\n\n",
	"                           Mother GOOSE - R5000",
	"                              Phoenix Seattle",
	"                              $Revision: 21 $",
	"\n",
	"                   Copyright (c) 1997 by Midway Games Inc.",
	"                 Portions Copyright (c) 1994 by DJ Delorie.",
	"            Portions Copyright (c) 1996 by 3DFx Interactive Inc.",
	"      Portions Copyright (c) 1996 by Integrated Device Technologies Inc.",
	"   Portions Copyright (c) 1983 by Regents of the University of California.",
	"\n\n"
};

static void print_banner(void)
{
	unsigned int	i;

	for(i = 0; i < 11; i++)
	{
		fprintf(stderr, "%s\n", banner[i]);
	}
}

void main(int *args)
{
	char	*p;

	if((snd_bank_name = getenv("VOL_ADJ_BANK_NAME")) == NULL)
	{
		snd_bank_name = "wavinmus";
	}
	else
	{
		p = snd_bank_name;
		while(*p != '\r' && *p && *p != '\n')
		{
			p++;
		}
		*p = 0;
	}
	volume_call = 3;
	D_802ef430 = open("pci:", O_RDWR | O_BINARY);
	if(D_802ef430 == -1)
	{
		fprintf(stderr, "Can not open the system controller device\n");
		exit(0);
	}
	print_banner();
	/* the + 32 of the game's malloc((DMA_BUFFER_SIZE * 2) + 32) ended up outside the call */
	D_802ef480 = malloc(DMA_BUFFER_SIZE * 2) + 32;
	if(D_802ef480 == NULL)
	{
		fprintf(stderr, "Can not allocate memory for DMA buffers\n");
		exit(0);
	}
	while(((int)D_802ef480 & 0x1F) != 0)
	{
		D_802ef480++;
	}
	D_802ef488 = D_802ef480 + DMA_BUFFER_SIZE;
	D_802ef480 = PHYS(D_802ef480);
	D_802ef488 = PHYS(D_802ef488);
	switch_init();
	init_sprites();
	graphics_init();
	install_interrupt_handler(0, interrupt_handler);
	install_sys_handler(4, galileo_dma0_handler);
	if(*args)
	{
		D_8015cba4 = 1;
	}
	else
	{
		D_8015cba4 = 0;
	}
	if(load_audits("AUDITS.FMT"))
	{
		config_cmos(cmos_config);
	}
	load_adjustments("ADJUST.FMT");
	func_801002d8();
	func_8012b704("main", 0, args, main_proc, 0x20000);
	fprintf(stderr, "Starting process dispatcher\n");
	func_80131538();
	fprintf(stderr, "process_dispatch returned\n");
	exit(0);
}

/* 0x8010099c: an empty function, unreferenced (the game has an empty one at 0x800c47a8,
   which its grBufferSwap calls) */
void func_8010099c(void)
{
}

/* 0x801009a4: called by galileo_timer3_handler on a DMA timeout: the SDK's
   do_dma_timeout_reboot() (galhand.c), empty here */
void func_801009a4(void)
{
}
