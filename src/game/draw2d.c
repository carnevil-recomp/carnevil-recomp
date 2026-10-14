/*
 * draw2d.c - the quad drawers (0x800d90c8-0x800db144): screen-space quads built from
 * MidVertex and handed to the game's triangle setup (draw_tri, 0x800d1750, which writes
 * Voodoo commands into the DMA list), and the world-space billboards the world sprites are
 * drawn with, the point sprites (a screen-aligned quad at a world point, sized by its
 * distance) and the life bar. draw_quad is 0x800d90c8.
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
extern float	project_point(float *, float *), get_screen_pos(float *, float *);	/* vecmat.c */
extern void	rot_z(float *, float *);
extern void	draw_tri_flat(void *, void *, void *);			/* 0x800d2174: a constant-colour triangle */

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
	d.a = alpha;
	c.a = alpha;
	b.a = alpha;
	a.a = alpha;
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
	d.a = alpha;
	c.a = alpha;
	b.a = alpha;
	a.a = alpha;
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

/* the point sprites: a textured quad centred on a world point, size units wide at a
   distance of one (so size * 1/w * the projection scale on the screen), skipped when off
   screen or over 128 pixels tall; the sizes are snapped to the triangle setup's precision */
#define SNAP	2097152.0f

/* 0x800d9550: white, the texture upright */
void draw_point_sprite(int key, float *pos, float size, float alpha)
{
	MidVertexWithTA	a, b, c, d;
	float		scr[3];
	float		hh, hw, s;

	if(get_screen_pos(pos, scr) == -1.0f)
	{
		return;
	}
	hh = size * scr[2] * proj_y;
	hw = size * scr[2] * proj_x;
	if(hh > 128.0f)
	{
		return;
	}
	guTexSource(find_surface(key)->handle);
	guColorCombineFunction(5);
	guAlphaSource(3);
	grAlphaBlendFunction(1, 5, 4, 0);
	grConstantColorValue(0xffffff);
	hh = hh + SNAP;
	hw = hw + SNAP;
	s = scr[2] * 255.0f;
	hh = hh - SNAP;
	hw = hw - SNAP;
	a.oow = scr[2];
	b.oow = scr[2];
	c.oow = scr[2];
	d.oow = scr[2];
	d.a = alpha;
	c.a = alpha;
	b.a = alpha;
	a.a = alpha;
	a.x = scr[0] - hw;
	a.y = scr[1] - hh;
	a.sow = 0.0f * scr[2];
	a.tow = 0.0f * scr[2];
	b.x = scr[0] + hw;
	b.y = scr[1] - hh;
	b.sow = s;
	b.tow = 0.0f * scr[2];
	c.x = scr[0] - hw;
	c.y = scr[1] + hh;
	c.sow = 0.0f * scr[2];
	c.tow = s;
	d.x = scr[0] + hw;
	d.y = scr[1] + hh;
	d.sow = s;
	d.tow = s;
	draw_tri_alpha(&a, &b, &d);
	draw_tri_alpha(&a, &d, &c);
	grAlphaBlendFunction(4, 0, 4, 0);
}

/* 0x800d970c: in a constant colour, alpha tested, the texture's first row at the bottom */
void draw_point_sprite_color(int key, float *pos, float size, float alpha, unsigned int color)
{
	MidVertexWithTA	a, b, c, d;
	float		scr[3];
	float		hh, hw, s;

	if(get_screen_pos(pos, scr) == -1.0f)
	{
		return;
	}
	hh = size * scr[2] * proj_y;
	hw = size * scr[2] * proj_x;
	if(hh > 128.0f)
	{
		return;
	}
	guTexSource(find_surface(key)->handle);
	guColorCombineFunction(5);
	guAlphaSource(3);
	grAlphaBlendFunction(1, 5, 4, 0);
	grConstantColorValue(color);
	grChromakeyMode(0);
	grAlphaTestFunction(4);
	grAlphaTestReferenceValue(5);
	hh = hh + SNAP;
	hw = hw + SNAP;
	s = scr[2] * 255.0f;
	hh = hh - SNAP;
	hw = hw - SNAP;
	a.oow = scr[2];
	b.oow = scr[2];
	c.oow = scr[2];
	d.oow = scr[2];
	d.a = alpha;
	c.a = alpha;
	b.a = alpha;
	a.a = alpha;
	a.x = scr[0] - hw;
	a.y = scr[1] - hh;
	a.sow = 0.0f * scr[2];
	a.tow = s;
	b.x = scr[0] + hw;
	b.y = scr[1] - hh;
	b.sow = s;
	b.tow = s;
	c.x = scr[0] - hw;
	c.y = scr[1] + hh;
	c.sow = 0.0f * scr[2];
	c.tow = 0.0f * scr[2];
	d.x = scr[0] + hw;
	d.y = scr[1] + hh;
	d.sow = s;
	d.tow = 0.0f * scr[2];
	draw_tri_alpha(&a, &b, &d);
	draw_tri_alpha(&a, &d, &c);
	grAlphaBlendFunction(4, 0, 4, 0);
	grAlphaTestReferenceValue(0);
}

/* 0x800d98e8: the texture's top half (rows 1 to 127) at half the height */
void draw_point_sprite_half(int key, float *pos, float size, float alpha, unsigned int color)
{
	MidVertexWithTA	a, b, c, d;
	float		scr[3];
	float		hh, hw, s, t;

	if(get_screen_pos(pos, scr) == -1.0f)
	{
		return;
	}
	hh = size * scr[2] * proj_y * 0.5f;
	hw = size * scr[2] * proj_x;
	if(hh > 128.0f)
	{
		return;
	}
	guTexSource(find_surface(key)->handle);
	guColorCombineFunction(5);
	guAlphaSource(3);
	grAlphaBlendFunction(1, 5, 4, 0);
	grConstantColorValue(color);
	grChromakeyMode(0);
	grAlphaTestFunction(4);
	grAlphaTestReferenceValue(5);
	hh = hh + SNAP;
	hw = hw + SNAP;
	t = 127.0f * scr[2];
	hh = hh - SNAP;
	s = scr[2] * 255.0f;
	hw = hw - SNAP;
	a.oow = scr[2];
	a.sow = scr[2];
	b.oow = scr[2];
	c.oow = scr[2];
	c.sow = scr[2];
	c.tow = scr[2];
	d.oow = scr[2];
	d.tow = scr[2];
	d.a = alpha;
	c.a = alpha;
	b.a = alpha;
	a.a = alpha;
	a.x = scr[0] - hw;
	a.y = scr[1] - hh;
	a.tow = t;
	b.x = scr[0] + hw;
	b.y = scr[1] - hh;
	b.sow = s;
	b.tow = t;
	c.x = scr[0] - hw;
	c.y = scr[1] + hh;
	d.x = scr[0] + hw;
	d.y = scr[1] + hh;
	d.sow = s;
	draw_tri_alpha(&a, &b, &d);
	draw_tri_alpha(&a, &d, &c);
	grAlphaBlendFunction(4, 0, 4, 0);
	grAlphaTestReferenceValue(0);
}

/* 0x800d9ad4: draw_point_sprite_color over everything (the depth test passes always) */
void draw_point_sprite_over(int key, float *pos, float size, float alpha, unsigned int color)
{
	MidVertexWithTA	a, b, c, d;
	float		scr[3];
	float		hh, hw, s;

	if(get_screen_pos(pos, scr) == -1.0f)
	{
		return;
	}
	hh = size * scr[2] * proj_y;
	hw = size * scr[2] * proj_x;
	if(hh > 128.0f)
	{
		return;
	}
	guTexSource(find_surface(key)->handle);
	guColorCombineFunction(5);
	guAlphaSource(3);
	grAlphaBlendFunction(1, 5, 4, 0);
	grConstantColorValue(color);
	grChromakeyMode(0);
	grAlphaTestFunction(4);
	grAlphaTestReferenceValue(5);
	grDepthBufferFunction(7);
	hh = hh + SNAP;
	hw = hw + SNAP;
	s = scr[2] * 255.0f;
	hh = hh - SNAP;
	hw = hw - SNAP;
	a.oow = scr[2];
	b.oow = scr[2];
	c.oow = scr[2];
	d.oow = scr[2];
	d.a = alpha;
	c.a = alpha;
	b.a = alpha;
	a.a = alpha;
	a.x = scr[0] - hw;
	a.y = scr[1] - hh;
	a.sow = 0.0f * scr[2];
	a.tow = s;
	b.x = scr[0] + hw;
	b.y = scr[1] - hh;
	b.sow = s;
	b.tow = s;
	c.x = scr[0] - hw;
	c.y = scr[1] + hh;
	c.sow = 0.0f * scr[2];
	c.tow = 0.0f * scr[2];
	d.x = scr[0] + hw;
	d.y = scr[1] + hh;
	d.sow = s;
	d.tow = 0.0f * scr[2];
	draw_tri_alpha(&a, &b, &d);
	draw_tri_alpha(&a, &d, &c);
	grAlphaBlendFunction(4, 0, 4, 0);
	grAlphaTestReferenceValue(0);
	grDepthBufferFunction(3);
}

/* 0x800d9cc0: the flame: the quad hangs from a quarter of its height above the point to
   three quarters below it (it is drawn at the group's pivot and burns upward), the
   texture upright */
void draw_flame_sprite(int key, float *pos, float size, float alpha, unsigned int color)
{
	MidVertexWithTA	a, b, c, d;
	float		scr[3];
	float		hh, hw, s, top, bottom;

	if(get_screen_pos(pos, scr) == -1.0f)
	{
		return;
	}
	hh = size * scr[2] * proj_y;
	hw = size * scr[2] * proj_x;
	if(hh > 128.0f)
	{
		return;
	}
	guTexSource(find_surface(key)->handle);
	guColorCombineFunction(5);
	guAlphaSource(3);
	grAlphaBlendFunction(1, 5, 4, 0);
	grConstantColorValue(color);
	grChromakeyMode(0);
	grAlphaTestFunction(4);
	grAlphaTestReferenceValue(5);
	hh = hh + SNAP;
	hw = hw + SNAP;
	s = scr[2] * 255.0f;
	hh = hh - SNAP;
	hw = hw - SNAP;
	top = scr[1] - hh * 1.5f;
	bottom = scr[1] + hh * 0.5f;
	a.oow = scr[2];
	b.oow = scr[2];
	c.oow = scr[2];
	d.oow = scr[2];
	d.a = alpha;
	c.a = alpha;
	b.a = alpha;
	a.a = alpha;
	a.x = scr[0] - hw;
	a.y = top;
	a.sow = 0.0f * scr[2];
	a.tow = 0.0f * scr[2];
	b.x = scr[0] + hw;
	b.y = top;
	b.sow = s;
	b.tow = 0.0f * scr[2];
	c.x = scr[0] - hw;
	c.y = bottom;
	c.sow = 0.0f * scr[2];
	c.tow = s;
	d.x = scr[0] + hw;
	d.y = bottom;
	d.sow = s;
	d.tow = s;
	draw_tri_alpha(&a, &b, &d);
	draw_tri_alpha(&a, &d, &c);
	grAlphaBlendFunction(4, 0, 4, 0);
	grAlphaTestReferenceValue(0);
}

/* 0x800d9eb4: a life bar from x0 to x1 at y: the border (LBARBORD, 14 pixels tall) with
   the bar (LBARBAR, 16 tall, in the given colour) filling value / max of it */
void draw_life_bar(float value, float max, float x0, float x1, float y, unsigned int color)
{
	MidVertexWithTA	a, b, c, d;
	float		oow, xend, s;

	guTexSource(find_surface(0x6008)->handle);
	guColorCombineFunction(4);
	guAlphaSource(2);
	grAlphaBlendFunction(1, 5, 4, 0);
	oow = 0.90909088f;
	a.x = x0;
	a.y = y - 7.0f;
	a.oow = oow;
	a.sow = 0.45454544f;
	a.tow = 28.863636f;
	b.x = x1;
	b.y = y - 7.0f;
	b.oow = oow;
	b.sow = 232.27272f;
	b.tow = 28.863636f;
	c.x = x0;
	c.y = y + 7.0f;
	c.oow = oow;
	c.sow = 0.45454544f;
	c.tow = 0.22727272f;
	d.x = x1;
	d.y = y + 7.0f;
	d.oow = oow;
	d.sow = 232.27272f;
	d.tow = 0.22727272f;
	func_800d326c((MidVertex *)&a, (MidVertex *)&d, (MidVertex *)&b);
	func_800d326c((MidVertex *)&a, (MidVertex *)&c, (MidVertex *)&d);
	guTexSource(find_surface(0x6009)->handle);
	guColorCombineFunction(5);
	grConstantColorValue(color);
	xend = x0 + (x1 - x0) * (value / max);
	s = value / max * 255.0f * oow;
	a.x = x0;
	a.y = y - 8.0f;
	a.oow = oow;
	a.sow = 0.0f;
	a.tow = 29.09091f;
	b.x = xend;
	b.y = y - 8.0f;
	b.oow = oow;
	b.sow = s;
	b.tow = 29.09091f;
	c.x = x0;
	c.y = y + 8.0f;
	c.oow = oow;
	c.sow = 0.0f;
	c.tow = 0.0f;
	d.x = xend;
	d.y = y + 8.0f;
	d.oow = oow;
	d.sow = s;
	d.tow = 0.0f;
	func_800d326c((MidVertex *)&a, (MidVertex *)&d, (MidVertex *)&b);
	func_800d326c((MidVertex *)&a, (MidVertex *)&c, (MidVertex *)&d);
}

/* the constant-colour quads: four bare vertices for the flat triangle at 0x800d2174 */
typedef struct flat_vertex
{
	float	x, y, oow, unused;
} flat_vertex_t;

/* 0x800dac0c: centred on (x, y), w by h */
void draw_flat_quad(float x, float y, float w, float h)
{
	flat_vertex_t	a, b, c, d;

	w = w * 0.5f;
	h = h * 0.5f;
	a.oow = 0.90909088f;
	b.oow = 0.90909088f;
	c.oow = 0.90909088f;
	d.oow = 0.90909088f;
	a.x = x - w;
	a.y = y - h;
	b.x = x + w;
	b.y = y - h;
	c.x = x - w;
	c.y = y + h;
	d.x = x + w;
	d.y = y + h;
	draw_tri_flat(&a, &b, &d);
	draw_tri_flat(&a, &d, &c);
}

/* 0x800dacb0: from x0 to x1, h tall around y (the volume level) */
void draw_flat_quad_span(float x0, float x1, float y, float h)
{
	flat_vertex_t	a, b, c, d;

	h = h * 0.5f;
	a.x = x0;
	b.x = x1;
	c.x = x0;
	d.x = x1;
	a.oow = 0.90909088f;
	b.oow = 0.90909088f;
	c.oow = 0.90909088f;
	d.oow = 0.90909088f;
	a.y = y - h;
	b.y = y - h;
	c.y = y + h;
	d.y = y + h;
	draw_tri_flat(&a, &b, &d);
	draw_tri_flat(&a, &d, &c);
}

/* 0x800dad48: a textured quad turned by angle about its centre (the corners go through
   the vertex's depth slot, which is set afterwards), texture alpha */
void draw_quad_rotated(float x, float y, float w, float h, float u, float v, float angle)
{
	MidVertexWithTA	a, b, c, d;
	float		rot[3], m[9];
	float		hw, hh, oow;

	hw = w * 0.5f;
	hh = h * 0.5f;
	rot[2] = angle;
	rot_z(m, rot);
	a.x = -hw;
	a.y = -hh;
	a.oow = 0.0f;
	b.x = hw;
	b.y = -hh;
	b.oow = 0.0f;
	c.x = -hw;
	c.y = hh;
	c.oow = 0.0f;
	d.x = hw;
	d.y = hh;
	d.oow = 0.0f;
	mat_vec_mul(&a.x, m, &a.x);
	mat_vec_mul(&b.x, m, &b.x);
	mat_vec_mul(&c.x, m, &c.x);
	mat_vec_mul(&d.x, m, &d.x);
	guAlphaSource(2);
	a.x = a.x + x;
	a.y = a.y + y;
	b.x = b.x + x;
	b.y = b.y + y;
	c.x = c.x + x;
	c.y = c.y + y;
	d.x = d.x + x;
	d.y = d.y + y;
	oow = 0.90909088f;
	a.sow = 0.0f;
	v = v * oow;
	c.sow = 0.0f;
	c.tow = 0.0f;
	d.tow = 0.0f;
	u = u * oow;
	a.oow = oow;
	b.oow = oow;
	c.oow = oow;
	d.oow = oow;
	a.tow = v;
	b.sow = u;
	b.tow = v;
	d.sow = u;
	draw_tri((MidVertex *)&a, (MidVertex *)&b, (MidVertex *)&d);
	draw_tri((MidVertex *)&a, (MidVertex *)&d, (MidVertex *)&c);
}

/* 0x800daf3c: draw_quad_rotated with an alpha */
void draw_quad_rotated_alpha(float x, float y, float w, float h, float u, float v, float angle, float alpha)
{
	MidVertexWithTA	a, b, c, d;
	float		rot[3], m[9];
	float		hw, hh, oow;

	hw = w * 0.5f;
	hh = h * 0.5f;
	rot[2] = angle;
	rot_z(m, rot);
	a.x = -hw;
	a.y = -hh;
	a.oow = 0.0f;
	b.x = hw;
	b.y = -hh;
	b.oow = 0.0f;
	c.x = -hw;
	c.y = hh;
	c.oow = 0.0f;
	d.x = hw;
	d.y = hh;
	d.oow = 0.0f;
	mat_vec_mul(&a.x, m, &a.x);
	mat_vec_mul(&b.x, m, &b.x);
	mat_vec_mul(&c.x, m, &c.x);
	mat_vec_mul(&d.x, m, &d.x);
	guAlphaSource(3);
	oow = 0.90909088f;
	v = v * oow;
	d.a = alpha;
	c.a = alpha;
	b.a = alpha;
	a.a = alpha;
	a.x = a.x + x;
	a.y = a.y + y;
	b.x = b.x + x;
	b.y = b.y + y;
	c.x = c.x + x;
	c.y = c.y + y;
	d.x = d.x + x;
	d.y = d.y + y;
	a.oow = oow;
	b.oow = oow;
	c.oow = oow;
	d.oow = oow;
	u = u * oow;
	a.sow = 0.0f;
	a.tow = v;
	b.sow = u;
	b.tow = v;
	c.sow = 0.0f;
	c.tow = 0.0f;
	d.sow = u;
	d.tow = 0.0f;
	draw_tri_alpha(&a, &b, &d);
	draw_tri_alpha(&a, &d, &c);
}
