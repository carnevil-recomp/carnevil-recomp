/*
 * hostctx.h - the port's coroutine switch, for main.c (the GOOSE main loop on its own stack)
 * and sched.c (one context per GOOSE process). POSIX hosts use ucontext, as before; Windows
 * uses fibers, its native equivalent. The calls mirror ucontext's:
 *   hctx_make(c, fn, stack, size, link)  prepare c to run fn on the given stack; when fn
 *                                        returns, continue at link (uc_link)
 *   hctx_swap(save, to)                  save the current context in save, run to
 *   hctx_set(to)                         run to; the current context is abandoned
 * On Windows the stack argument is ignored (a fiber owns its stack); a re-made context frees
 * its old fiber, which is never the one running at that point.
 */
#ifndef PORT_HOSTCTX_H
#define PORT_HOSTCTX_H
#include <stddef.h>
#ifdef _WIN32
#include <windows.h>
typedef struct hostctx {
	void		*fiber;
	void		(*fn)(void);
	struct hostctx	*link;
} hostctx_t;
static void WINAPI hctx_trampoline(void *arg)
{
	hostctx_t	*c = (hostctx_t *)arg;
	c->fn();
	if(c->link) SwitchToFiber(c->link->fiber);	/* uc_link; a fiber function must not return */
	for(;;) SwitchToFiber(c->link->fiber);
}
static inline void hctx_host(hostctx_t *self)			/* make the calling thread a fiber once */
{
	if(!self->fiber) self->fiber = IsThreadAFiber() ? GetCurrentFiber() : ConvertThreadToFiber(0);
}
static inline void hctx_make(hostctx_t *c, void (*fn)(void), void *stack, size_t size, hostctx_t *link)
{
	(void)stack;
	if(c->fiber) DeleteFiber(c->fiber);
	c->fn = fn; c->link = link;
	c->fiber = CreateFiber(size, hctx_trampoline, c);
}
static inline void hctx_swap(hostctx_t *save, hostctx_t *to) { save->fiber = GetCurrentFiber(); SwitchToFiber(to->fiber); }
static inline void hctx_set(hostctx_t *to) { SwitchToFiber(to->fiber); }
#else
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif
#include <ucontext.h>
typedef struct hostctx { ucontext_t uc; } hostctx_t;
static inline void hctx_host(hostctx_t *self) { (void)self; }
static inline void hctx_make(hostctx_t *c, void (*fn)(void), void *stack, size_t size, hostctx_t *link)
{
	getcontext(&c->uc);
	c->uc.uc_stack.ss_sp = stack;
	c->uc.uc_stack.ss_size = size;
	c->uc.uc_link = link ? &link->uc : 0;
	makecontext(&c->uc, fn, 0);
}
static inline void hctx_swap(hostctx_t *save, hostctx_t *to) { swapcontext(&save->uc, &to->uc); }
static inline void hctx_set(hostctx_t *to) { setcontext(&to->uc); }
#endif
#endif
