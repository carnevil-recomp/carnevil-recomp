/* Host self-test: the same src/ modules compiled natively with clang.
   crc(): must reproduce the GAME.EXE header word the BIOS checks.
   fsin()/fcos(): must agree with libm within float tolerance. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "types.h"
u32 crc(unsigned char *data, int len);
float fsin(float t); float fcos(float t);
int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "original/partition/GAME.EXE", "rb");
    if (!f) { perror("GAME.EXE"); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *d = malloc(n); fread(d, 1, n, f); fclose(f);
    unsigned long stored = d[0] | d[1] << 8 | d[2] << 16 | (unsigned long)d[3] << 24;
    unsigned long got = crc(d + 4, (int)(n - 4)) & 0xffffffffUL;
    printf("crc: header %08lx computed %08lx  %s\n", stored, got, stored == got ? "OK" : "MISMATCH");
    float maxerr = 0; for (int i = 0; i < 10000; i++) { float t = i * 0.001f;
        float e1 = fabsf(fsin(t) - sinf(t)), e2 = fabsf(fcos(t) - cosf(t)); if (e1 > maxerr) maxerr = e1; if (e2 > maxerr) maxerr = e2; }
    printf("fsin/fcos vs libm over [0,10): max abs error %.3g  %s\n", maxerr, maxerr < 2e-6f ? "OK" : "SUSPICIOUS");
    return 0;
}
