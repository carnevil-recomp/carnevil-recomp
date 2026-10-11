/*
 * game_stub.c - stand-ins for the game-side symbols GOOSE main() needs before the game's
 * own code is decompiled. main_proc here is a placeholder that exercises the dispatcher;
 * src/game/main.c (the real main_proc) replaces it once its callees exist.
 */
#include <stdio.h>
#include <stdlib.h>
#include <goose/process.h>
#include "hal.h"

extern void sleep(int), die(int);
extern int interrupt_handler(void);
extern struct process_node *qcreate(char *, int, void (*)(), int, int, int, int);
extern struct process_node *cur_proc;

void func_800ca4a8(void) { }					/* records get_free_mem_left() */
void func_8013f5b4(void) { }					/* two I/O ASIC ioctls after graphics init */
int  vblank_handler(void) { return interrupt_handler(); }	/* the game's handler wraps the library's */
int  interrupt_ioasic_handler(void) { return 1; }
void game_dma0_handler(void) { }

static void child_proc(int *args)
{
	int	i;
	for(i = 0; i < args[0]; i++)
	{
		fprintf(stderr, "[demo] child tick %d (frame %lu)\n", i, hal_frame_count);
		sleep(1);
	}
	fprintf(stderr, "[demo] child done\n");
}

void main_proc(int *args)
{
	int	n = 0;
	(void)args;
	fprintf(stderr, "[demo] main_proc running in process \"%.8s\"\n", cur_proc->process_name);
	qcreate("child", 1, child_proc, 3, 0, 0, 0);
	while(1)
	{
		if(n % 60 == 0) fprintf(stderr, "[demo] main_proc frame %lu\n", hal_frame_count);
		n++;
		sleep(1);
	}
}
