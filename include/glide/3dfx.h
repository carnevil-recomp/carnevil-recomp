/*
 * Host replacement for the SDK's glide/3dfx.h: the original typedefs FxU32/FxI32/FxColor_t as
 * `long`, which is 64 bits on LP64 hosts and would change every Glide struct layout (Gu3dfHeader,
 * texture nodes, vertices). The MIPS build never sees this file: its include path lists the SDK
 * first; the host build lists include/ first (see Makefile HOSTCFLAGS).
 */
#ifndef __3DFX_H__
#define __3DFX_H__
#include <stdint.h>
typedef uint8_t		FxU8;
typedef int8_t		FxI8;
typedef uint16_t	FxU16;
typedef int16_t		FxI16;
typedef int32_t		FxI32;
typedef uint32_t	FxU32;
typedef int		FxBool;
typedef float		FxFloat;
typedef double		FxDouble;
typedef uint32_t	FxColor_t;
typedef struct { float r, g, b, a; } FxColor4;
#define FXTRUE    1
#define FXFALSE   0
#define FXUNUSED( a ) ( (a) = (a) )
#define FXBIT( i )    ( 1 << (i) )
#define FX_ENTRY extern
#define FX_CALL
#endif
