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
int strncasecmp(const char *, const char *, unsigned long);
int strcasecmp(const char *, const char *);
#define strnicmp(a, b, n) strncasecmp((a), (b), (n))
#define stricmp(a, b) strcasecmp((a), (b))
#endif
