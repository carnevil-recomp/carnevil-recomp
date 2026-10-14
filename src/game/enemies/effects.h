/*
 * effects.h - the effects attached to an enemy (blood, sparks, a part flying off): a list of
 * records on enemy_t.effects, each with its own draw, think and remove callbacks.
 */
#ifndef EFFECTS_H
#define EFFECTS_H

#include "enemy.h"
#include "model.h"

typedef struct effect
{
	int		type;			/* 0x00 */
	void		(*draw)(struct effect *, void *, struct group *);	/* 0x04: (the enemy's list, param, group) */
	void		(*think)(enemy_t *, void *);				/* 0x08 */
	void		(*remove)(enemy_t *, void *);				/* 0x0c */
	struct group	*group;			/* 0x10: the group it rides */
	void		*param;			/* 0x14: its own record */
	struct effect	*next;			/* 0x18 */
} effect_t;					/* sizeof = 0x1c */

/* the particle lists (effects.c) */
typedef struct drop				/* a blood drop (0x28) */
{
	float		pos[3], vel[3];
	int		life;			/* +0x18 */
	float		floor;			/* +0x1c */
	struct drop	*next;			/* +0x20 */
	int		unk24;
} drop_t;

typedef struct particle				/* a spark or a fleck (0x20): a colour word at +0x18 */
{
	float		pos[3], vel[3];
	unsigned char	b, g, r, a;		/* +0x18: the constant colour the triangle is drawn in */
	struct particle	*next;			/* +0x1c */
} particle_t;

typedef struct smoke				/* a column of smoke (0x70) */
{
	float		pos[3];
	float		t;			/* +0x0c: the texture row at its foot, scrolling up */
	float		height;			/* +0x10: in texture rows */
	float		alpha;			/* +0x14 */
	float		alpha_dir;		/* +0x18: 1 rising, -1 fading */
	unsigned int	color;			/* +0x1c */
	float		top[4][3];		/* +0x20: the column's corners (the template scaled) */
	float		base[2][3];		/* +0x50: the foot's corners */
	float		size;			/* +0x68 */
	struct smoke	*next;			/* +0x6c */
} smoke_t;

typedef struct snowflake			/* 0x20 */
{
	float		pos[3], vel[3];
	int		life;			/* +0x18 */
	struct snowflake *next;			/* +0x1c */
} snowflake_t;

typedef struct spray				/* the blood spray effect's record (0x28) */
{
	float		base[3];		/* +0x00: where it springs from, on the group */
	float		dir[3];			/* +0x0c: the way it goes, on the group */
	group_t		*group;			/* +0x18 */
	int		unk1c;
	unsigned int	color;			/* +0x20 */
	int		frame;			/* +0x24: 0..11 through the pulse table */
} spray_t;

typedef struct flame				/* the flame effect's record (0x10) */
{
	float		scale;			/* +0x00 */
	float		y;			/* +0x04 */
	int		unk8;
	int		frame;			/* +0x0c: 0..4 */
} flame_t;

void	attach_effect(enemy_t *e, effect_t *ef);
void	draw_effects(enemy_t *e);
void	think_effects(enemy_t *e);
void	free_effects(enemy_t *e);
void	detach_effect(enemy_t *e, group_t *g);
void	remove_effects_of_type(enemy_t *e, int type);

void	burst_blood(float *pos, float floor);
void	add_chunks(float *pos, float floor);
void	add_chunks_n(float *pos, float floor, int n);
void	remove_chnk_model(void);
void	draw_chunks(void);
void	free_chunks(void);
void	init_blood_drops(void);
void	add_blood_drop(float *pos, float *vel, float floor);
void	draw_blood_drops(void);
void	free_blood_drops(void);
effect_t *attach_spray(float *base, float *dir, group_t *g);
effect_t *attach_flame(float scale, float y, group_t *g);
void	init_sparks(void);
void	add_spark(float *pos);
void	draw_sparks(void);
void	free_sparks(void);
void	init_smoke(void);
void	add_smoke(float *pos, unsigned int color);
void	draw_smoke(void);
void	free_smoke(void);
void	add_snow_around(float *pos);
void	malloc_snow(float *pos, float *vel);
void	init_snow(void);
void	draw_snow(void);
void	free_snow(void);
void	blow_snow(float *from, float *to);
void	spray_flecks(float *pos, int r, int g, int b);
void	fleck_enemy(enemy_t *e, int r, int g, int b);
void	burst_flecks(float *pos, int r, int g, int b);
void	init_flecks(void);
void	add_fleck(float *pos, float *vel, int r, int g, int b);
void	draw_flecks(void);
void	free_flecks(void);

#endif
