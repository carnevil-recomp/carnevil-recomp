/*
 * clip.h - the world-space triangle clipper (src/game/clip.c, 0x8010bf30-0x8010d814). A
 * cvert_t is a textured, alpha vertex: in world or view space z is the depth, after
 * projection it holds 1/z and u, v are divided by z (the layout the triangle set-up at
 * 0x800d3528 reads: x, y, q, s/w, t/w, a).
 */
#ifndef CLIP_H
#define CLIP_H

typedef struct cvert
{
	float	x, y, z;
	float	u, v;
	float	a;
} cvert_t;

float	draw_world_tri(cvert_t *a, cvert_t *b, cvert_t *c);
void	clip_screen_tri(cvert_t *a, cvert_t *b, cvert_t *c);
void	clip_vertex_y(cvert_t *out, float y, cvert_t *from, cvert_t *to);
void	clip_vertex_x(cvert_t *out, float x, cvert_t *from, cvert_t *to);
void	clip_vertex_z(cvert_t *out, float z, cvert_t *from, cvert_t *to);

#endif
