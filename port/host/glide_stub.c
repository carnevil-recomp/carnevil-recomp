/*
 * glide_stub.c - the 3dfx Glide entry points the library calls, as no-ops that log their
 * first use. The real renderer (OpenGL) replaces this file; the Voodoo DMA path (dma_start)
 * becomes "present the frame".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hal.h"

#define STUB(name) static int seen_##name; if(!seen_##name) { seen_##name = 1; fprintf(stderr, "[glide] %s (stub)\n", #name); }

void grSstVidMode(int which, void *timing) { (void)which; (void)timing; STUB(grSstVidMode) }
void grGlideInit(void) { STUB(grGlideInit) }
int  grSstQueryHardware(int *hw) { (void)hw; STUB(grSstQueryHardware) return 1; }
void grSstSelect(int n) { (void)n; }
int  grSstOpen(int a, int b, int c, int d, int e, int f) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; STUB(grSstOpen) return 1; }
void grBufferClear(unsigned int color, int alpha, int depth) { (void)color;(void)alpha;(void)depth; }
void grBufferSwap(int interval) { (void)interval; }
void grTexCombineFunction(int tmu, int fn) { (void)tmu;(void)fn; }
void guColorCombineFunction(int fn) { (void)fn; }
void guAlphaSource(int s) { (void)s; }
void guTexSource(int tmu, int addr, int even, void *info) { (void)tmu;(void)addr;(void)even;(void)info; }
void grDepthBufferMode(int m) { (void)m; }
void grDepthBufferFunction(int f) { (void)f; }
void grDepthMask(int m) { (void)m; }
void grDepthBiasLevel(float l) { (void)l; }
void grAlphaBlendFunction(int a, int b, int c, int d) { (void)a;(void)b;(void)c;(void)d; }
void grAlphaTestFunction(int f) { (void)f; }
void grChromakeyMode(int m) { (void)m; }
void grChromakeyValue(unsigned int v) { (void)v; }
void grConstantColorValue(unsigned int v) { (void)v; }
void grTexClampMode(int tmu, int s, int t) { (void)tmu;(void)s;(void)t; }
void grTexFilterMode(int tmu, int min, int mag) { (void)tmu;(void)min;(void)mag; }
void grDrawTriangleDma(void *a, void *b, void *c) { (void)a;(void)b;(void)c; }
void grFreeTexture(void *tn) { (void)tn; }
void guTexMemReset(void) { STUB(guTexMemReset) }
void *guLoadTextureDirect(const char *name, int id, int a, int b) { (void)id;(void)a;(void)b; fprintf(stderr, "[glide] guLoadTextureDirect(%s): no texture memory yet\n", name); return 0; }
void *guLoadTextureDirectFromMemory(void *p, int id, int a, int b) { (void)p;(void)id;(void)a;(void)b; STUB(guLoadTextureDirectFromMemory) return 0; }
int  gu3dfGetInfo(const char *name, void *info) { (void)info; fprintf(stderr, "[glide] gu3dfGetInfo(%s): .3df loading not implemented yet\n", name); return 0; }
int  gu3dfLoad(const char *name, void *info) { (void)name;(void)info; return 0; }
unsigned int get_mm_frag(int handle) { (void)handle; return 0; }
unsigned int get_tmu_fragmentation(void) { return 0; }
void show_tmu_map(void) { fprintf(stderr, "[glide] show_tmu_map (stub)\n"); }
void dma_start(void)		/* the Galileo DMA kick that sends the frame's commands to the Voodoo */
{
	static unsigned long n;
	if(++n == 1) fprintf(stderr, "[glide] dma_start: frames are not rendered yet\n");
}
int func_80157004(void) { return hal_refresh_hz(); }	/* Glide: current vertical refresh */
