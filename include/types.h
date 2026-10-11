/* Fixed-width types shared by the matching build (SN GCC 2.7.2, ILP32: long is 32 bits) and
   the host port (LP64 on macOS/Linux: long is 64 bits). Game and SDK sources imported into
   src/ use these names instead of long/unsigned long so both builds mean the same thing. */
#ifndef CARNEVIL_TYPES_H
#define CARNEVIL_TYPES_H
#ifdef PORT_HOST
#include <stdint.h>
typedef uint8_t  u8;  typedef int8_t  s8;
typedef uint16_t u16; typedef int16_t s16;
typedef uint32_t u32; typedef int32_t s32;
typedef uint64_t u64; typedef int64_t s64;
#else
typedef unsigned char  u8;  typedef signed char s8;
typedef unsigned short u16; typedef short       s16;
typedef unsigned long  u32; typedef long        s32;
typedef unsigned long long u64; typedef long long s64;
#endif
#endif
