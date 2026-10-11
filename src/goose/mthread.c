/*
 * mthread.c - GOOSE process dispatcher and context switch, as linked in CarnEvil
 * ($Revision: 13 $; the SDK ships revision 34). The inline assembly is transcribed from
 * the game: no interrupt masking around the stack swaps, registers are saved below the
 * stack pointer before it is adjusted, no pre-DMA hook, and die() kills through kill().
 */
/****************************************************************************/
/*                                                                          */
/* mthread.c - Source for process system for Phoenix system.                */
/*                                                                          */
/* Written by:  Michael J. Lynch                                            */
/* Version:     1.00                                                        */
/* Date:        6/28/95                                                     */
/*                                                                          */
/* Copyright (c) 1995 by Williams Electronics Games Inc.                    */
/* All Rights Reserved                                                      */
/*                                                                          */
/* Use, duplication, or disclosure is strictly forbidden unless approved    */
/* in writing by Williams Electronics Games Inc.                            */
/*                                                                          */
/* $Revision: 34 $                                                             */
/*                                                                          */
/****************************************************************************/
#include	<stdio.h>
#include	<string.h>
#ifndef VEGAS
#include	<glide/glide.h>
#else
#include	<glide.h>
#endif
#include	<goose/process.h>
#include	<goose/gfader.h>
#include	<goose/object.h>
#include    <ioctl.h>
#include	<goose/goose.h>

#ifdef VEGAS
void wait_vsync(int);
#endif

char	goose_mthread_c_version[] = {"$Revision: 13 $"};

#if defined(PROFILE) || defined(VEGAS)

__asm__("\n"
"	.set	noreorder\n"
"	.globl	___clr_count\n"
"___clr_count:\n"
"	jr	$31\n"
"	mtc0	$0,$9\n"
"\n"
"	.globl	___get_count\n"
"___get_count:\n"
"	mfc0	$2,$9\n"
"	nop\n"
"	nop\n"
"	jr	$31\n"
"	nop\n"
"	.set	reorder\n"
"");

void ___clr_count(void);
int ___get_count(void);

int	profile_enable = 1;

#define	START_TIMER	if(profile_enable) ___clr_count()

#ifndef VEGAS
#define MAX_NANOS	17600000
#define	GET_TIME(A)	if(profile_enable) (A) = (___get_count() * 13)
#else
// NOTE - This is for 250 Mhz processor
#define	GET_TIME(A)	if(profile_enable) (A) = (___get_count() * 8)
#endif
#endif


// Function prototypes for global functions used in this module
void	draw_sprites(void);
void	draw_background(void);
void	draw_transition(void);
void	dma_start(void);
void	kill(struct process_node *proc, int cause);
void	do_pre_dma_proc(void);
void	do_pre_proc(void);
void	do_post_proc(void);


// Static variables use only in this module
static int				__draw = 1;
static int				frame_factor = 1;
static int frame_time = 1;
static int enable_realtime = FALSE;

#ifndef VEGAS
static void	(*pstall_func)(void) = (void (*)(void))0xbfc00000;
static int	pstall_count = 0;
#endif

// External variables accessed from this module
#ifndef VEGAS
extern volatile int			got_interrupt;
extern int						*dma_buffer1, *dma_buffer2;
#endif
extern int						background_color;
extern struct process_node	*plist;
extern struct process_node	*pfree;
extern struct process_node	*cur_proc;

#if defined(PROFILE) || defined(VEGAS)
int	psleep_time[256];
int	pwake_time[256];
int	pdone_time[256];
int	pkill_time[256];
int	cp_next[256];
int	kill_time, kill_start, kill_end;
int	pnum;
int	pkcount = 0;
char	pdie_name[256][8];
struct process_node	__proc[256];
#if defined(VEGAS)
unsigned int	max_nanos = 0;
extern int		tsec;
#endif
#endif


// Assorted definitions
#define	FALSE	0

extern int	got_interrupt_save;		/* copy of got_interrupt taken each spin of the frame wait */

__asm__("\n"
"	.text\n"
"	.align	2\n"
"	.globl	sleep\n"
"	.ent		sleep\n"
"	.frame	$sp,0,$31\n"
"	.mask		0x00000000,0\n"
"	.fmask	0x00000000,0\n"
"	.set		noreorder\n"
"sleep:\n"
"	# Get address of current process pointer\n"
"	la		$8,cur_proc\n"
"	bne	$8,$0,1f\n"
"	nop\n"
"	break	1,7\n"
"1:\n"
"	lw		$9,0($8)\n"
"	lh		$10,12($9)\n"
"	or		$4,$10,$4\n"
"	sh		$4,12($9)\n"
"	# Drop into system mode by swapping r27 & r29.\n"
"	xor	$27,$27,$29\n"
"	xor	$29,$27,$29\n"
"	xor	$27,$27,$29\n"
"	# Save the registers preserved across calls on the process stack\n"
"	sw		$16,-4($27)\n"
"	sw		$17,-8($27)\n"
"	sw		$18,-12($27)\n"
"	sw		$19,-16($27)\n"
"	sw		$20,-20($27)\n"
"	sw		$21,-24($27)\n"
"	sw		$22,-28($27)\n"
"	sw		$23,-32($27)\n"
"	sw		$30,-36($27)\n"
"	sw		$31,-40($27)\n"
"	swc1	$f20,-44($27)\n"
"	swc1	$f22,-48($27)\n"
"	swc1	$f24,-52($27)\n"
"	swc1	$f26,-56($27)\n"
"	swc1	$f28,-60($27)\n"
"	swc1	$f30,-64($27)\n"
"	swc1	$f21,-68($27)\n"
"	swc1	$f23,-72($27)\n"
"	swc1	$f25,-76($27)\n"
"	swc1	$f27,-80($27)\n"
"	swc1	$f29,-84($27)\n"
"	swc1	$f31,-88($27)\n"
"	j		___sys_wake\n"
"	sw		$27,8($9)\n"
"	.set	reorder\n"
"	.end	sleep\n"
"");

void die( int cause )
{
	kill(cur_proc, cause);
	__asm__("\n"
"	.set	noreorder\n"
"   xor	$27,$27,$29\n"
"   xor	$29,$27,$29\n"
"   j		___sys_wake\n"
"   xor	$27,$27,$29\n"
"	.set	reorder\n"
"	");
}

void clear_screen(void)
{
	grBufferClear(background_color, 0, GR_WDEPTHVALUE_FARTHEST);
}


void process_dispatch(void)
{
	got_interrupt = 0;
	while(1)
	{
		if(__draw)
		{
			load_gamma_table();
		}
		pstall_count = 0;
		got_interrupt_save = got_interrupt;
		do
		{
			++pstall_count;
			if(pstall_count > 100000000)
			{
				pstall_func();
			}
		} while(got_interrupt < frame_factor);
		got_interrupt = 0;
		/* FRAME RATE */
		if( enable_realtime == TRUE)
		{
			/* start the timer */
			_ioctl(5, FIOCSTARTTIMER3, 0);
		}
		if(__draw)
		{
			dma_start();
		}
		do_pre_proc();
		cur_proc = plist;
		while(cur_proc)
		{
			if(cur_proc->process_sleep_time > 0)
			{
				cur_proc->process_sleep_time--;
				if(!cur_proc->process_sleep_time)
				{
					__asm__("	.set	noreorder");
					__asm__("	.set	noat");
					__asm__("	sw	$1,-4($29)");
					__asm__("	sw	$2,-8($29)");
					__asm__("	sw	$3,-12($29)");
					__asm__("	sw	$4,-16($29)");
					__asm__("	sw	$5,-20($29)");
					__asm__("	sw	$6,-24($29)");
					__asm__("	sw	$7,-28($29)");
					__asm__("	sw	$8,-32($29)");
					__asm__("	sw	$9,-36($29)");
					__asm__("	sw	$10,-40($29)");
					__asm__("	sw	$11,-44($29)");
					__asm__("	sw	$12,-48($29)");
					__asm__("	sw	$13,-52($29)");
					__asm__("	sw	$14,-56($29)");
					__asm__("	sw	$15,-60($29)");
					__asm__("	sw	$16,-64($29)");
					__asm__("	sw	$17,-68($29)");
					__asm__("	sw	$18,-72($29)");
					__asm__("	sw	$19,-76($29)");
					__asm__("	sw	$20,-80($29)");
					__asm__("	sw	$21,-84($29)");
					__asm__("	sw	$22,-88($29)");
					__asm__("	sw	$23,-92($29)");
					__asm__("	sw	$24,-96($29)");
					__asm__("	sw	$25,-100($29)");
					__asm__("	sw	$30,-108($29)");
					__asm__("	sw	$31,-112($29)");
					__asm__("	.set	at");
					__asm__("	swc1	$f0,-116($29)");
					__asm__("	swc1	$f2,-120($29)");
					__asm__("	swc1	$f4,-124($29)");
					__asm__("	swc1	$f6,-128($29)");
					__asm__("	swc1	$f8,-132($29)");
					__asm__("	swc1	$f10,-136($29)");
					__asm__("	swc1	$f12,-140($29)");
					__asm__("	swc1	$f14,-144($29)");
					__asm__("	swc1	$f16,-148($29)");
					__asm__("	swc1	$f18,-152($29)");
					__asm__("	swc1	$f20,-156($29)");
					__asm__("	swc1	$f22,-160($29)");
					__asm__("	swc1	$f24,-164($29)");
					__asm__("	swc1	$f26,-168($29)");
					__asm__("	swc1	$f28,-172($29)");
					__asm__("	swc1	$f30,-176($29)");
					__asm__("	swc1	$f21,-180($29)");
					__asm__("	swc1	$f23,-184($29)");
					__asm__("	swc1	$f25,-188($29)");
					__asm__("	swc1	$f27,-192($29)");
					__asm__("	swc1	$f29,-196($29)");
					__asm__("	swc1	$f31,-200($29)");
					__asm__("	addi	$29,$29,-204");
					__asm__("	la		$8,cur_proc");
					__asm__("	lw		$9,0($8)");
					__asm__("	lw		$27,8($9)");
					__asm__("	nop");
					__asm__("	lw		$16,-4($27)");
					__asm__("	lw		$17,-8($27)");
					__asm__("	lw		$18,-12($27)");
					__asm__("	lw		$19,-16($27)");
					__asm__("	lw		$20,-20($27)");
					__asm__("	lw		$21,-24($27)");
					__asm__("	lw		$22,-28($27)");
					__asm__("	lw		$23,-32($27)");
					__asm__("	lw		$30,-36($27)");
					__asm__("	lw		$31,-40($27)");
					__asm__("	lwc1	$f20,-44($27)");
					__asm__("	lwc1	$f22,-48($27)");
					__asm__("	lwc1	$f24,-52($27)");
					__asm__("	lwc1	$f26,-56($27)");
					__asm__("	lwc1	$f28,-60($27)");
					__asm__("	lwc1	$f30,-64($27)");
					__asm__("	lwc1	$f21,-68($27)");
					__asm__("	lwc1	$f23,-72($27)");
					__asm__("	lwc1	$f25,-76($27)");
					__asm__("	lwc1	$f27,-80($27)");
					__asm__("	lwc1	$f29,-84($27)");
					__asm__("	lwc1	$f31,-88($27)");
					__asm__("	xor	$27,$27,$29");
					__asm__("	xor	$29,$27,$29");
					__asm__("	jr		$31");
					__asm__("	xor	$27,$27,$29");
					__asm__("\t.globl\t___sys_wake\n___sys_wake:");
					__asm__("	addi	$29,$29,204");
					__asm__("	.set	noat");
					__asm__("	lw	$1,-4($29)");
					__asm__("	lw	$2,-8($29)");
					__asm__("	lw	$3,-12($29)");
					__asm__("	lw	$4,-16($29)");
					__asm__("	lw	$5,-20($29)");
					__asm__("	lw	$6,-24($29)");
					__asm__("	lw	$7,-28($29)");
					__asm__("	lw	$8,-32($29)");
					__asm__("	lw	$9,-36($29)");
					__asm__("	lw	$10,-40($29)");
					__asm__("	lw	$11,-44($29)");
					__asm__("	lw	$12,-48($29)");
					__asm__("	lw	$13,-52($29)");
					__asm__("	lw	$14,-56($29)");
					__asm__("	lw	$15,-60($29)");
					__asm__("	lw	$16,-64($29)");
					__asm__("	lw	$17,-68($29)");
					__asm__("	lw	$18,-72($29)");
					__asm__("	lw	$19,-76($29)");
					__asm__("	lw	$20,-80($29)");
					__asm__("	lw	$21,-84($29)");
					__asm__("	lw	$22,-88($29)");
					__asm__("	lw	$23,-92($29)");
					__asm__("	lw	$24,-96($29)");
					__asm__("	lw	$25,-100($29)");
					__asm__("	lw	$30,-108($29)");
					__asm__("	lw	$31,-112($29)");
					__asm__("	.set	at");
					__asm__("	lwc1	$f0,-116($29)");
					__asm__("	lwc1	$f2,-120($29)");
					__asm__("	lwc1	$f4,-124($29)");
					__asm__("	lwc1	$f6,-128($29)");
					__asm__("	lwc1	$f8,-132($29)");
					__asm__("	lwc1	$f10,-136($29)");
					__asm__("	lwc1	$f12,-140($29)");
					__asm__("	lwc1	$f14,-144($29)");
					__asm__("	lwc1	$f16,-148($29)");
					__asm__("	lwc1	$f18,-152($29)");
					__asm__("	lwc1	$f20,-156($29)");
					__asm__("	lwc1	$f22,-160($29)");
					__asm__("	lwc1	$f24,-164($29)");
					__asm__("	lwc1	$f26,-168($29)");
					__asm__("	lwc1	$f28,-172($29)");
					__asm__("	lwc1	$f30,-176($29)");
					__asm__("	lwc1	$f21,-180($29)");
					__asm__("	lwc1	$f23,-184($29)");
					__asm__("	lwc1	$f25,-188($29)");
					__asm__("	lwc1	$f27,-192($29)");
					__asm__("	lwc1	$f29,-196($29)");
					__asm__("	lwc1	$f31,-200($29)");
					__asm__("	.set	reorder");
				}
			}
			if(cur_proc)
			{
				cur_proc = cur_proc->next;
			}
			else
			{
				cur_proc = plist;
			}
		}
		do_post_proc();
		if(__draw)
		{
			draw_3d_objects();
			draw_sprites();
			draw_transition();
		}
		do_process_sort();
		/* FRAME RATE */
		if( enable_realtime == TRUE)
		{
			/* get the time in nano seconds */
			_ioctl (5, FIOCGETTIMER3, (int)&frame_time);
		}
	}
}


struct process_node *qcreate(char *name, int process_id, void (*entry_func)(), int arg1, int arg2, int arg3, int arg4)
{
	int	args[4];

	// Fill in the argument array
	args[0] = arg1;
	args[1] = arg2;
	args[2] = arg3;
	args[3] = arg4;

	// Go create the process on the FRONT of the process list
	return(create_process(name, process_id, args, entry_func, 0));
}

struct process_node *iqcreate(char *name, int process_id, void (*entry_func)(), int arg1, int arg2, int arg3, int arg4)
{
	int	args[4];

	// Fill in the argument array
	args[0] = arg1;
	args[1] = arg2;
	args[2] = arg3;
	args[3] = arg4;

	// Go create the process in the process list immediately after the
	// process that is calling this function.
	return(create_process_immediate(name, process_id, args, entry_func, 0));
}

void set_frame_factor(int f)
{
	frame_factor = f;
}

void draw_enable(int val)
{
	// If we are enabling drawing and it WAS disabled - add
	// the background draw to the dma list
	if(val == 1 && !__draw)
	{
		draw_background();
	}
	__draw = val;
}

void mthread_enable_framerate(void)
{ /* mthread_enable_framerate() */
	enable_realtime = TRUE;
} /* mthread_enable_framerate() */

void mthread_disable_framerate(void)
{ /* mthread_disable_framerate() */
	enable_realtime = FALSE;
} /* mthread_disable_framerate() */

int mthread_get_framerate(void)
{ /* mthread_get_framerate() */
	return(frame_time);
} /* mthread_get_framerate() */

void install_process_stall_func(void (*func)(void))
{
	pstall_func = func;
}
