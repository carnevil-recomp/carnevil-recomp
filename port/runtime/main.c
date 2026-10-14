/*
 * CarnEvil host port entry: set up the HAL, then run GOOSE main() (src/goose/initg.c) on a
 * stack inside the 32-bit arena (see mem.c), so that every address the game sees fits in int.
 */
#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include "hal.h"
extern void goose_main(void);
extern void *mem_alloc_stack(size_t);
static ucontext_t host_ctx, goose_ctx;
int main(int argc, char **argv)
{
	size_t	n = 4 << 20;
	hal_init(argc, argv);
	fprintf(stderr, "[port] partition: %s\n", hal_partition_root);
	getcontext(&goose_ctx);
	goose_ctx.uc_stack.ss_sp = mem_alloc_stack(n);
	goose_ctx.uc_stack.ss_size = n;
	goose_ctx.uc_link = &host_ctx;
	makecontext(&goose_ctx, goose_main, 0);
	swapcontext(&host_ctx, &goose_ctx);
	return 0;
}
