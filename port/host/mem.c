/*
 * mem.c - a 32-bit-addressable arena for everything the game can see.
 *
 * The SDK and game sources are ILP32 and pass pointers through int (ioctl arguments, the
 * process-stack marker, alignment tests). On a 64-bit host that only works if every such
 * pointer lies below 4 GB, so malloc/free/calloc/realloc are replaced by a first-fit
 * allocator on an arena mapped at a fixed low address, and all stacks (the GOOSE main loop's
 * and each process') come from the same arena. The executable itself must allow low
 * mappings (macOS: -Wl,-pagezero_size,0x1000).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#define ARENA_ADDR	0x20000000u
#define ARENA_SIZE	(256u << 20)
#define ALIGN		16u

typedef struct blk { size_t size; int free; struct blk *next, *prev; } blk_t;	/* 32-byte header */
static unsigned char	*arena;
static blk_t		*head;

static void arena_init(void)
{
	void *p;
	if(arena) return;
	p = mmap((void *)(uintptr_t)ARENA_ADDR, ARENA_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
	if(p == MAP_FAILED || (uintptr_t)p != ARENA_ADDR)
	{
		const char msg[] = "[mem] cannot map the 32-bit arena (link with -Wl,-pagezero_size,0x1000)\n";
		(void)!write(2, msg, sizeof msg - 1); _exit(1);
	}
	arena = p;
	head = (blk_t *)arena; head->size = ARENA_SIZE - sizeof(blk_t); head->free = 1; head->next = head->prev = 0;
}

static void split(blk_t *b, size_t n)
{
	if(b->size >= n + sizeof(blk_t) + ALIGN)
	{
		blk_t *r = (blk_t *)((unsigned char *)(b + 1) + n);
		r->size = b->size - n - sizeof(blk_t); r->free = 1; r->next = b->next; r->prev = b;
		if(b->next) b->next->prev = r;
		b->next = r; b->size = n;
	}
}

void *malloc(size_t n)
{
	/* fresh arcade memory is zero and the game relies on it here and there (records it does not
	   fully initialise); the port zeroes every block */
	blk_t *b;
	arena_init();
	n = (n + ALIGN - 1) & ~(size_t)(ALIGN - 1);
	if(!n) n = ALIGN;
	for(b = head; b; b = b->next)
	{
		if(b->free && b->size >= n) { split(b, n); b->free = 0; memset(b + 1, 0, n); return b + 1; }
	}
	fprintf(stderr, "[mem] arena exhausted (%zu bytes requested)\n", n);
	return 0;
}

void free(void *p)
{
	blk_t *b;
	if(!p) return;
	b = (blk_t *)p - 1;
	b->free = 1;
	if(b->next && b->next->free) { blk_t *n = b->next; b->size += sizeof(blk_t) + n->size; b->next = n->next; if(n->next) n->next->prev = b; }
	if(b->prev && b->prev->free) { blk_t *q = b->prev; q->size += sizeof(blk_t) + b->size; q->next = b->next; if(b->next) b->next->prev = q; }
}

void *calloc(size_t a, size_t b)
{
	void *p = malloc(a * b);
	if(p) memset(p, 0, a * b);
	return p;
}

void *realloc(void *p, size_t n)
{
	blk_t *b; void *q;
	if(!p) return malloc(n);
	b = (blk_t *)p - 1;
	if(b->size >= n) return p;
	q = malloc(n);
	if(q) { memcpy(q, p, b->size); free(p); }
	return q;
}

void *mem_alloc_stack(size_t n) { return malloc(n); }
