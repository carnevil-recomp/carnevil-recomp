/*
 * flash.c - the flash Mari throws in the Haunted House (0x80114df0-0x8011539c; the word at
 * 0x8011539c is padding before the next file). While her throw animation runs her draw
 * calls mari_throw_flash, which glows a FLASH sprite between her hands
 * (groups 7 and 13) and at frame 0x139 plays her "chastity" sound 0x1b and launches the
 * flash: an object (type 0x905) that flies at the camera and hurts every playing player when
 * it arrives, or, shot, drops, bounces and fades. Also the FLASH texmap loaders.
 */
#include <stdlib.h>
#include <glide/glide.h>
#include "model.h"
#include "enemy.h"
#include "objects.h"
#include "texmap.h"
#include "credits.h"			/* player_stats_t */
#include "levelsnd.h"

extern player_stats_t *get_target_player(void);			/* objects.c */
extern int	shot_hits_sphere(float *, float, player_stats_t *);	/* objects.c */
extern void	hurt_player(player_stats_t *, int, int, float);		/* player.c */
extern void	draw_point_sprite_over(int key, float *pos, float size, float alpha, unsigned int color);	/* draw2d.c */
extern int	charge_camera(enemy_t *, float, float, float, float);	/* enemy.c: steer at a point by the camera, 2 on arrival */

#define FLASH_TEX	0x2e00		/* FLASH01-04.WMS (flash_texmap) */
#define FLASH_OBJECT	0x905		/* its game object type */
#define FLASH_FLYING	1		/* ai_state: on its way to the camera */
#define FLASH_SHOT	2		/* ai_state: shot down, falling */
#define FLASH_DAMAGE	300		/* hurt_player: the damage ... */
#define HURT_WEAPON	3		/* ... and the kind (weapons.c) */
#define PLAYER_PLAYING	4		/* player_stats_t.state */

static void	draw_flash(game_object_t *);
static int	init_flash(game_object_t *);
static void	flash_think(game_object_t *);
static int	flash_hit(game_object_t *, player_stats_t *);
static int	flash_die(game_object_t *, player_stats_t *);
static void	remove_flash(game_object_t *);

texmap_t	flash_texmap[4] =		/* 0x80194658 */
{
	{ 0x2e00, 1, 0xffffff, 0, "FLASH01.WMS" },
	{ 0x2e01, 1, 0xffffff, 0, "FLASH02.WMS" },
	{ 0x2e02, 1, 0xffffff, 0, "FLASH03.WMS" },
	{ 0x2e03, 1, 0xffffff, 0, "FLASH04.WMS" },
};

void	launch_flash(float *pos, int frame);

/* 0x80114df0: Mari's throw, from her draw: the flash grows between her hands with the counter
   (the sprite frame is counter * 4 % 4, so always the first) and leaves them at frame 0x139 */
void mari_throw_flash(enemy_t *e)
{
	group_t	*g;
	float	pos[3];
	float	size, f;
	int	n;
	int	frame;

	g = e->groups;
	pos[0] = (g[7].pivot[0] + g[13].pivot[0]) * 0.5f;
	pos[1] = (g[7].pivot[1] + g[13].pivot[1]) * 0.5f;
	pos[2] = (g[7].pivot[2] + g[13].pivot[2]) * 0.5f;
	n = e->counter * 4;
	frame = n % 4;
	if(e->frame < 0x13a)
	{
		if(e->frame == 0x139)
		{
			play_enemy_sound(e, 0x1b, "chastity", 4);
			launch_flash(pos, frame);
		}
		else
		{
			f = n;
			size = f / 95.0f * 3.0f;
			grAlphaTestFunction(GR_CMP_GREATER);
			grAlphaTestReferenceValue(5);
			draw_point_sprite_over(FLASH_TEX + frame, pos, size, f * 2.0f, 0xffffff);
		}
	}
}

/* 0x80114f1c: launch a flash from pos at the target player, for 120 frames */
void launch_flash(float *pos, int frame)
{
	enemy_t	*e;
	short	id;

	id = add_object1_wrapper(FLASH_OBJECT, draw_flash, init_flash, flash_think);
	if(id)
	{
		e = get_object_data(id);
		e->x = pos[0];
		e->y = pos[1];
		e->z = pos[2];
		e->counter = 120;
		e->frame = frame;
		e->target = get_target_player();
		e->unk98 = 0.0f;
		e->rx = 0.0f;
		e->ry = 0.0f;
		e->rz = 0.0f;
		e->alpha = 255.0f;
	}
}

/* 0x80114fd8 */
static void draw_flash(game_object_t *o)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(5);
	draw_point_sprite_over(FLASH_TEX + e->frame, &e->x, 3.0f, 255.0f, 0xffffff);
}

/* 0x80115030: unreferenced */
void func_80115030(int unused0, player_stats_t *p, int unused2, enemy_t *e)
{
	e->counter = 0;
	e->killer = p;
}

/* 0x8011503c: a shot against the flash's sphere */
static int flash_hit(game_object_t *o, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	if(shot_hits_sphere(&e->x, e->radius * e->radius, p))
	{
		p->hit = 1;
		p->hit_id = o->id;
		p->hit_group = 0;
		p->die = (void *)flash_die;
		p->hit_point[0] = p->aim[0];
		p->hit_point[1] = p->aim[1];
		p->hit_point[2] = p->aim[2];
	}
	return 0;
}

/* 0x801150c8 */
static int flash_die(game_object_t *o, player_stats_t *p)
{
	return 0;
}

/* 0x801150d0: the record is a bare enemy, hit radius 1.2 */
static int init_flash(game_object_t *o)
{
	enemy_t	*e;

	o->data = malloc(sizeof(enemy_t));
	if(o->data == 0)
	{
		return 1;
	}
	o->hit = flash_hit;
	o->die = flash_die;
	o->remove = remove_flash;
	e = (enemy_t *)o->data;
	e->counter = 30;
	e->unk4 = 20;
	e->ai_state = FLASH_FLYING;
	e->num = 0;
	e->flags = 12;
	e->rx = 0.0f;
	e->ry = 0.0f;
	e->rz = 0.0f;
	e->x = 0.0f;
	e->y = 0.0f;
	e->z = 0.0f;
	e->radius = 1.2f;
	return 0;
}

/* 0x80115180 */
static void remove_flash(game_object_t *o)
{
	free(o->data);
}

/* 0x801151a4: flying, it cycles its four frames and steers at the camera; arriving it goes
   and hurts every player in the game. Shot down, it falls, bounces off its floor at half
   speed and fades out */
static void flash_think(game_object_t *o)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	switch(e->ai_state)
	{
	case FLASH_FLYING:
		if(++e->frame >= 4)
		{
			e->frame = 0;
		}
		if(charge_camera(e, 0.0f, 2.5f, 0.0f, 3.0f) == 2)
		{
			remove_game_object(o->id);
			if(player_stats[0].state == PLAYER_PLAYING)
			{
				hurt_player(&player_stats[0], FLASH_DAMAGE, HURT_WEAPON, 0.0f);
			}
			if(player_stats[1].state == PLAYER_PLAYING)
			{
				hurt_player(&player_stats[1], FLASH_DAMAGE, HURT_WEAPON, 0.0f);
			}
		}
		break;
	case FLASH_SHOT:
		e->x += e->goal[0];
		e->y += e->goal[1];
		e->z += e->goal[2];
		e->goal[1] += -0.23f;
		if(e->y < e->unk98)
		{
			e->y = e->unk98;
			e->goal[1] *= -0.5f;
		}
		e->alpha -= 5.0f;
		if(e->alpha <= 0.0f)
		{
			remove_game_object(o->id);
		}
		break;
	}
}

/* 0x8011533c */
void load_flash_texmap(void)
{
	load_texmap(flash_texmap, 4, "flas");
}

/* 0x8011536c: nothing to load */
void load_flash(void)
{
}

/* 0x80115374 */
void remove_flash_texmap(void)
{
	remove_surface(flash_texmap, 4);
}
