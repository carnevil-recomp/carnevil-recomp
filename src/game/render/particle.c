/*
 * particle.c - a single textured triangle at a world point (0x800ecd20, from the file at
 * 0x800ec540-0x800ed030 whose other functions are not decompiled yet). The blood drops and
 * sparks draw through it.
 */
#include <glide/glide.h>
#include "camera.h"

extern float	get_screen_pos(float *, float *);
extern void	func_800d3528(MidVertexWithTA *, MidVertexWithTA *, MidVertexWithTA *);	/* x, y, q, s/w, t/w, a with the depth gradients */

/* 0x800ecd20: the triangle from the point's screen position, size * 1/z * proj_x on a side
   (snapped to quarter pixels), with the whole texture across it and the given alpha */
void draw_particle(float *pos, float size, float alpha)
{
	MidVertexWithTA	a, b, c;
	float		scr[3], s;

	if(get_screen_pos(pos, scr) == -1.0f)
	{
		return;
	}
	s = size * scr[2] * proj_x;
	s += 2097152.0f;
	s -= 2097152.0f;
	a.a = b.a = c.a = alpha;
	a.oow = b.oow = c.oow = scr[2];
	a.x = scr[0];
	c.x = scr[0];
	a.sow = scr[2] * 0.0f;
	a.tow = scr[2] * 255.0f;
	b.sow = scr[2] * 255.0f;
	b.tow = scr[2] * 255.0f;
	a.y = scr[1];
	b.y = scr[1];
	c.sow = scr[2] * 0.0f;
	c.tow = scr[2] * 0.0f;
	b.x = scr[0] + s;
	c.y = scr[1] + s;
	func_800d3528(&a, &b, &c);
}
