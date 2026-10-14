/*
 * particle.c - particles (the file at 0x800ec540-0x800ed030): a single textured triangle at a
 * world point (draw_particle, which the blood drops and sparks draw through too) and the puff
 * object (game object 0x1000, Krampus's breath): a burst of up to 400 particles thrown from a
 * point, falling and bouncing on a floor, fading over 25 frames. The puff's record is an
 * enemy's size; its puff_t hangs off +0x54.
 */
#include <stdio.h>
#include <stdlib.h>
#include <glide/glide.h>
#include "camera.h"
#include "objects.h"
#include "enemy.h"
#include "texmap.h"

extern float	func_800fe8dc(float);				/* effects.c: a random float in +-range */
extern int	printf(const char *, ...);
void		draw_particle(float *, float, float);

/* one particle (0x34 bytes) */
typedef struct puff_particle
{
	float	pos[3];			/* 0x00 */
	float	vel[3];			/* 0x0c */
	float	acc[3];			/* 0x18: gravity, 0.22 a frame */
	float	unused[3];		/* 0x24 */
	float	size;			/* 0x30 */
} puff_particle_t;

/* the puff (0x10 bytes, at the record's +0x54) */
typedef struct puff
{
	float		floor;		/* 0x00: the particles bounce here */
	unsigned int	colour;		/* 0x04 */
	int		count;		/* 0x08 */
	puff_particle_t	*parts;		/* 0x0c */
} puff_t;

#define PUFF(e)		(*(puff_t **)&(e)->unk54)
#define PUFF_LIFE	26		/* frames: the alpha drops 10 a frame from 255 */
#define PUFF_MAX	400

static void	remove_puff(game_object_t *);

/* 0x800ec540: the puff's record; puff_setup fills it */
int init_puff(game_object_t *o)
{
	enemy_t	*e;

	e = malloc(sizeof(enemy_t));
	o->data = (short *)e;
	if(!e)
	{
		return 1;
	}
	e->counter = 0;
	e->frame = 0;
	e->ai_state = 0;
	e->flags = 0;
	e->unk54 = 0;
	o->die = 0;
	o->hit = 0;
	o->remove = remove_puff;
	return 0;
}

/* 0x800ec5ac: throw count particles (at most 400) from pos: each starts with the velocity
   (vx, vy, vz) plus up to (rx, ry, rz) either way, falls, and bounces at floor; drawn in
   colour. On failure the object is removed */
void puff_setup(short id, int count, float *pos, float floor, float vx, float vy, float vz, float rx, float ry, float rz, unsigned int colour)
{
	enemy_t		*e;
	puff_t		*pf;
	puff_particle_t	*pp;
	int		i;

	e = get_object_data(id);
	PUFF(e) = malloc(sizeof(puff_t));
	if(!PUFF(e))
	{
		printf("Cannot malloc particle_data for particle!\n");
		remove_game_object(id);
		return;
	}
	pf = PUFF(e);
	pf->count = count;
	if(count > PUFF_MAX)
	{
		pf->count = PUFF_MAX;
	}
	pf->parts = malloc(pf->count * sizeof(puff_particle_t));
	if(!PUFF(e))		/* the original tests the puff again, not the particles */
	{
		pf->parts = 0;
		remove_game_object(id);
		printf("Cannot malloc particle_data2 for particle!\n");
	}
	for(i = 0; i < pf->count; i++)
	{
		pp = &pf->parts[i];
		pp->pos[0] = pos[0];
		pp->pos[1] = pos[1];
		pp->pos[2] = pos[2];
		pp->vel[0] = vx + func_800fe8dc(rx);
		pp->vel[1] = vy + func_800fe8dc(ry);
		pp->vel[2] = vz + func_800fe8dc(rz);
		pp->size = 0.5f;
		pp->acc[0] = 0.0f;
		pp->acc[1] = -0.22f;
		pp->acc[2] = 0.0f;
		pp->unused[2] = 0.0f;
		pp->unused[1] = 0.0f;
		pp->unused[0] = 0.0f;
	}
	pf->colour = colour;
	pf->floor = floor;
}

/* 0x800ec798: every particle of a puff takes this size */
void puff_set_size(short id, float size)
{
	puff_t	*pf;
	int	i;

	pf = PUFF((enemy_t *)get_object_data(id));
	for(i = 0; i < pf->count; i++)
	{
		pf->parts[i].size = size;
	}
}

/* 0x800ec7fc */
static void remove_puff(game_object_t *o)
{
	enemy_t		*e;
	puff_t		*pf;
	puff_particle_t	*pp;

	e = (enemy_t *)o->data;
	pf = PUFF(e);
	pp = pf->parts;
	if(e)
	{
		if(pf)
		{
			if(pp)
			{
				free(pp);
			}
			free(pf);
		}
		free(e);
	}
}

/* 0x800ec978: the particles with the PARTCL texture (0x7001) in the puff's colour, black keyed
   out, fading 10 a frame */
void draw_puff(game_object_t *o)
{
	enemy_t	*e;
	puff_t	*pf;
	int	i;

	e = (enemy_t *)o->data;
	pf = PUFF(e);
	guTexSource(find_surface(0x7001)->handle);
	guColorCombineFunction(5);
	guAlphaSource(3);
	grAlphaBlendFunction(1, 5, 4, 0);
	grConstantColorValue(pf->colour);
	grChromakeyMode(1);
	grChromakeyValue(0);
	for(i = 0; i < pf->count; i++)
	{
		draw_particle(pf->parts[i].pos, pf->parts[i].size, 255.0f - (float)(unsigned short)e->counter * 10.0f);
	}
	grAlphaBlendFunction(4, 0, 4, 0);
	grChromakeyMode(0);
}

/* 0x800eca94: move the particles; one below the floor is put on it and bounces (an eighth of
   its sideways speed, a sixth of its fall, upward); the puff goes after 25 frames */
void puff_think(game_object_t *o)
{
	enemy_t		*e;
	puff_t		*pf;
	puff_particle_t	*pp;
	int		i;

	e = (enemy_t *)o->data;
	pf = PUFF(e);
	for(i = 0; i < pf->count; i++)
	{
		pp = &pf->parts[i];
		pp->pos[0] = pp->pos[0] + pp->vel[0];
		pp->pos[1] = pp->pos[1] + pp->vel[1];
		pp->pos[2] = pp->pos[2] + pp->vel[2];
		if(pp->pos[1] < pf->floor)
		{
			pp->pos[1] = pf->floor;
			pp->vel[0] = pp->vel[0] / 8.0f;
			pp->vel[1] = pp->vel[1] / -6.0f;
			pp->vel[2] = pp->vel[2] / 8.0f;
		}
		pp->vel[0] = pp->vel[0] + pp->acc[0];
		pp->vel[1] = pp->vel[1] + pp->acc[1];
		pp->vel[2] = pp->vel[2] + pp->acc[2];
	}
	if(++e->counter >= PUFF_LIFE)
	{
		remove_game_object(o->id);
	}
}

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
