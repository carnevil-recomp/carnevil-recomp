/*
 * CarnEvil host port entry: set up the HAL, then run GOOSE main() (src/goose/initg.c) on a
 * stack inside the 32-bit arena (see mem.c), so that every address the game sees fits in int.
 */
#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include "hostctx.h"
#include "hal.h"
extern void goose_main(void);
extern void *mem_alloc_stack(size_t);
static hostctx_t host_ctx, goose_ctx;
int main(int argc, char **argv)
{
	size_t	n = 4 << 20;
	hal_init(argc, argv);
	fprintf(stderr, "[port] partition: %s\n", hal_partition_root);
	hctx_host(&host_ctx);
	hctx_make(&goose_ctx, goose_main, mem_alloc_stack(n), n, &host_ctx);
	hctx_swap(&host_ctx, &goose_ctx);
	return 0;
}
