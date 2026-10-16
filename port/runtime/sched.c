/*
 * sched.c - the GOOSE process dispatcher for the host port.
 *
 * Replaces mthread.c, whose dispatcher, sleep() and die() switch MIPS register sets by hand.
 * The loop below is mthread.c's process_dispatch() line for line; the register switching is
 * done with ucontext (fibers on Windows, hostctx.h) on a private host stack per process. process.c (the process lists,
 * create_process, kill, ...) is the original library source, unchanged.
 */
#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "hostctx.h"
#include <goose/process.h>
#include "hal.h"

#define HOST_STACK_SIZE	(512 * 1024)
#define GR_WDEPTHVALUE_FARTHEST	0xffff
#define TRUE	1
#define FALSE	0

extern struct process_node *plist, *pfree, *cur_proc;
extern volatile int got_interrupt;
extern int background_color;
extern void load_gamma_table(void), dma_start(void), do_pre_proc(void), do_post_proc(void);
extern void draw_3d_objects(void), draw_sprites(void), draw_transition(void), draw_background(void);
extern void do_process_sort(void), kill(struct process_node *, int);
extern void grBufferClear(unsigned int, int, int);
extern int _ioctl(int, int, int);

static int	__draw = 1;
static int	frame_factor = 1;
static int	frame_time = 1;
static int	enable_realtime = FALSE;
volatile int got_interrupt_save;
int	profile_enable = 1;

/* one host context per live process node; nodes are recycled by process.c, so an entry is
   re-initialised whenever create_process() has written its fresh-stack marker */
typedef struct hctx {
	struct process_node	*pn;
	hostctx_t		ctx;
	void			*stack;
	int			dead;
	struct hctx		*next;
} hctx_t;
static hctx_t		*ctx_list;
static hctx_t		*running;
static hostctx_t	sys_ctx;

void _proc_shell(void) { }		/* only its address matters: create_process() stores it as the marker */

static int fresh_marker(struct process_node *pn)
{
	int	*pdata = (int *)pn->process_stack_ptr - 10;
	return pn->process_data && *pdata == (int)(intptr_t)_proc_shell;
}

static hctx_t *find_ctx(struct process_node *pn)
{
	hctx_t	*h;
	for(h = ctx_list; h; h = h->next)
		if(h->pn == pn) return h;
	h = calloc(1, sizeof(*h)); h->pn = pn; h->next = ctx_list; ctx_list = h;
	return h;
}

static void proc_entry(void)
{
	struct process_node	*pn = cur_proc;
	pn->entry_func(pn->process_data);		/* PSHELL.S: entry_func(args) */
	die(1);						/* if it returns - die */
}

static void resume(struct process_node *pn)
{
	hctx_t	*h = find_ctx(pn);
	if(fresh_marker(pn))
	{
		*((int *)pn->process_stack_ptr - 10) = 0;
		if(!h->stack) h->stack = malloc(HOST_STACK_SIZE);
		h->dead = 0;
		hctx_make(&h->ctx, proc_entry, h->stack, HOST_STACK_SIZE, &sys_ctx);
	}
	running = h;
	hctx_swap(&sys_ctx, &h->ctx);
	running = 0;
}

/* true while a GOOSE process runs, so port code may call sleep() */
int sched_in_process(void) { return cur_proc && running; }

/* GOOSE sleep(): add ticks to the current process' sleep count and yield to the dispatcher */
void sleep(int ticks)
{
	if(!cur_proc || !running)
	{
		fprintf(stderr, "sleep() called outside a process\n");
		abort();
	}
	cur_proc->process_sleep_time |= ticks;
	hctx_swap(&running->ctx, &sys_ctx);
}

void die(int cause)
{
	hctx_t	*h = running;
	kill(cur_proc, cause);
	h->dead = 1;
	hctx_set(&sys_ctx);
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
		got_interrupt_save = got_interrupt;
		while(got_interrupt < frame_factor)
		{
			hal_wait_frame(1);
		}
		got_interrupt = 0;
		if(enable_realtime == TRUE)
		{
			_ioctl(5, 0x6b05 /* FIOCSTARTTIMER3 */, 0);
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
					resume(cur_proc);
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
		if(enable_realtime == TRUE)
		{
			_ioctl(5, 0x6b08 /* FIOCGETTIMER3 */, (int)(intptr_t)&frame_time);
		}
	}
}

struct process_node *qcreate(char *name, int process_id, void (*entry_func)(), int arg1, int arg2, int arg3, int arg4)
{
	int	args[4];
	args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4;
	return(create_process(name, process_id, args, entry_func, 0));
}

struct process_node *iqcreate(char *name, int process_id, void (*entry_func)(), int arg1, int arg2, int arg3, int arg4)
{
	int	args[4];
	args[0] = arg1; args[1] = arg2; args[2] = arg3; args[3] = arg4;
	return(create_process_immediate(name, process_id, args, entry_func, 0));
}

void set_frame_factor(int f) { frame_factor = f; }
void draw_enable(int val)
{
	if(val == 1 && !__draw)
	{
		draw_background();
	}
	__draw = val;
}
void mthread_enable_framerate(void) { enable_realtime = TRUE; }
void mthread_disable_framerate(void) { enable_realtime = FALSE; }
int mthread_get_framerate(void) { return(frame_time); }
void install_process_stall_func(void (*func)(void)) { (void)func; }
void wait_vsync(int n) { hal_wait_frame(n); }
