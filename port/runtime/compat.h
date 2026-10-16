/* Host compatibility shim: things the SDK's DJGPP headers define that the host libc does not.
   Values copied from original/sdk/include/MATH.H and FCNTL.H. */
#ifndef PORT_COMPAT_H
#define PORT_COMPAT_H
#include <math.h>
#ifndef M_TWOPI
#define M_TWOPI  6.28318530717958647692
#endif
#ifndef M_3PI_2
#define M_3PI_2  4.71238898038468985769
#endif
#ifndef O_BINARY
#define O_BINARY 0           /* DJGPP text/binary distinction; no-op on POSIX */
#endif
/* DJGPP's case-insensitive compares (the game links its own copies; see 0x8014f310).
   Declared rather than pulled from <string.h>: some SDK modules declare their own memset. */
#ifdef _WIN32
/* MinGW's string.h declares these four itself (with size_t); declare them the same way here
   without including it, because some SDK modules declare their own memset/strlen */
int strncasecmp(const char *, const char *, unsigned int);
int strcasecmp(const char *, const char *);
int strnicmp(const char *, const char *, unsigned int);
int stricmp(const char *, const char *);
#else
int strncasecmp(const char *, const char *, unsigned long);
int strcasecmp(const char *, const char *);
#define strnicmp(a, b, n) strncasecmp((a), (b), (n))
#define stricmp(a, b) strcasecmp((a), (b))
#endif
/* The game's heap must lie below 4 GB (port/runtime/mem.c). On macOS each library binds to its
   own malloc, so defining malloc/free in the executable only affects the game's code. On Linux a
   definition in the executable would interpose on every shared library (Mesa, SDL, libLLVM), and
   memory they get from libc's other allocators would reach the arena's free; there the arena
   functions are renamed instead, for the C code that includes this header. */
#if !defined(__APPLE__) && !defined(__cplusplus)
#include <stdlib.h>
void	*port_malloc(size_t), port_free(void *), *port_calloc(size_t, size_t), *port_realloc(void *, size_t);
#define malloc	port_malloc
#define free	port_free
#define calloc	port_calloc
#define realloc	port_realloc
#endif
/* Windows (MinGW): the C library's file calls cannot be replaced by defining them in the
   executable as the HAL does on macOS and Linux; the Windows link wraps them instead
   (-Wl,--wrap=open ...: the HAL defines __wrap_open and calls __real_open), which leaves struct
   members such as obj->remove alone. Here only GOOSE's sleep(), which would clash with the
   CRT's, is renamed, and mkdir loses its mode argument. */
#if defined(_WIN32) && !defined(__cplusplus)
int	_mkdir(const char *);
#define sleep(...)	goose_sleep(__VA_ARGS__)
#define mkdir(p, ...)	_mkdir(p)
#endif
#endif
