/*
 * effects.c - the effects file (0x800fd990-0x801007a0): the effect list on an enemy, and the
 * particle lists the draw pass runs after the level: the chunks of flesh (a model piece that
 * flies, bounces and fades), the blood drops, the sparks, the columns of smoke, the snow and
 * the flecks (small coloured triangles). Each list is singly linked through its record and
 * reaped with a pointer to the previous record, the list head standing in as the first
 * record's link. See effects.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <glide/glide.h>
#include "effects.h"
#include "model.h"
#include "texmap.h"
#include "camera.h"
#include "clip.h"

extern int	free_mem_left;				/* 0x801e0ee8 */
extern int	violence_level;				/* 0x801de8a0 */
#include "credits.h"			/* debug_mode */
extern float	debug_p1x, debug_p1y, debug_p1z, debug_p2x, debug_p2y, debug_p2z;	/* objects.c */
extern int	randrng(int);
extern float	func_800fe8dc(float range);		/* a random float in +-range (0x800fe8dc, this file) */
extern float	get_screen_pos(float *, float *);
extern void	draw_particle(float *, float, float);	/* particle.c */
extern void	rot_matrix_zyx(float *, float *), rot_matrix_yxz(float *, float *), rot_matrix_neg_xyz(float *, float *);
extern void	mat_vec_mul(float *, float *, float *), vec_add(float *, float *, float *);
extern void	offset_in_view(float *, float, float, float, float *);	/* objects.c */
extern void	func_800ce2f8(float *, float, float *);	/* objects.c: a point above a group */
extern void	func_800d9cc0(int, float, float, float, float *);	/* a quad drawer the flame uses (draw2d's file) */
extern void	func_800d326c(MidVertex *, MidVertex *, MidVertex *);	/* a textured triangle with the depth gradients */
extern void	func_800d2dd4(void *, void *, void *);	/* a flat-coloured triangle with the depth gradients */
extern int	load_model(void **, int, void **, int, void **, int, char *, int);
extern void	remove_model(void **, int, void **, int, void **, int, char *);
/* 0x80190968, 0x80190988: the chunk's texture, with and without blood */
texmap_t	chnk_texmap[1] = { { 0x3f00, 1, 0xffffff, 0, "chnk.WMS" } };
texmap_t	chnknv_texmap[1] = { { 0x3f00, 1, 0xffffff, 0, "chnknv.WMS" } };

#define PREV_OF(head, type)	((type *)((char *)&(head) - offsetof(type, next)))

/* 0x800fd990: push an effect on the front of the list */
void attach_effect(enemy_t *e, effect_t *ef)
{
	if(ef)
	{
		ef->next = e->effects;
		e->effects = ef;
	}
}

/* 0x800fd9ac: after the model: every effect's draw */
void draw_effects(enemy_t *e)
{
	effect_t *ef;

	ef = e->effects;
	while(ef)
	{
		ef->draw(e->effects, ef->param, ef->group);
		ef = ef->next;
	}
}

/* 0x800fda34: every frame: every effect's think */
void think_effects(enemy_t *e)
{
	effect_t *ef;

	ef = e->effects;
	while(ef)
	{
		ef->think(e, ef->param);
		ef = ef->next;
	}
}

/* 0x800fda88: the enemy goes: remove and free them all */
void free_effects(enemy_t *e)
{
	effect_t *ef, *next;

	ef = e->effects;
	while(ef)
	{
		next = ef->next;
		ef->remove(e, ef->param);
		free(ef);
		ef = next;
	}
}

/* 0x800fdaf0: drop the effects riding group g */
void detach_effect(enemy_t *e, group_t *g)
{
	effect_t *ef, *next, *prev;

	prev = 0;
	ef = e->effects;
	while(ef)
	{
		next = ef->next;
		if(ef->group != g)
		{
			prev = ef;
		}
		else
		{
			ef->remove(e, ef->param);
			free(ef);
			if(prev)
			{
				prev->next = next;
			}
			else
			{
				e->effects = next;
			}
		}
		ef = next;
	}
}

/* 0x800fdb84: every effect of the type comes off the enemy: its remove callback, then the
   record */
void remove_effects_of_type(enemy_t *e, int type)
{
	effect_t *ef, *prev, *next;

	prev = 0;
	for(ef = e->effects; ef; ef = next)
	{
		next = ef->next;
		if(ef->type == type)
		{
			ef->remove(e, ef->param);
			free(ef);
			if(prev)
			{
				prev->next = next;
			}
			else
			{
				e->effects = next;
			}
		}
		else
		{
			prev = ef;
		}
	}
}

/* ---- the chunks (type 1 parts drawn with the CHNK model) ---- */

part_t		*chunk_list;			/* 0x8022b16c */
int		num_chunks;			/* 0x801e0e14 */
void		*chnk_verts, *chnk_faces, *chnk_normals;	/* 0x801e587c, 0x80212dfc, 0x801e15a4 */
submesh_t	chnk_submesh[1] = { { 0, 7, 0, 0, 5, 0, 5, 0.032833f, { -0.001f, -0.013f, -0.0155f } } };	/* 0x801909a8 */

/* 0x800fdc18: forty blood drops springing up from a point, when violence is on */
void burst_blood(float *pos, float floor)
{
	float	vel[3];
	int	i;

	if(violence_level > 0)
	{
		for(i = 0; i < 40; i++)
		{
			vel[0] = (float)(randrng(1000) - 500) / 2000.0f;
			vel[1] = (float)randrng(1000) / 2000.0f + 1.0f;
			vel[2] = (float)(randrng(1000) - 500) / 2000.0f;
			add_blood_drop(pos, vel, floor);
		}
	}
}

/* 0x800fdcf4: chunks of flesh from a point: ten while fewer than ten are flying, five
   under twenty, three under forty-one; none without violence or 32K of heap */
void add_chunks(float *pos, float floor)
{
	part_t	*c;
	float	s;
	int	i, n;

	if(violence_level <= 0)
	{
		return;
	}
	if(free_mem_left <= 0x7fff)
	{
		return;
	}
	if(num_chunks >= 41)
	{
		return;
	}
	if(num_chunks < 10)
	{
		n = 10;
	}
	else if(num_chunks < 20)
	{
		n = 5;
	}
	else
	{
		n = 3;
	}
	for(i = 0; i < n; i++)
	{
		num_chunks++;
		c = malloc(sizeof(part_t));
		if(c == 0)
		{
			printf("Chunk malloc failed!\n");
			return;
		}
		c->type = 1;
		c->unk3c = 0;
		c->state = 0;
		c->pos[0] = pos[0];
		c->pos[1] = pos[1];
		c->pos[2] = pos[2];
		c->rot[0] = 0.0f;
		c->rot[1] = 0.0f;
		c->rot[2] = 0.0f;
		s = func_800fe8dc(0.5f) + 1.25f;
		c->scale[0] = s * -8.3333f;
		c->scale[1] = s * 10.0f;
		c->scale[2] = s * 8.3333f;
		c->vel[0] = (float)(randrng(1000) - 500) / 2000.0f;
		c->vel[1] = (float)randrng(1000) / 2000.0f + 1.0f;
		c->vel[2] = (float)(randrng(1000) - 500) / 2000.0f;
		c->floor = floor;
		c->alpha = 255.0f;
		c->alpha_step = -14.0f;
		c->next = chunk_list;
		chunk_list = c;
	}
}

/* 0x800fdf00: n chunks from a point */
void add_chunks_n(float *pos, float floor, int n)
{
	part_t	*c;
	float	s;
	int	i;

	if(violence_level <= 0)
	{
		return;
	}
	if(free_mem_left <= 0x7fff)
	{
		return;
	}
	for(i = 0; i < n; i++)
	{
		num_chunks++;
		c = malloc(sizeof(part_t));
		if(c == 0)
		{
			printf("Chunk malloc failed!\n");
			return;
		}
		c->type = 1;
		c->unk3c = 0;
		c->state = 0;
		c->pos[0] = pos[0];
		c->pos[1] = pos[1];
		c->pos[2] = pos[2];
		c->rot[0] = 0.0f;
		c->rot[1] = 0.0f;
		c->rot[2] = 0.0f;
		s = func_800fe8dc(0.5f) + 1.25f;
		c->scale[0] = s * -8.3333f;
		c->scale[1] = s * 10.0f;
		c->scale[2] = s * 8.3333f;
		c->vel[0] = (float)(randrng(1000) - 500) / 2000.0f;
		c->vel[1] = (float)randrng(1000) / 2000.0f + 1.0f;
		c->vel[2] = (float)(randrng(1000) - 500) / 2000.0f;
		c->floor = floor;
		c->alpha = 255.0f;
		c->alpha_step = -14.0f;
		c->next = chunk_list;
		chunk_list = c;
	}
}

/* 0x800fe0e0, 0x800fe128: the chunk's texmap (bloodless under violence 0 and 1) and model */
void load_chnk_texmap(void)
{
	texmap_t *t;

	t = chnknv_texmap;
	if(violence_level >= 2)
	{
		t = chnk_texmap;
	}
	load_texmap(t, 1, "chnk");
}

void load_chnk_model(void)
{
	load_model(&chnk_verts, 6, &chnk_faces, 8, &chnk_normals, 6, "CHNK.ZM", 0x4475546c);
	chunk_list = 0;
	num_chunks = 0;
}

/* 0x800fe198 */
void remove_chnk_model(void)
{
	remove_surface(chnk_texmap, 1);
	remove_model(&chnk_verts, 6, &chnk_faces, 8, &chnk_normals, 6, "CHNK.ZM");
}

/* 0x800fe1fc: draw every chunk and move it: a flying one tumbles and falls until it meets
   its floor, where it bounces at half speed until it is too slow and starts fading; a fading
   one goes when its alpha runs out. A chunk that just came to rest is looked at again at once */
void draw_chunks(void)
{
	part_t	*c, *prev, *next;

	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	c = chunk_list;
	prev = PREV_OF(chunk_list, part_t);
	while(c)
	{
		next = c->next;
		if(c->type == 1)
		{
			draw_part_model(c, chnk_submesh, chnk_verts, 6, chnk_faces, 8, chnk_normals, 6);
		}
		if(c->state == 0)
		{
			c->rot[0] = c->rot[0] + 0.5f;
			c->rot[1] = c->rot[1] + 0.1f;
			c->pos[0] = c->pos[0] + c->vel[0];
			c->pos[1] = c->pos[1] + c->vel[1];
			c->pos[2] = c->pos[2] + c->vel[2];
			if(c->pos[1] < c->floor)
			{
				c->pos[1] = c->floor;
				c->vel[1] = c->vel[1] * -0.5f;
				if(c->vel[1] < 0.12f)
				{
					c->state = 2;
					c->unk3c = 0;
					continue;
				}
			}
			c->vel[1] = c->vel[1] + -0.22f;
			prev = c;
		}
		else if(c->state == 2)
		{
			c->alpha = c->alpha + c->alpha_step;
			if(c->alpha < 0.0f)
			{
				free(c);
				c = next;
				prev->next = c;
				num_chunks--;
				continue;
			}
			prev = c;
		}
		else
		{
			prev = c;
		}
		c = next;
	}
}

/* 0x800fe3d8 */
void free_chunks(void)
{
	part_t	*c, *prev, *next;

	c = chunk_list;
	prev = PREV_OF(chunk_list, part_t);
	while(c)
	{
		next = c->next;
		free(c);
		num_chunks--;
		c = next;
		prev->next = c;
	}
}

/* ---- the blood drops ---- */

drop_t		*drop_list;			/* 0x8021f0b0 */
int		num_drops;			/* 0x801e5be4 */
surface_t	*drop_surface;			/* 0x801f8844: PARTCL */
unsigned int	drop_color;			/* 0x801e0e08: dark red, or green without violence */

/* 0x800fe440 */
void init_blood_drops(void)
{
	drop_list = 0;
	drop_surface = find_surface(0x7001);
	if(violence_level >= 2)
	{
		drop_color = 0x800000;
	}
	else
	{
		drop_color = 0x8000;
	}
	num_drops = 0;
}

/* 0x800fe498: a drop with the velocity, falling to the floor, for 1..40 frames (up to 512
   drops, with 32K of heap) */
void add_blood_drop(float *pos, float *vel, float floor)
{
	drop_t	*d;

	if(free_mem_left <= 0x7fff)
	{
		return;
	}
	if(num_drops >= 0x200)
	{
		return;
	}
	d = malloc(sizeof(drop_t));
	if(d == 0)
	{
		return;
	}
	d->pos[0] = pos[0];
	d->pos[1] = pos[1];
	d->pos[2] = pos[2];
	d->vel[0] = vel[0];
	d->vel[1] = vel[1];
	d->vel[2] = vel[2];
	d->floor = floor;
	d->life = randrng(0x28) + 1;
	d->next = drop_list;
	drop_list = d;
	num_drops++;
}

/* 0x800fe580: draw every drop (its alpha from the life left) and move it: it falls, stops
   dead on its floor, and goes when its life runs out */
void draw_blood_drops(void)
{
	drop_t	*d, *prev, *next;

	guTexSource(drop_surface->handle);
	guColorCombineFunction(GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grConstantColorValue(drop_color);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(0x40);
	d = drop_list;
	prev = PREV_OF(drop_list, drop_t);
	while(d)
	{
		next = d->next;
		draw_particle(d->pos, 1.0f, (float)(unsigned int)d->life / 25.0f * 255.0f);
		d->pos[0] = d->pos[0] + d->vel[0];
		d->pos[1] = d->pos[1] + d->vel[1];
		d->pos[2] = d->pos[2] + d->vel[2];
		d->vel[1] = d->vel[1] + -0.22f;
		if(d->pos[1] < d->floor)
		{
			d->pos[1] = d->floor;
			d->vel[0] = 0.0f;
			d->vel[2] = 0.0f;
		}
		d->life--;
		if(d->life == 0)
		{
			free(d);
			d = next;
			prev->next = d;
			num_drops--;
		}
		else
		{
			prev = d;
			d = next;
		}
	}
	grAlphaTestReferenceValue(0);
}

/* 0x800fe748 */
void free_blood_drops(void)
{
	drop_t	*d, *prev, *next;

	d = drop_list;
	prev = PREV_OF(drop_list, drop_t);
	while(d)
	{
		next = d->next;
		free(d);
		d = next;
		prev->next = d;
	}
	num_drops = 0;
}

/* ---- the blood spray: an effect (type 1) that pours drops from a point on a group ---- */

float	spray_pulse[12] = { 0.25f, 0.3f, 0.35f, 0.4f, 0.6f, 0.9f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f };	/* 0x801909c8 */

/* 0x800fe924: nothing to draw */
void spray_draw(effect_t *list, void *param, group_t *g)
{
}

/* 0x800fe92c: while the group shows, six drops a frame from the base point along the
   direction, each scattered by a fifth and scaled by the pulse; debug mode 0x80000 takes
   the base and direction from the debug points */
void spray_think(enemy_t *e, void *param)
{
	spray_t	*s;
	drop_t	*d;
	float	origin[3], v[3], floor, scale;
	int	n;

	s = (spray_t *)param;
	if(!(s->group->flags & 0x2000))
	{
		return;
	}
	if(debug_mode & 0x80000)
	{
		s->dir[0] = debug_p1x;
		s->dir[1] = debug_p1y;
		s->dir[2] = debug_p1z;
		s->base[0] = debug_p2x;
		s->base[1] = debug_p2y;
		s->base[2] = debug_p2z;
	}
	s->frame++;
	if(s->frame >= 12)
	{
		s->frame = 0;
	}
	scale = spray_pulse[s->frame];
	mat_vec_mul(s->base, s->group->mat, origin);
	vec_add(origin, s->group->pos, origin);
	for(n = 6; n > 0; n--)
	{
		v[0] = scale * (s->dir[0] + (float)(randrng(10000) - 5000) / 25000.0f * s->dir[0]);
		v[1] = scale * (s->dir[1] + (float)(randrng(10000) - 5000) / 25000.0f * s->dir[1]);
		v[2] = scale * (s->dir[2] + (float)(randrng(10000) - 5000) / 25000.0f * s->dir[2]);
		mat_vec_mul(v, s->group->mat, v);
		floor = origin[1] - 20.0f;
		if(free_mem_left <= 0x7fff)
		{
			continue;
		}
		if(num_drops >= 0x200)
		{
			continue;
		}
		d = malloc(sizeof(drop_t));
		if(d == 0)
		{
			continue;
		}
		d->pos[0] = origin[0];
		d->pos[1] = origin[1];
		d->pos[2] = origin[2];
		d->vel[0] = v[0];
		d->vel[1] = v[1];
		d->vel[2] = v[2];
		d->floor = floor;
		d->life = randrng(0x28) + 1;
		d->next = drop_list;
		drop_list = d;
		num_drops++;
	}
}

/* 0x800febb0 */
void spray_remove(enemy_t *e, void *param)
{
	free(param);
}

/* 0x800fe7a4: a spray from base along dir on the group (both on the group); the debug points
   take their values */
effect_t *attach_spray(float *base, float *dir, group_t *g)
{
	effect_t *ef;
	spray_t	*s;

	ef = malloc(sizeof(effect_t));
	if(ef == 0)
	{
		return(0);
	}
	ef->type = 1;
	ef->draw = spray_draw;
	ef->think = spray_think;
	ef->remove = spray_remove;
	s = malloc(sizeof(spray_t));
	ef->param = s;
	if(s == 0)
	{
		free(ef);
		return(0);
	}
	s->base[0] = base[0];
	s->base[1] = base[1];
	s->base[2] = base[2];
	s->dir[0] = dir[0];
	s->dir[1] = dir[1];
	s->dir[2] = dir[2];
	debug_p2y = s->base[1];
	debug_p2z = s->base[2];
	s->group = g;
	s->unk1c = 0;
	s->color = 0xffffff;
	s->frame = 0;
	debug_p1x = s->dir[0];
	debug_p1y = s->dir[1];
	debug_p2x = s->base[0];
	debug_p1z = s->dir[2];
	return(ef);
}

/* ---- the flame: an effect (type 2) burning on a group ---- */

int	flame_counter = 1;			/* 0x801909f8: 1..4, which flame texture the next one starts on */

/* 0x800fecd4: the flame sprite (0x5031 on, five frames) above the group, flickering in
   size; every fifth frame a spark and a puff of smoke */
void flame_draw(effect_t *list, void *param, group_t *g)
{
	flame_t	*f;
	float	p[3], size;

	f = (flame_t *)param;
	size = ((float)randrng(50) + 75.0f) * 0.01f;
	grDepthMask(FXFALSE);
	func_800ce2f8(g->pivot, f->scale, p);
	func_800d9cc0(f->frame + 0x5031, f->y * size, 128.0f, *(float *)&f->unk8, p);
	grDepthMask(FXTRUE);
	f->frame++;
	if(f->frame >= 5)
	{
		f->frame = 0;
		add_spark(p);
		add_smoke(p, 0x5f5f6f);
	}
}

/* 0x800fedb0 */
void flame_think(enemy_t *e, void *param)
{
}

/* 0x800fedb8 */
void flame_remove(enemy_t *e, void *param)
{
	free(param);
}

/* 0x800febd0: a flame scale high and y up on the group, starting on the next of the four
   flame frames */
effect_t *attach_flame(float scale, float y, group_t *g)
{
	effect_t *ef;
	flame_t	*f;

	ef = malloc(sizeof(effect_t));
	if(ef == 0)
	{
		printf("FX malloc failed in flame!\n");
		return(0);
	}
	ef->type = 2;
	ef->draw = flame_draw;
	ef->think = flame_think;
	ef->remove = flame_remove;
	ef->group = g;
	f = malloc(sizeof(flame_t));
	ef->param = f;
	if(f == 0)
	{
		free(ef);
		return(0);
	}
	f->scale = scale;
	f->y = y;
	f->unk8 = -1;
	f->frame = flame_counter;
	flame_counter++;
	if(flame_counter >= 5)
	{
		flame_counter = 1;
	}
	return(ef);
}

/* ---- the sparks ---- */

particle_t	*spark_list;			/* 0x80237294 */
surface_t	*spark_surface;			/* 0x801fad14: SPARKZ */

/* 0x800fedd8 */
void init_sparks(void)
{
	spark_list = 0;
	spark_surface = find_surface(0x7005);
}

/* 0x800fee08: a spark from a point: a random velocity (up to a fifth sideways, 0.05 to 0.55
   up), orange */
void add_spark(float *pos)
{
	particle_t *p;

	p = malloc(sizeof(particle_t));
	if(p == 0)
	{
		printf("spark malloc failed!\n");
		return;
	}
	p->pos[0] = pos[0];
	p->pos[1] = pos[1];
	p->pos[2] = pos[2];
	p->vel[0] = (float)(randrng(10000) - 5000) / 50000.0f;
	p->vel[1] = (float)(randrng(10000) - 5000) / 20000.0f + 0.3f;
	p->vel[2] = (float)(randrng(10000) - 5000) / 50000.0f;
	p->r = 0xff;
	p->a = 0;
	p->g = 0xb2;
	p->b = 0;
	p->next = spark_list;
	spark_list = p;
}

/* 0x800fef18: draw every spark in its colour (its red as the alpha) and move it; the green
   fades by 5 a frame and the red by 6, and it goes when the red runs out */
void draw_sparks(void)
{
	particle_t *p, *prev, *next;
	int	g, r;

	guTexSource(spark_surface->handle);
	guColorCombineFunction(GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(0x10);
	p = spark_list;
	prev = PREV_OF(spark_list, particle_t);
	while(p)
	{
		next = p->next;
		grConstantColorValue(*(unsigned int *)&p->b);
		draw_particle(p->pos, 0.6f, (float)p->r);
		p->pos[0] = p->pos[0] + p->vel[0];
		p->pos[1] = p->pos[1] + p->vel[1];
		p->pos[2] = p->pos[2] + p->vel[2];
		g = p->g - 5;
		if(g < 0)
		{
			g = 0;
		}
		r = p->r - 6;
		if(r < 0)
		{
			free(p);
			prev->next = next;
		}
		else
		{
			prev = p;
			prev->r = r;
			prev->g = g;
		}
		p = next;
	}
	grAlphaTestReferenceValue(0);
}

/* 0x800ff05c */
void free_sparks(void)
{
	particle_t *p, *prev, *next;

	p = spark_list;
	prev = PREV_OF(spark_list, particle_t);
	while(p)
	{
		next = p->next;
		free(p);
		p = next;
		prev->next = p;
	}
}

/* ---- the smoke ---- */

smoke_t		*smoke_list;			/* 0x80237264 */
surface_t	*smoke_surface;			/* 0x801faccc: SMOKTALL */
float		smoke_top[4][3];		/* 0x801e7510: the column's corners, 3 wide from 3 to 12 up */
float		smoke_base[2][3];		/* 0x801f8828: the foot's corners at the ground */

/* 0x800ff0b0 */
void init_smoke(void)
{
	smoke_list = 0;
	smoke_surface = find_surface(0x5001);
	smoke_top[0][2] = 0.0f;
	smoke_top[1][2] = 0.0f;
	smoke_top[2][2] = 0.0f;
	smoke_top[3][2] = 0.0f;
	smoke_base[0][1] = 0.0f;
	smoke_base[0][2] = 0.0f;
	smoke_base[1][1] = 0.0f;
	smoke_base[1][2] = 0.0f;
	smoke_top[0][0] = -1.5f;
	smoke_top[0][1] = 3.0f;
	smoke_top[1][0] = 1.5f;
	smoke_top[1][1] = 3.0f;
	smoke_top[2][0] = -1.5f;
	smoke_top[2][1] = 12.0f;
	smoke_top[3][0] = 1.5f;
	smoke_top[3][1] = 12.0f;
	smoke_base[0][0] = -1.5f;
	smoke_base[1][0] = 1.5f;
}

/* 0x800ff190: a column of smoke near a point (scattered by half a unit), in the colour, of a
   random size from 1 to 2.5 (its texture starting on a random row) */
void add_smoke(float *pos, unsigned int color)
{
	smoke_t	*s;
	float	size;

	s = malloc(sizeof(smoke_t));
	if(s == 0)
	{
		return;
	}
	s->pos[0] = pos[0] + (float)(randrng(10000) - 5000) / 10000.0f;
	s->pos[1] = pos[1];
	s->pos[2] = pos[2] + (float)(randrng(10000) - 5000) / 10000.0f;
	s->t = (float)randrng(255);
	s->alpha = 0.0f;
	s->next = smoke_list;
	size = (float)(randrng(10000) - 5000) / 6666.67f + 1.0f;
	s->color = color;
	s->size = size;
	s->height = size * 48.0f;
	s->top[0][0] = smoke_top[0][0] * s->size;
	s->top[0][1] = smoke_top[0][1] * s->size;
	s->top[0][2] = smoke_top[0][2] * s->size;
	s->top[1][0] = smoke_top[1][0] * s->size;
	s->top[1][1] = smoke_top[1][1] * s->size;
	s->top[1][2] = smoke_top[1][2] * s->size;
	s->top[2][0] = smoke_top[2][0] * s->size;
	s->top[2][1] = smoke_top[2][1] * s->size;
	s->top[2][2] = smoke_top[2][2] * s->size;
	s->top[3][0] = smoke_top[3][0] * s->size;
	s->top[3][1] = smoke_top[3][1] * s->size;
	s->top[3][2] = smoke_top[3][2] * s->size;
	s->base[0][0] = smoke_base[0][0] * s->size;
	s->base[0][1] = smoke_base[0][1] * s->size;
	s->base[0][2] = smoke_base[0][2] * s->size;
	s->base[1][0] = smoke_base[1][0] * s->size;
	s->base[1][1] = smoke_base[1][1] * s->size;
	s->base[1][2] = smoke_base[1][2] * s->size;
	s->alpha_dir = 1.0f;
	smoke_list = s;
}

/* 0x800ff418: draw every column turned to face the camera (the column from its foot to the
   top, then the foot itself) in its colour with the texture's alpha, and move it: the texture
   scrolls up four rows a frame and the alpha rises by 14 a frame to 240, then falls; it goes
   when the alpha runs out */
void draw_smoke(void)
{
	smoke_t	*s, *prev, *next;
	cvert_t	v0, v1, v2, v3, t0, t3;
	float	m[9];

	guTexSource(smoke_surface->handle);
	guColorCombineFunction(GR_COLORCOMBINE_CCRGB);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(1);
	grDepthMask(FXFALSE);
	s = smoke_list;
	prev = PREV_OF(smoke_list, smoke_t);
	while(s)
	{
		rot_matrix_yxz(m, &camera_rx);
		next = s->next;
		grConstantColorValue(s->color);
		mat_vec_mul(s->top[0], m, &v0.x);
		mat_vec_mul(s->top[1], m, &v1.x);
		mat_vec_mul(s->top[2], m, &v2.x);
		mat_vec_mul(s->top[3], m, &v3.x);
		v0.x += s->pos[0];
		v1.x += s->pos[0];
		v2.x += s->pos[0];
		v3.x += s->pos[0];
		v0.y += s->pos[1];
		v1.y += s->pos[1];
		v2.y += s->pos[1];
		v3.y += s->pos[1];
		v0.z += s->pos[2];
		v1.z += s->pos[2];
		v2.z += s->pos[2];
		v3.z += s->pos[2];
		v0.u = 31.9375f;
		v0.v = s->t;
		v1.u = 0.0625f;
		v1.v = s->t;
		v2.u = 31.9375f;
		v2.v = s->t - s->height;
		v3.u = 0.0625f;
		v3.v = s->t - s->height;
		v0.a = s->alpha;
		v1.a = s->alpha;
		v2.a = s->alpha;
		v3.a = s->alpha;
		t0 = v0;
		t3 = v3;
		draw_world_tri(&v0, &v1, &v3);
		draw_world_tri(&t0, &v2, &t3);
		mat_vec_mul(s->base[0], m, &v0.x);
		mat_vec_mul(s->base[1], m, &v1.x);
		mat_vec_mul(s->top[0], m, &v2.x);
		mat_vec_mul(s->top[1], m, &v3.x);
		v0.x += s->pos[0];
		v1.x += s->pos[0];
		v2.x += s->pos[0];
		v3.x += s->pos[0];
		v0.y += s->pos[1];
		v1.y += s->pos[1];
		v2.y += s->pos[1];
		v3.y += s->pos[1];
		v0.z += s->pos[2];
		v1.z += s->pos[2];
		v2.z += s->pos[2];
		v3.z += s->pos[2];
		v0.u = 31.9375f;
		v0.v = (float)((double)s->t + (double)s->height * 0.25);
		v1.u = 0.0625f;
		v1.v = (float)((double)s->t + (double)s->height * 0.25);
		v2.u = 31.9375f;
		v2.v = s->t;
		v3.u = 0.0625f;
		v3.v = s->t;
		v0.a = s->alpha;
		v1.a = s->alpha;
		v2.a = s->alpha;
		v3.a = s->alpha;
		t0 = v0;
		t3 = v3;
		draw_world_tri(&v0, &v1, &v3);
		draw_world_tri(&t0, &v2, &t3);
		s->t = s->t + 4.0f;
		s->alpha = s->alpha + s->alpha_dir * 14.0f;
		if(240.0f < s->alpha)
		{
			s->alpha = 240.0f;
			s->alpha_dir = -1.0f;
		}
		if(s->alpha < 0.0f)
		{
			free(s);
			s = next;
			prev->next = s;
		}
		else
		{
			prev = s;
			s = next;
		}
	}
	grAlphaTestReferenceValue(0);
	grDepthMask(FXTRUE);
}

/* 0x800ff9cc */
void free_smoke(void)
{
	smoke_t	*s, *prev, *next;

	s = smoke_list;
	prev = PREV_OF(smoke_list, smoke_t);
	while(s)
	{
		next = s->next;
		free(s);
		s = next;
		prev->next = s;
	}
}

/* ---- the snow ---- */

snowflake_t	*snow_list;			/* 0x801f8824 */
surface_t	*snow_surface;			/* 0x801e6c0c: SNOW */

/* 0x800ffa20: six flakes scattered around a point as the camera sees it (16 either side, 9.5
   up, 11 to 39 nearer), at rest */
void add_snow_around(float *pos)
{
	float	p[3], none[3];
	float	dx, dz;
	int	i;

	none[0] = 0.0f;
	none[1] = 0.0f;
	none[2] = 0.0f;
	for(i = 0; i < 6; i++)
	{
		dx = (float)(randrng(10000) - 5000) / 312.5f;
		dz = (float)(randrng(10000) - 5000) / 357.143f + -25.0f;
		offset_in_view(pos, dx, 9.5f, dz, p);
		malloc_snow(p, none);
	}
}

/* 0x800ffb38: a flake at pos with the velocity, for 80 frames */
void malloc_snow(float *pos, float *vel)
{
	snowflake_t *f;

	f = malloc(sizeof(snowflake_t));
	if(f == 0)
	{
		printf("snow malloc failed!\n");
		return;
	}
	f->pos[0] = pos[0];
	f->pos[1] = pos[1];
	f->pos[2] = pos[2];
	f->vel[0] = vel[0];
	f->vel[1] = vel[1];
	f->vel[2] = vel[2];
	f->life = 80;
	f->next = snow_list;
	snow_list = f;
}

/* 0x800ffb08 */
void init_snow(void)
{
	snow_list = 0;
	snow_surface = find_surface(0x7006);
}

/* 0x800ffbdc: draw every flake in view (a triangle 0.6 units on a side) and move it: it
   drifts with its velocity, which dies away by a fifth a frame, and falls 0.22 a frame with
   a flutter; it goes after its 80 frames */
void draw_snow(void)
{
	snowflake_t *f, *prev, *next;
	MidVertex	a, b, c;
	float		scr[3], size;

	guTexSource(snow_surface->handle);
	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(4);
	f = snow_list;
	prev = PREV_OF(snow_list, snowflake_t);
	while(f)
	{
		next = f->next;
		if(get_screen_pos(f->pos, scr) != -1.0f)
		{
			size = scr[2] * 0.6f * proj_x;
			size += 2097152.0f;
			size -= 2097152.0f;
			a.oow = b.oow = c.oow = scr[2];
			a.x = scr[0];
			a.y = scr[1];
			b.y = scr[1];
			c.x = scr[0];
			a.sow = scr[2] * 0.0f;
			c.sow = scr[2] * 0.0f;
			c.tow = scr[2] * 0.0f;
			a.tow = scr[2] * 255.0f;
			b.sow = scr[2] * 255.0f;
			b.tow = scr[2] * 255.0f;
			b.x = scr[0] + size;
			c.y = scr[1] + size;
			func_800d326c(&a, &b, &c);
		}
		f->pos[0] = f->pos[0] + f->vel[0];
		f->pos[1] = f->pos[1] + f->vel[1];
		f->pos[2] = f->pos[2] + f->vel[2];
		f->pos[1] = f->pos[1] + ((float)(randrng(10000) - 5000) / 250000.0f + -0.22f);
		f->vel[0] = f->vel[0] * 0.8f;
		f->vel[1] = f->vel[1] * 0.8f;
		f->vel[2] = f->vel[2] * 0.8f;
		f->life--;
		if(f->life == 0)
		{
			free(f);
			prev->next = next;
		}
		else
		{
			prev = f;
		}
		f = next;
	}
	grAlphaTestReferenceValue(0);
}

/* 0x800ffe0c */
void free_snow(void)
{
	snowflake_t *f, *prev, *next;

	f = snow_list;
	prev = PREV_OF(snow_list, snowflake_t);
	while(f)
	{
		next = f->next;
		free(f);
		f = next;
		prev->next = f;
	}
}

/* 0x800ffe60: something passing from one point to another blows the flakes within ten
   units of the second point along (the nearer, the harder) */
void blow_snow(float *from, float *to)
{
	snowflake_t *f;
	float	d[3], r, k;

	d[0] = to[0] - from[0];
	d[1] = to[1] - from[1];
	d[2] = to[2] - from[2];
	for(f = snow_list; f; f = f->next)
	{
		r = (f->pos[0] - to[0]) * (f->pos[0] - to[0]) + (f->pos[1] - to[1]) * (f->pos[1] - to[1]) + (f->pos[2] - to[2]) * (f->pos[2] - to[2]);
		if(r < 100.0f)
		{
			k = (100.0f - r) * 0.04f;
			f->vel[0] = d[0] * k;
			f->vel[2] = d[2] * k;
		}
	}
}

/* ---- the flecks: small flat-coloured triangles ---- */

particle_t	*fleck_list;			/* 0x8021eec4 */

/* 0x800fff30: twenty flecks from a point, upwards within a quarter turn, at 0.2 to 0.4 */
void spray_flecks(float *pos, int r, int g, int b)
{
	float	v[3], angles[3], m[9];
	int	i;

	for(i = 0; i < 20; i++)
	{
		v[0] = 0.0f;
		v[1] = 0.0f;
		v[2] = (float)(randrng(10000) - 5000) / 50000.0f + 0.2f;
		angles[0] = (float)(randrng(10000) - 5000) / 19098.6f + 3.1415927f;
		angles[1] = (float)(randrng(10000) - 5000) / 19098.6f;
		angles[2] = 0.0f;
		rot_matrix_zyx(m, angles);
		mat_vec_mul(v, m, v);
		add_fleck(pos, v, r & 0xff, g & 0xff, b & 0xff);
	}
}

/* 0x80100080: eight flecks back out of an enemy the way it faces, then four at half the
   colour */
void fleck_enemy(enemy_t *e, int r, int g, int b)
{
	float	v[3], m[9];
	int	i;

	for(i = 0; i < 8; i++)
	{
		rot_matrix_neg_xyz(m, &e->rx);
		v[0] = (float)(randrng(10000) - 5000) / 125000.0f;
		v[1] = (float)(randrng(10000) - 5000) / 125000.0f;
		v[2] = (float)(randrng(10000) - 5000) / 50000.0f + -0.1f;
		mat_vec_mul(v, m, v);
		add_fleck(&e->x, v, r & 0xff, g & 0xff, b & 0xff);
	}
	r = (r & 0xff) >> 1;
	g = (g & 0xff) >> 1;
	b = (b & 0xff) >> 1;
	for(i = 0; i < 4; i++)
	{
		rot_matrix_neg_xyz(m, &e->rx);
		v[0] = (float)(randrng(10000) - 5000) / 125000.0f;
		v[1] = (float)(randrng(10000) - 5000) / 125000.0f;
		v[2] = (float)(randrng(10000) - 5000) / 50000.0f + -0.1f;
		mat_vec_mul(v, m, v);
		add_fleck(&e->x, v, r & 0xff, g & 0xff, b & 0xff);
	}
}

/* 0x80100284: fifty flecks in every direction at 0.3 from a point, then fifty lighter ones
   at 0.15 */
void burst_flecks(float *pos, int r, int g, int b)
{
	float	v[3], angles[3], m[9];
	int	i;

	for(i = 0; i < 50; i++)
	{
		v[0] = 0.0f;
		v[1] = 0.0f;
		v[2] = 0.3f;
		angles[0] = (float)(randrng(10000) - 5000) / 1591.55f;
		angles[1] = (float)(randrng(10000) - 5000) / 1591.55f;
		angles[2] = (float)(randrng(10000) - 5000) / 1591.55f;
		rot_matrix_zyx(m, angles);
		mat_vec_mul(v, m, v);
		add_fleck(pos, v, r & 0xff, g & 0xff, b & 0xff);
	}
	r |= 0x80;
	g |= 0x80;
	b |= 0x80;
	b &= 0xff;
	for(i = 0; i < 50; i++)
	{
		v[0] = 0.0f;
		v[1] = 0.0f;
		v[2] = 0.15f;
		angles[0] = (float)(randrng(10000) - 5000) / 1591.55f;
		angles[1] = (float)(randrng(10000) - 5000) / 1591.55f;
		angles[2] = (float)(randrng(10000) - 5000) / 1591.55f;
		rot_matrix_zyx(m, angles);
		mat_vec_mul(v, m, v);
		add_fleck(pos, v, r & 0xff, g & 0xff, b);
	}
}

/* 0x80100478 */
void init_flecks(void)
{
	fleck_list = 0;
}

/* 0x80100488: a fleck with the velocity and colour (with 32K of heap) */
void add_fleck(float *pos, float *vel, int r, int g, int b)
{
	particle_t *p;

	if(free_mem_left <= 0x7fff)
	{
		return;
	}
	p = malloc(sizeof(particle_t));
	if(p == 0)
	{
		return;
	}
	p->pos[0] = pos[0];
	p->pos[1] = pos[1];
	p->pos[2] = pos[2];
	p->vel[0] = vel[0];
	p->vel[1] = vel[1];
	p->vel[2] = vel[2];
	p->a = 0xe6;
	p->r = r;
	p->g = g;
	p->b = b;
	p->next = fleck_list;
	fleck_list = p;
}

/* 0x80100558: draw every fleck in view (a flat triangle up to 3 units on a side) in its
   colour and move it: it drifts, falls a little, and its colour fades by 8 a frame; it goes
   when the colour is gone */
void draw_flecks(void)
{
	particle_t *p, *prev, *next;
	MidVertex	a, b, c;
	float		scr[3], size;
	int		r, g, bl;

	guColorCombineFunction(GR_COLORCOMBINE_CCRGB);
	guAlphaSource(GR_ALPHASOURCE_CC_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	p = fleck_list;
	prev = PREV_OF(fleck_list, particle_t);
	while(p)
	{
		next = p->next;
		if(get_screen_pos(p->pos, scr) != -1.0f)
		{
			grConstantColorValue(*(unsigned int *)&p->b);
			size = scr[2] * 0.22f * proj_x;
			if(3.0f < size)
			{
				size = 3.0f;
			}
			size += 2097152.0f;
			size -= 2097152.0f;
			a.oow = b.oow = c.oow = scr[2];
			a.x = scr[0];
			a.y = scr[1];
			b.y = scr[1];
			c.x = scr[0];
			b.x = scr[0] + size;
			c.y = scr[1] + size * 0.5f;
			func_800d2dd4(&a, &b, &c);
		}
		p->pos[0] = p->pos[0] + p->vel[0];
		p->pos[1] = p->pos[1] + p->vel[1];
		p->pos[2] = p->pos[2] + p->vel[2];
		p->vel[1] = p->vel[1] + -0.0012f;
		r = p->r - 8;
		if(r < 0)
		{
			r = 0;
		}
		g = p->g - 8;
		if(g < 0)
		{
			g = 0;
		}
		bl = p->b - 8;
		if(bl < 0)
		{
			bl = 0;
		}
		if(r == 0 && g == 0 && bl == 0)
		{
			free(p);
			prev->next = next;
		}
		else
		{
			prev = p;
			prev->r = r;
			prev->g = g;
			prev->b = bl;
		}
		p = next;
	}
}

/* 0x8010074c */
void free_flecks(void)
{
	particle_t *p, *prev, *next;

	p = fleck_list;
	prev = PREV_OF(fleck_list, particle_t);
	while(p)
	{
		next = p->next;
		free(p);
		p = next;
		prev->next = p;
	}
}
