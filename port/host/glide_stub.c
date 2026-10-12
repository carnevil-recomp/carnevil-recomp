/* Glide entry points not yet given a real implementation (see render_gl.c for the rest) */
#include <stdio.h>
void guTexDownloadMipMap(unsigned int h, const void *src, const void *tbl) { (void)h; (void)src; (void)tbl; }
int  guTexAllocateMemory(void) { return -1; }
void grDrawSprite(float *v) { (void)v; }
void grVertexLayout(int a, int b, int c) { (void)a; (void)b; (void)c; }
