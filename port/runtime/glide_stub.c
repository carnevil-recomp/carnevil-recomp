/* Glide entry points the Seattle build of CarnEvil never reaches (see render_gl.c for the rest):
   guTexAllocateMemory/guTexDownloadMipMap serve only create_dtexture (texture.c, the .3df path),
   which nothing calls (every texture goes through create_texture -> guLoadTextureDirect);
   grDrawSprite/grVertexLayout are in the library's VEGAS branches only. */
#include <stdio.h>
void guTexDownloadMipMap(unsigned int h, const void *src, const void *tbl) { (void)h; (void)src; (void)tbl; }
int  guTexAllocateMemory(void) { return -1; }
void grDrawSprite(float *v) { (void)v; }
void grVertexLayout(int a, int b, int c) { (void)a; (void)b; (void)c; }
