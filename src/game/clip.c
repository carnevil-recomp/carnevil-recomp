/*
 * clip.c - the world-space triangle clipper (0x8010bf30-0x8010d814): a textured alpha
 * triangle given in world space is taken into view space, clipped against the near plane
 * (z = 1.1), projected, clipped to the 400x256 screen one edge at a time and handed to the
 * triangle set-up (0x800d3528). A triangle entirely beyond z = 400 is dropped; there is no far
 * clipping. The vertices are transformed in place, so callers keep copies of the corners
 * they reuse. The world sprites' alpha billboards and the blood effects draw through it.
 */
#include "camera.h"
#include "clip.h"

extern void	func_800d3528(cvert_t *, cvert_t *, cvert_t *);	/* x, y, q, s/w, t/w, a with the depth gradients */

#define NEAR_Z		1.1f
#define FAR_Z		400.0f
#define SCREEN_W	400
#define SCREEN_H	256
#define SNAP		2097152.0f			/* x + 2^21 - 2^21 rounds to a quarter pixel */

/* a view-space vertex onto the screen: 1/z, x and y scaled from the centre, u and v over z */
#define PROJECT(p) \
	(p)->z = 1.0f / (p)->z; \
	(p)->x = (p)->x * (p)->z * proj_x + 200.0f; \
	(p)->y = (p)->y * (p)->z * proj_y + (float)half_height; \
	(p)->u = (p)->u * (p)->z; \
	(p)->v = (p)->v * (p)->z

/* 0x8010bf30: returns 1 when something was drawn, -1 when the triangle was behind the
   near plane or beyond the far one */
float draw_world_tri(cvert_t *a, cvert_t *b, cvert_t *c)
{
	cvert_t	n1, n2;
	float	nx, ny, nz;
	float	m00, m10, m20, m01, m11, m21, m02, m12, m22;
	float	x, y, z;

	nx = camera_neg_pos[0];
	ny = camera_neg_pos[1];
	nz = camera_neg_pos[2];
	m00 = view_matrix[0];
	m10 = view_matrix[3];
	m20 = view_matrix[6];
	m01 = view_matrix[1];
	m11 = view_matrix[4];
	m21 = view_matrix[7];
	m02 = view_matrix[2];
	m12 = view_matrix[5];
	m22 = view_matrix[8];
	x = a->x + nx;
	y = a->y + ny;
	z = a->z + nz;
	a->x = x * m00 + y * m01 + z * m02;
	a->y = x * m10 + y * m11 + z * m12;
	a->z = x * m20 + y * m21 + z * m22;
	x = b->x + nx;
	y = b->y + ny;
	z = b->z + nz;
	b->x = x * m00 + y * m01 + z * m02;
	b->y = x * m10 + y * m11 + z * m12;
	b->z = x * m20 + y * m21 + z * m22;
	x = c->x + nx;
	y = c->y + ny;
	z = c->z + nz;
	c->x = x * m00 + y * m01 + z * m02;
	c->y = x * m10 + y * m11 + z * m12;
	c->z = x * m20 + y * m21 + z * m22;
	if(a->z > FAR_Z && b->z > FAR_Z && c->z > FAR_Z)
	{
		return(-1.0f);
	}
	if(a->z < NEAR_Z)
	{
		if(b->z < NEAR_Z && c->z < NEAR_Z)
		{
			return(-1.0f);
		}
		if(b->z >= NEAR_Z && c->z >= NEAR_Z)
		{
			/* a behind: the quad b, c and the two crossings */
			clip_vertex_z(&n1, NEAR_Z, b, a);
			clip_vertex_z(&n2, NEAR_Z, c, a);
			PROJECT(b);
			PROJECT(c);
			PROJECT(&n1);
			PROJECT(&n2);
			clip_screen_tri(&n1, b, c);
			clip_screen_tri(&n2, &n1, c);
		}
		else if(c->z < NEAR_Z)
		{
			/* only b in front */
			clip_vertex_z(&n1, NEAR_Z, b, a);
			clip_vertex_z(&n2, NEAR_Z, b, c);
			PROJECT(b);
			PROJECT(&n1);
			PROJECT(&n2);
			clip_screen_tri(&n1, b, &n2);
		}
		else if(b->z < NEAR_Z)
		{
			/* only c in front */
			clip_vertex_z(&n1, NEAR_Z, c, b);
			clip_vertex_z(&n2, NEAR_Z, c, a);
			PROJECT(c);
			PROJECT(&n1);
			PROJECT(&n2);
			clip_screen_tri(&n2, &n1, c);
		}
		else
		{
			PROJECT(a);
			PROJECT(b);
			PROJECT(c);
			clip_screen_tri(a, b, c);
		}
	}
	else
	{
		if(b->z < NEAR_Z)
		{
			if(c->z >= NEAR_Z)
			{
				/* b behind */
				clip_vertex_z(&n1, NEAR_Z, a, b);
				clip_vertex_z(&n2, NEAR_Z, c, b);
				PROJECT(a);
				PROJECT(c);
				PROJECT(&n1);
				PROJECT(&n2);
				clip_screen_tri(&n1, c, a);
				clip_screen_tri(&n1, &n2, c);
			}
			else
			{
				/* only a in front */
				clip_vertex_z(&n1, NEAR_Z, a, b);
				clip_vertex_z(&n2, NEAR_Z, a, c);
				PROJECT(a);
				PROJECT(&n1);
				PROJECT(&n2);
				clip_screen_tri(&n1, &n2, a);
			}
		}
		else if(c->z < NEAR_Z)
		{
			/* c behind */
			clip_vertex_z(&n1, NEAR_Z, a, c);
			clip_vertex_z(&n2, NEAR_Z, b, c);
			PROJECT(a);
			PROJECT(b);
			PROJECT(&n1);
			PROJECT(&n2);
			clip_screen_tri(&n2, &n1, a);
			clip_screen_tri(&n2, a, b);
		}
		else
		{
			PROJECT(a);
			PROJECT(b);
			PROJECT(c);
			clip_screen_tri(a, b, c);
		}
	}
	return(1.0f);
}

/* 0x8010cd54: a projected triangle against the bottom, top, left and right edges in turn,
   each piece re-entering for the remaining edges; whole pieces go to the set-up */
void clip_screen_tri(cvert_t *a, cvert_t *b, cvert_t *c)
{
	cvert_t	n1, n2;

	a->x = a->x + SNAP - SNAP;
	a->y = a->y + SNAP - SNAP;
	b->x = b->x + SNAP - SNAP;
	b->y = b->y + SNAP - SNAP;
	c->x = c->x + SNAP - SNAP;
	c->y = c->y + SNAP - SNAP;
	if(a->y < 0)
	{
		if(b->y < 0 && c->y < 0)
		{
			return;
		}
		if(b->y >= 0.0 && c->y >= 0.0)
		{
			clip_vertex_y(&n1, 0, b, a);
			clip_vertex_y(&n2, 0, c, a);
			clip_screen_tri(&n1, b, c);
			clip_screen_tri(&n2, &n1, c);
			return;
		}
		if(c->y < 0.0)
		{
			clip_vertex_y(&n1, 0, b, a);
			clip_vertex_y(&n2, 0, b, c);
			clip_screen_tri(&n1, b, &n2);
			return;
		}
		if(b->y < 0.0)
		{
			clip_vertex_y(&n1, 0, c, b);
			clip_vertex_y(&n2, 0, c, a);
			clip_screen_tri(&n2, &n1, c);
			return;
		}
	}
	else
	{
		if(b->y < 0)
		{
			if(c->y >= 0.0)
			{
				clip_vertex_y(&n1, 0, a, b);
				clip_vertex_y(&n2, 0, c, b);
				clip_screen_tri(&n1, c, a);
				clip_screen_tri(&n1, &n2, c);
			}
			else
			{
				clip_vertex_y(&n1, 0, a, b);
				clip_vertex_y(&n2, 0, a, c);
				clip_screen_tri(&n1, &n2, a);
			}
			return;
		}
		if(c->y < 0)
		{
			clip_vertex_y(&n1, 0, a, c);
			clip_vertex_y(&n2, 0, b, c);
			clip_screen_tri(&n2, &n1, a);
			clip_screen_tri(&n2, a, b);
			return;
		}
	}
	if(a->y > SCREEN_H)
	{
		if(b->y > SCREEN_H && c->y > SCREEN_H)
		{
			return;
		}
		if(b->y <= SCREEN_H && c->y <= SCREEN_H)
		{
			clip_vertex_y(&n1, SCREEN_H, b, a);
			clip_vertex_y(&n2, SCREEN_H, c, a);
			clip_screen_tri(&n1, b, c);
			clip_screen_tri(&n2, &n1, c);
			return;
		}
		if(c->y > SCREEN_H)
		{
			clip_vertex_y(&n1, SCREEN_H, b, a);
			clip_vertex_y(&n2, SCREEN_H, b, c);
			clip_screen_tri(&n1, b, &n2);
			return;
		}
		if(b->y > SCREEN_H)
		{
			clip_vertex_y(&n1, SCREEN_H, c, b);
			clip_vertex_y(&n2, SCREEN_H, c, a);
			clip_screen_tri(&n2, &n1, c);
			return;
		}
	}
	else
	{
		if(b->y > SCREEN_H)
		{
			if(c->y <= SCREEN_H)
			{
				clip_vertex_y(&n1, SCREEN_H, a, b);
				clip_vertex_y(&n2, SCREEN_H, c, b);
				clip_screen_tri(&n1, c, a);
				clip_screen_tri(&n1, &n2, c);
			}
			else
			{
				clip_vertex_y(&n1, SCREEN_H, a, b);
				clip_vertex_y(&n2, SCREEN_H, a, c);
				clip_screen_tri(&n1, &n2, a);
			}
			return;
		}
		if(c->y > SCREEN_H)
		{
			clip_vertex_y(&n1, SCREEN_H, a, c);
			clip_vertex_y(&n2, SCREEN_H, b, c);
			clip_screen_tri(&n2, &n1, a);
			clip_screen_tri(&n2, a, b);
			return;
		}
	}
	if(a->x < 0)
	{
		if(b->x < 0 && c->x < 0)
		{
			return;
		}
		if(b->x >= 0.0 && c->x >= 0.0)
		{
			clip_vertex_x(&n1, 0, b, a);
			clip_vertex_x(&n2, 0, c, a);
			clip_screen_tri(&n1, b, c);
			clip_screen_tri(&n2, &n1, c);
			return;
		}
		if(c->x < 0.0)
		{
			clip_vertex_x(&n1, 0, b, a);
			clip_vertex_x(&n2, 0, b, c);
			clip_screen_tri(&n1, b, &n2);
			return;
		}
		if(b->x < 0.0)
		{
			clip_vertex_x(&n1, 0, c, b);
			clip_vertex_x(&n2, 0, c, a);
			clip_screen_tri(&n2, &n1, c);
			return;
		}
	}
	else
	{
		if(b->x < 0)
		{
			if(c->x >= 0.0)
			{
				clip_vertex_x(&n1, 0, a, b);
				clip_vertex_x(&n2, 0, c, b);
				clip_screen_tri(&n1, c, a);
				clip_screen_tri(&n1, &n2, c);
			}
			else
			{
				clip_vertex_x(&n1, 0, a, b);
				clip_vertex_x(&n2, 0, a, c);
				clip_screen_tri(&n1, &n2, a);
			}
			return;
		}
		if(c->x < 0)
		{
			clip_vertex_x(&n1, 0, a, c);
			clip_vertex_x(&n2, 0, b, c);
			clip_screen_tri(&n2, &n1, a);
			clip_screen_tri(&n2, a, b);
			return;
		}
	}
	if(a->x > SCREEN_W)
	{
		if(b->x > SCREEN_W && c->x > SCREEN_W)
		{
			return;
		}
		if(b->x <= SCREEN_W && c->x <= SCREEN_W)
		{
			clip_vertex_x(&n1, SCREEN_W, b, a);
			clip_vertex_x(&n2, SCREEN_W, c, a);
			clip_screen_tri(&n1, b, c);
			clip_screen_tri(&n2, &n1, c);
			return;
		}
		if(c->x > SCREEN_W)
		{
			clip_vertex_x(&n1, SCREEN_W, b, a);
			clip_vertex_x(&n2, SCREEN_W, b, c);
			clip_screen_tri(&n1, b, &n2);
			return;
		}
		if(b->x > SCREEN_W)
		{
			clip_vertex_x(&n1, SCREEN_W, c, b);
			clip_vertex_x(&n2, SCREEN_W, c, a);
			clip_screen_tri(&n2, &n1, c);
			return;
		}
	}
	else
	{
		if(b->x > SCREEN_W)
		{
			if(c->x <= SCREEN_W)
			{
				clip_vertex_x(&n1, SCREEN_W, a, b);
				clip_vertex_x(&n2, SCREEN_W, c, b);
				clip_screen_tri(&n1, c, a);
				clip_screen_tri(&n1, &n2, c);
			}
			else
			{
				clip_vertex_x(&n1, SCREEN_W, a, b);
				clip_vertex_x(&n2, SCREEN_W, a, c);
				clip_screen_tri(&n1, &n2, a);
			}
			return;
		}
		if(c->x > SCREEN_W)
		{
			clip_vertex_x(&n1, SCREEN_W, a, c);
			clip_vertex_x(&n2, SCREEN_W, b, c);
			clip_screen_tri(&n2, &n1, a);
			clip_screen_tri(&n2, a, b);
			return;
		}
	}
	func_800d3528(a, b, c);
}

/* 0x8010d64c: the point of the edge from..to at the given y */
void clip_vertex_y(cvert_t *out, float y, cvert_t *from, cvert_t *to)
{
	float	t;

	out->y = y;
	t = (from->y - y) / (from->y - to->y);
	out->x = from->x - (from->x - to->x) * t;
	out->a = from->a - (from->a - to->a) * t;
	out->z = from->z - (from->z - to->z) * t;
	out->u = from->u - (from->u - to->u) * t;
	out->v = from->v - (from->v - to->v) * t;
}

/* 0x8010d6e4: at the given x */
void clip_vertex_x(cvert_t *out, float x, cvert_t *from, cvert_t *to)
{
	float	t;

	out->x = x;
	t = (from->x - x) / (from->x - to->x);
	out->y = from->y - (from->y - to->y) * t;
	out->a = from->a - (from->a - to->a) * t;
	out->z = from->z - (from->z - to->z) * t;
	out->u = from->u - (from->u - to->u) * t;
	out->v = from->v - (from->v - to->v) * t;
}

/* 0x8010d77c: at the given depth */
void clip_vertex_z(cvert_t *out, float z, cvert_t *from, cvert_t *to)
{
	float	t;

	out->z = z;
	t = (from->z - z) / (from->z - to->z);
	out->x = from->x - (from->x - to->x) * t;
	out->y = from->y - (from->y - to->y) * t;
	out->a = from->a - (from->a - to->a) * t;
	out->u = from->u - (from->u - to->u) * t;
	out->v = from->v - (from->v - to->v) * t;
}
