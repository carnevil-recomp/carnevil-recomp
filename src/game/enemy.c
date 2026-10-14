/*
 * enemy.c - helpers shared by the enemy types: health by the health adjustment, placing an
 * enemy on a path point and following a path, and the reach test for an attack. One original
 * file (0x800fb310-0x800fd100); the steering and facing routines in it are still assembly.
 */
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "credits.h"			/* player_stats */
#include "camera.h"			/* camera_x/y/z */

extern int	D_80237438;		/* adjust.c: adjustment 0x32 (0..4) */

/* 0x800fb4a0: the enemy's health scaled by adjustment 0x32 */
void set_enemy_health(enemy_t *e, float health)
{
	switch(D_80237438)
	{
	case 0:
		e->health = health * 0.5f;
		break;
	case 1:
		e->health = health * 0.75f;
		break;
	case 2:
		e->health = health;
		break;
	case 3:
		e->health = health * 1.25f;
		break;
	case 4:
		e->health = health * 1.5f;
		break;
	default:
		e->health = health;
		break;
	}
}

/* 0x800fb51c: the same with a flatter scale (0.8, 0.9, 1, 1.25, 1.5) */
void set_enemy_health2(enemy_t *e, float health)
{
	switch(D_80237438)
	{
	case 0:
		e->health = health * 0.8f;
		break;
	case 1:
		e->health = health * 0.9f;
		break;
	case 2:
		e->health = health;
		break;
	case 3:
		e->health = health * 1.25f;
		break;
	case 4:
		e->health = health * 1.5f;
		break;
	default:
		e->health = health;
		break;
	}
}

/* 0x800fb70c: put the enemy on path point n: a position (scaled) and three angles */
void path_place(enemy_t *e, float *path, int n, float scale)
{
	float	*p;

	p = path + n * 6;
	e->x = p[0] * scale;
	e->y = p[1] * scale;
	e->z = p[2] * scale;
	e->rx = p[3];
	e->ry = p[4];
	e->rz = p[5];
}

/* 0x800fb760: the same without the roll */
void path_place_upright(enemy_t *e, float *path, int n, float scale)
{
	float	*p;

	p = path + n * 6;
	e->x = p[0] * scale;
	e->y = p[1] * scale;
	e->z = p[2] * scale;
	e->rx = p[3];
	e->ry = p[4];
	e->rz = 0;
}

/* 0x800fb7b0: path_place, then advance along the path while a player is in the game */
void path_follow(enemy_t *e, float *path, int n, float scale)
{
	float	*p;

	p = path + n * 6;
	e->x = p[0] * scale;
	e->y = p[1] * scale;
	e->z = p[2] * scale;
	e->rx = p[3];
	e->ry = p[4];
	e->rz = p[5];
	if((player_stats[0].state & 0x44) || (player_stats[1].state & 0x44))
	{
		e->path++;
	}
}

/* 0x800fb83c: path_follow facing the other way */
void path_follow_turned(enemy_t *e, float *path, int n, float scale)
{
	float	*p;

	p = path + n * 6;
	e->x = p[0] * scale;
	e->y = p[1] * scale;
	e->z = p[2] * scale;
	e->rx = p[3];
	e->ry = p[4] + 3.1415927f;
	e->rz = p[5];
	if((player_stats[0].state & 0x44) || (player_stats[1].state & 0x44))
	{
		e->path++;
	}
}

/* 0x800fb8d4: path_place facing the other way */
void path_place_turned(enemy_t *e, float *path, int n, float scale)
{
	float	*p;

	p = path + n * 6;
	e->x = p[0] * scale;
	e->y = p[1] * scale;
	e->z = p[2] * scale;
	e->rx = p[3];
	e->ry = p[4] + 3.1415927f;
	e->rz = p[5];
}

/* 0x800fcfc4: a visible group within r of the view point (an attack lands) */
int group_near_camera(group_t *g, float r)
{
	if(!(g->flags & 0x2000))
	{
		return(0);
	}
	if((g->pos[0] - camera_x) * (g->pos[0] - camera_x) + (g->pos[1] - camera_y) * (g->pos[1] - camera_y) + (g->pos[2] - camera_z) * (g->pos[2] - camera_z) < r * r)
	{
		return(1);
	}
	return(0);
}
