/*
 * draw2d.c - the quad drawers (0x800d90c8-0x800db144): screen-space quads built from
 * MidVertex and handed to the game's triangle setup (draw_tri, 0x800d1750, which writes
 * Voodoo commands into the DMA list), and the world-space billboards the world sprites are
 * drawn with. draw_quad is 0x800d90c8.
 */
#include <glide/glide.h>
#include "camera.h"
#include "sprites.h"
#include "clip.h"

extern void	guAlphaSource(int);
extern void	draw_tri(MidVertex *, MidVertex *, MidVertex *);
extern void	draw_tri_alpha(MidVertexWithTA *, MidVertexWithTA *, MidVertexWithTA *);	/* 0x800d2664 */
extern void	func_800d326c(MidVertex *, MidVertex *, MidVertex *);	/* a textured triangle with the depth gradients */
extern void	rot_matrix_zyx(float *, float *), rot_matrix_yxz(float *, float *);
extern void	mat_mul(float *, float *, float *), mat_vec_mul(float *, float *, float *);
extern float	project_point(float *, float *);			/* vecmat.c */
extern int	printf(const char *, ...);

/* centred at (x, y), w by h pixels, texture coordinates 0..u, 0..v (top row at v); w = 1.1 */
void draw_quad(float x, float y, float w, float h, float u, float v)
{
	MidVertex	a, b, c, d;
	float		oow;

	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	w *= 0.5f;
	h *= 0.5f;
	oow = 0.90909088f;
	v *= oow;
	u *= oow;
	a.oow = oow;
	b.oow = oow;
	c.oow = oow;
	d.oow = oow;
	a.x = x - w;
	a.y = y - h;
	a.sow = 0.0f;
	a.tow = v;
	b.x = x + w;
	b.y = y - h;
	b.sow = u;
	b.tow = v;
	c.x = x - w;
	c.y = y + h;
	c.sow = 0.0f;
	c.tow = 0.0f;
	d.x = x + w;
	d.y = y + h;
	d.sow = u;
	d.tow = 0.0f;
	draw_tri(&a, &b, &d);
	draw_tri(&a, &d, &c);
}

/* 0x800d91dc: draw_quad with the texture mirrored left to right */
void draw_quad_mirrored(float x, float y, float w, float h, float u, float v)
{
	MidVertex	a, b, c, d;
	float		oow;

	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	w *= 0.5f;
	h *= 0.5f;
	oow = 0.90909088f;
	u *= oow;
	v *= oow;
	a.oow = oow;
	b.oow = oow;
	c.oow = oow;
	d.oow = oow;
	a.x = x - w;
	a.y = y - h;
	a.sow = u;
	a.tow = v;
	b.x = x + w;
	b.y = y - h;
	b.sow = 0.0f;
	b.tow = v;
	c.x = x - w;
	c.y = y + h;
	c.sow = u;
	c.tow = 0.0f;
	d.x = x + w;
	d.y = y + h;
	d.sow = 0.0f;
	d.tow = 0.0f;
	draw_tri(&a, &b, &d);
	draw_tri(&a, &d, &c);
}

/* 0x800d92f0: draw_quad with an alpha at every corner */
void draw_quad_alpha(float x, float y, float w, float h, float u, float v, float alpha)
{
	MidVertexWithTA	a, b, c, d;
	float		oow;

	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA);
	w *= 0.5f;
	h *= 0.5f;
	oow = 0.90909088f;
	v *= oow;
	u *= oow;
	a.oow = oow;
	b.oow = oow;
	c.oow = oow;
	d.oow = oow;
	a.a = alpha;
	b.a = alpha;
	c.a = alpha;
	d.a = alpha;
	a.x = x - w;
	a.y = y - h;
	a.sow = 0.0f;
	a.tow = v;
	b.x = x + w;
	b.y = y - h;
	b.sow = u;
	b.tow = v;
	c.x = x - w;
	c.y = y + h;
	c.sow = 0.0f;
	c.tow = 0.0f;
	d.x = x + w;
	d.y = y + h;
	d.sow = u;
	d.tow = 0.0f;
	draw_tri_alpha(&a, &b, &d);
	draw_tri_alpha(&a, &d, &c);
}

/* 0x800d9420: draw_quad_alpha mirrored (it selects the plain texture alpha, unlike the others) */
void draw_quad_alpha_mirrored(float x, float y, float w, float h, float u, float v, float alpha)
{
	MidVertexWithTA	a, b, c, d;
	float		oow;

	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	w *= 0.5f;
	h *= 0.5f;
	oow = 0.90909088f;
	u *= oow;
	v *= oow;
	a.oow = oow;
	b.oow = oow;
	c.oow = oow;
	d.oow = oow;
	a.a = alpha;
	b.a = alpha;
	c.a = alpha;
	d.a = alpha;
	a.x = x - w;
	a.y = y - h;
	a.sow = u;
	a.tow = v;
	b.x = x + w;
	b.y = y - h;
	b.sow = 0.0f;
	b.tow = v;
	c.x = x - w;
	c.y = y + h;
	c.sow = u;
	c.tow = 0.0f;
	d.x = x + w;
	d.y = y + h;
	d.sow = 0.0f;
	d.tow = 0.0f;
	draw_tri_alpha(&a, &b, &d);
	draw_tri_alpha(&a, &d, &c);
}

/* ---- billboards: a w by h quad at pos, spun by rot and then turned to face the camera ---- */

/* turn the four corners (about the origin) into world space */
static void billboard_corners(float *pos, float *rot, float *c0, float *c1, float *c2, float *c3)
{
	float	m[9], cam[9];

	rot_matrix_zyx(m, rot);
	rot_matrix_yxz(cam, &camera_rx);
	mat_mul(cam, m, m);
	mat_vec_mul(c0, m, c0);
	mat_vec_mul(c1, m, c1);
	mat_vec_mul(c2, m, c2);
	mat_vec_mul(c3, m, c3);
	c0[0] += pos[0];
	c1[0] += pos[0];
	c2[0] += pos[0];
	c3[0] += pos[0];
	c0[1] += pos[1];
	c1[1] += pos[1];
	c2[1] += pos[1];
	c3[1] += pos[1];
	c0[2] += pos[2];
	c1[2] += pos[2];
	c2[2] += pos[2];
	c3[2] += pos[2];
}

/* project the corners and draw them with the depth gradients; a corner that fails to project
   drops the quad with a message */
static void billboard_draw(float *c0, float *c1, float *c2, float *c3, float u, float v)
{
	MidVertex	v0, v1, v2, v3;

	if(project_point(c0, (float *)&v0) == -1.0f || project_point(c1, (float *)&v1) == -1.0f
	   || project_point(c2, (float *)&v2) == -1.0f || project_point(c3, (float *)&v3) == -1.0f)
	{
		printf("CLIP\n");
		return;
	}
	v0.sow = u * v0.oow;
	v0.tow = v * v0.oow;
	v1.sow = 0.0f * v1.oow;
	v1.tow = v * v1.oow;
	v2.sow = u * v2.oow;
	v2.tow = 0.0f * v2.oow;
	v3.sow = 0.0f * v2.oow;
	v3.tow = 0.0f * v2.oow;
	func_800d326c(&v0, &v1, &v3);
	func_800d326c(&v0, &v3, &v2);
}

/* hand the corners to the clipper with the texture corners and a common alpha */
static void billboard_draw_alpha(float *c0, float *c1, float *c2, float *c3, float u, float v, float alpha)
{
	cvert_t	v0, v1, v2, v3, t0, t3;

	v0.x = c0[0]; v0.y = c0[1]; v0.z = c0[2]; v0.u = u;    v0.v = v;    v0.a = alpha;
	v1.x = c1[0]; v1.y = c1[1]; v1.z = c1[2]; v1.u = 0.0f; v1.v = v;    v1.a = alpha;
	v2.x = c2[0]; v2.y = c2[1]; v2.z = c2[2]; v2.u = u;    v2.v = 0.0f; v2.a = alpha;
	v3.x = c3[0]; v3.y = c3[1]; v3.z = c3[2]; v3.u = 0.0f; v3.v = 0.0f; v3.a = alpha;
	t0 = v0;
	t3 = v3;
	draw_world_tri(&v0, &v1, &v3);
	draw_world_tri(&t0, &v2, &t3);
}

/* 0x800da0f0: centred on pos, w by h either side */
void draw_billboard(float *pos, float *rot, float w, float h, float u, float v)
{
	float	c0[3], c1[3], c2[3], c3[3];

	c0[0] = -w; c0[1] = -h; c0[2] = 0.0f;
	c1[0] = w;  c1[1] = -h; c1[2] = 0.0f;
	c2[0] = -w; c2[1] = h;  c2[2] = 0.0f;
	c3[0] = w;  c3[1] = h;  c3[2] = 0.0f;
	billboard_corners(pos, rot, c0, c1, c2, c3);
	billboard_draw(c0, c1, c2, c3, u, v);
}

/* 0x800da3d0: as draw_billboard, through the clipper with an alpha */
void draw_billboard_alpha(float *pos, float *rot, float w, float h, float alpha, float u, float v)
{
	float	c0[3], c1[3], c2[3], c3[3];

	c0[0] = -w; c0[1] = -h; c0[2] = 0.0f;
	c1[0] = w;  c1[1] = -h; c1[2] = 0.0f;
	c2[0] = -w; c2[1] = h;  c2[2] = 0.0f;
	c3[0] = w;  c3[1] = h;  c3[2] = 0.0f;
	billboard_corners(pos, rot, c0, c1, c2, c3);
	billboard_draw_alpha(c0, c1, c2, c3, u, v, alpha);
}

/* 0x800da668: standing on pos, from 0 to 2h up */
void draw_billboard_raised(float *pos, float *rot, float w, float h, float u, float v)
{
	float	c0[3], c1[3], c2[3], c3[3];

	c0[0] = -w; c0[1] = 0.0f;  c0[2] = 0.0f;
	c1[0] = w;  c1[1] = 0.0f;  c1[2] = 0.0f;
	c2[0] = -w; c2[1] = h + h; c2[2] = 0.0f;
	c3[0] = w;  c3[1] = h + h; c3[2] = 0.0f;
	billboard_corners(pos, rot, c0, c1, c2, c3);
	billboard_draw(c0, c1, c2, c3, u, v);
}

/* 0x800da94c: from half a height below pos to one and a half above, through the clipper */
void draw_billboard_raised_alpha(float *pos, float *rot, float w, float h, float alpha, float u, float v)
{
	float	c0[3], c1[3], c2[3], c3[3];

	c0[0] = -w; c0[1] = h * -0.5;  c0[2] = 0.0f;
	c1[0] = w;  c1[1] = h * -0.5f; c1[2] = 0.0f;
	c2[0] = -w; c2[1] = h * 1.5f;  c2[2] = 0.0f;
	c3[0] = w;  c3[1] = h * 1.5f;  c3[2] = 0.0f;
	billboard_corners(pos, rot, c0, c1, c2, c3);
	billboard_draw_alpha(c0, c1, c2, c3, u, v, alpha);
}
