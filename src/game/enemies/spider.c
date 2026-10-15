/*
 * spider.c - the Haunted House spider (object type 0x2007, assets SPDR.ZM / spdr.za and the
 * spdrfur texmaps). One original file, 0x800f4618-0x800f56d0, tables at 0x8018f658 (texmaps)
 * and 0x8018fb18 (submeshes, spider_data.c), rodata from 0x80171df0. The model has 450
 * vertices, 626 normals and 756 faces in 36 submeshes, the animation 203 frames. It walks a
 * path compiled into .data (spider_path, 0x80180fb8) instead of a .pth file.
 *
 * Frames: 0-23 the walk, 44-93 the drop from the ceiling, 94-117 the turn, 118-135 the first
 * attack (it hurts the player at 128), 163-180 the second (at 173), 136-162 and 181-202 the two
 * deaths. States: 0 and 0x100 a first step, 0x101 walking at the player until it is close
 * (func_800fbba0 answers 2), 0x141/0x142 the two attacks, 0x102 walking back, 0x180 its path
 * round and round, 0x181 its path and then at the player, 0x2001 the drop, 0x401/0x402 the
 * deaths (an odd and an even spawn index die differently), 0x403 dead. At the top violence
 * level a shot takes off the limb it hits: eight routines each knock three groups loose.
 */
#include <stdio.h>
#include <stdlib.h>
#include <glide/glide.h>
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "texmap.h"
#include "credits.h"			/* debug_mode, player_stats_t */
#include "levelsnd.h"
#include "camera.h"			/* camera_x */

extern char	text_buffer[];
extern int	violence_level;

extern texmap_t	spider_texmap[38];		/* spider_data.c */
extern submesh_t submesh_info_spider[36];
extern float	spider_path[94][6];

extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);	/* text at a world position */
extern void	func_800c8938(float *, float, unsigned int);			/* a debug sphere */
extern void	func_800d14a4(float, float);					/* a debug map dot */
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern int	randrng(int);
extern void	path_place(enemy_t *, void *, short, float);
extern void	animate_loose_groups(enemy_t *, int);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	point_toward_camera(float *, float, float *);			/* objects.c */
extern int	step_toward_goal_xz(enemy_t *, float);
extern int	func_800fbba0(enemy_t *, float, float, float, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern void	detach_group(group_t *, float *, float, float *);		/* model.c: a part flies off */
extern int	mark_die();							/* spawn.c: (id, killer) */
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *);

float	*spider_verts;			/* 0x801e5bb4 */
face_t	*spider_faces;			/* 0x8022b100 */
float	*spider_normals;		/* 0x801e5880 */

#define SPIDER_GROUPS	0x24
#define SPIDER_NVERTS	0x1c2
#define SPIDER_NFACES	0x2f4
#define SPIDER_NNORMALS	0x272
#define SPIDER_FRAMES	0xcb
#define SPIDER_WALK_END	0x18		/* frames 0-23 walk */

/* the states */
#define SPIDER_START	0		/* a first step, then the first attack */
#define SPIDER_STEP	0x100		/* a first step, then walks at the player */
#define SPIDER_WALK	0x101		/* walks at the player until it is close */
#define SPIDER_BACK	0x102		/* walks back after an attack */
#define SPIDER_ATTACK1	0x141
#define SPIDER_ATTACK2	0x142
#define SPIDER_PATH_LOOP	0x180	/* its path round and round */
#define SPIDER_PATH	0x181		/* its path, then at the player */
#define SPIDER_DIE1	0x401
#define SPIDER_DIE2	0x402
#define SPIDER_DEAD	0x403
#define SPIDER_DROP	0x2001		/* drops from the ceiling */

int	func_800f4944(game_object_t *, player_stats_t *);
void	func_800f4dec(game_object_t *);
void	func_800f5278(enemy_t *, short);
void	spider_drop_limb1(enemy_t *);
void	spider_drop_limb2(enemy_t *);
void	spider_drop_limb4(enemy_t *);
void	spider_drop_limb5(enemy_t *);
void	spider_drop_limb7(enemy_t *);
void	spider_drop_limb8(enemy_t *);

/* 0x800f4618: the draw callback (type 0x2007) */
void func_800f4618(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	draw_model(e, submesh_info_spider, SPIDER_GROUPS, spider_verts, SPIDER_NVERTS, spider_faces, SPIDER_NFACES, spider_normals, SPIDER_NNORMALS);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		if(e->ai_state == SPIDER_ATTACK1)
		{
			draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffff00);
		}
		else
		{
			draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
		}
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->hit_group->pos, e->radius, 0x600000);
	}
	if(debug_mode & 0x20)
	{
		guAlphaSource(1);
		guColorCombineFunction(1);
		grConstantColorValue(0xffffff00);
		grAlphaBlendFunction(1, 5, 1, 5);
		func_800d14a4(e->x * 0.12f + 200.0f, e->z * 0.12f + 128.0f);
		grAlphaBlendFunction(4, 0, 4, 0);
	}
}

/* 0x800f47b0: the hit test: the bounding sphere, then the submesh */
int func_800f47b0(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		if((unsigned int)p->unk10 != 0xffff0000)
		{
			p->unk10 = 0xffffff00;
		}
		group = find_submesh_hit(e, submesh_info_spider, SPIDER_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
			p->hit_group = group;
			p->die = (void *)func_800f4944;
		}
	}
	return(0);
}

/* 0x800f4880: a given damage (nothing in the image calls it): once the health runs out, a
   cry, 1000 points and one of the two deaths, else 50 points */
void func_800f4880(void *unused, player_stats_t *p, float damage, enemy_t *e)
{
	if(!(e->ai_state & 0x400))
	{
		e->health -= damage;
		if(e->health <= 0.0f)
		{
			play_enemy_sound(e, 0x64, "hhouse1", 8);
			award_points(p->index, 1000);
			e->killer = p;
			if(e->num & 1)
			{
				e->ai_state = SPIDER_DIE1;
				e->frame = 0x88;
			}
			else
			{
				e->ai_state = SPIDER_DIE2;
				e->frame = 0xb5;
			}
		}
		else
		{
			award_points(p->index, 50);
		}
	}
}

/* 0x800f4944: the die callback: at the top violence level the limb hit comes off (or a
   part it cannot lose is marked); the damage as above; blood, 50 points, a splat and chunks
   whatever happens */
int func_800f4944(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;
	float	damage;
	int	group;

	e = (enemy_t *)obj->data;
	group = p->hit_group;
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 3: case 4: case 5: case 32:
			spider_drop_limb1(e);
			break;
		case 6: case 28: case 29: case 31:
			spider_drop_limb2(e);
			break;
		case 7: case 8: case 23: case 30:
			spider_drop_limb4(e);
			break;
		case 0: case 1: case 2: case 22:
			spider_drop_limb5(e);
			break;
		case 14: case 15: case 16: case 17:
			spider_drop_limb7(e);
			break;
		case 10: case 11: case 12: case 13:
			spider_drop_limb8(e);
			break;
		case 18: case 19: case 20: case 21:
		case 24: case 25: case 26: case 27:
			break;
		default:
			g = &e->groups[group];
			g->flags |= 0xf0000000;
			break;
		}
	}
	damage = shot_damage_of(p);
	if(!(e->ai_state & 0x400))
	{
		e->health -= damage;
		if(e->health <= 0.0f)
		{
			play_enemy_sound(e, 0x64, "hhouse1", 8);
			award_points(p->index, 1000);
			e->killer = p;
			if(e->num & 1)
			{
				e->ai_state = SPIDER_DIE1;
				e->frame = 0x88;
			}
			else
			{
				e->ai_state = SPIDER_DIE2;
				e->frame = 0xb5;
			}
		}
		else
		{
			award_points(p->index, 50);
		}
	}
	burst_blood(p->hit_point, e->y, 0x800000);
	award_points(p->index, 50);
	add_world_sprite(p->hit_point, 0, 0);
	add_chunks(p->hit_point, e->y);
	return(0);
}

/* 0x800f4b18 */
void load_spider_texmap(void)
{
	load_texmap(spider_texmap, 0x26, "spdr");
}

/* 0x800f4b48: the model and the animation (the path is in .data) */
void load_spider(void)
{
	load_model((void **)&spider_verts, SPIDER_NVERTS, (void **)&spider_faces, SPIDER_NFACES, (void **)&spider_normals, SPIDER_NNORMALS, "SPDR.ZM", 0xcad33308);
	load_anim(submesh_info_spider, SPIDER_GROUPS, SPIDER_FRAMES, "spdr.za", 0x4518d413);
}

/* 0x800f4bd0 */
void remove_spider_assets(void)
{
	remove_surface(spider_texmap, 0x26);
	remove_animation(submesh_info_spider, SPIDER_GROUPS, SPIDER_FRAMES, "spdr.za");
	remove_model((void **)&spider_verts, SPIDER_NVERTS, (void **)&spider_faces, SPIDER_NFACES, (void **)&spider_normals, SPIDER_NNORMALS, "SPDR.ZM");
}

/* 0x800f4c50: the init callback (type 0x2007): the record, 36 groups, 15.6 health (24 with
   two players in), a hiss, a first step */
int func_800f4c50(game_object_t *obj)
{
	enemy_t	*e;
	group_t	*g;
	void	*data;
	int	i;

	data = malloc(sizeof(enemy_t));
	obj->data = data;
	if(!data)
	{
		return(1);
	}
	obj->hit = func_800f47b0;
	obj->die = func_800f4944;
	obj->remove = func_800f4dec;
	e = (enemy_t *)data;
	e->counter = 0;
	e->num = 0;
	e->flags = 0xc;
	e->target = get_target_player();
	e->rx = 0;
	e->ry = 0;
	e->rz = 0;
	e->x = 0;
	e->y = 0;
	e->z = 0;
	e->unkcc = 1.0f;
	set_enemy_scale(e, 1.0f);
	e->vel[0] = 0;
	e->vel[1] = 0;
	e->vel[2] = 0;
	if((player_stats[0].state & 4) && (player_stats[1].state & 4))
	{
		set_enemy_health(e, 24.0f);
	}
	else
	{
		set_enemy_health(e, 15.599999f);	/* 0x41799999, just under 15.6 */
	}
	e->groups = malloc(SPIDER_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < SPIDER_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	init_groups(e, submesh_info_spider, SPIDER_GROUPS);
	g = e->groups;
	e->hit_group = &g[34];
	g[34].floor = 0.0f;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	func_800f5278(e, SPIDER_WALK);
	play_enemy_sound(e, 0x63, "hhouse1", 8);
	return(0);
}

/* 0x800f4dec: the remove callback */
void func_800f4dec(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	free(e->groups);
	free(e);
}

/* 0x800f4e24: the think callback */
void func_800f4e24(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	animate_loose_groups(e, SPIDER_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case SPIDER_PATH:
		if(++e->frame >= SPIDER_WALK_END)
		{
			e->frame = 0;
		}
		path_place(e, spider_path, e->path, 1.0f);
		if(++e->path > e->unk92)
		{
			e->target = get_target_player();
			e->frame = 0;
			e->ai_state = SPIDER_WALK;
		}
		break;
	case SPIDER_PATH_LOOP:
		if(++e->frame >= SPIDER_WALK_END)
		{
			e->frame = 0;
		}
		path_place(e, spider_path, e->path, 1.0f);
		if(++e->path > e->unk92)
		{
			e->path = e->unk90;
		}
		break;
	case SPIDER_DROP:
		if(++e->frame >= 0x5e)
		{
			e->frame = 0x2c;
		}
		break;
	case SPIDER_START:
		if(++e->frame >= SPIDER_WALK_END)
		{
			e->frame = 0x76;
			e->ai_state = SPIDER_ATTACK1;
		}
		break;
	case SPIDER_STEP:
		if(++e->frame >= SPIDER_WALK_END)
		{
			e->frame = 0;
			e->ai_state = SPIDER_WALK;
		}
		break;
	case SPIDER_WALK:
		if(++e->frame >= SPIDER_WALK_END)
		{
			e->frame = 0;
		}
		if(func_800fbba0(e, 0.0f, 0.0f, -22.5f, 2.0f) == 2)
		{
			e->frame = 0x76;
			e->ai_state = SPIDER_ATTACK1;
		}
		break;
	case SPIDER_DIE1:
		if(++e->frame >= 0xa3)
		{
			e->frame--;
			e->ai_state = SPIDER_DEAD;
			if(debug_mode & 1)
			{
				remove_game_object(obj->id);
			}
			else
			{
				mark_die(obj->id, e->killer);
			}
		}
		break;
	case SPIDER_DIE2:
		if(++e->frame >= SPIDER_FRAMES)
		{
			e->frame--;
			e->ai_state = SPIDER_DEAD;
			mark_die(obj->id, e->killer);
		}
		break;
	case SPIDER_ATTACK1:
		if(e->frame == 0x80)
		{
			hurt_player(e->target, 0x96, 7, 0);
		}
		if(++e->frame >= 0x88)
		{
			e->frame = 0xa3;
			e->ai_state = SPIDER_ATTACK2;
		}
		break;
	case SPIDER_ATTACK2:
		if(e->frame == 0xad)
		{
			hurt_player(e->target, 0x96, 4, 0);
		}
		if(++e->frame >= 0xb5)
		{
			e->frame = 0x5e;
			e->ai_state = SPIDER_BACK;
			point_toward_camera(&camera_x, -60.0f, e->goal);
		}
		break;
	case SPIDER_BACK:
		if(++e->frame >= SPIDER_WALK_END)
		{
			e->frame = 0;
		}
		if(step_toward_goal_xz(e, 2.0f))
		{
			e->target = get_target_player();
			e->frame = 0;
			e->ai_state = SPIDER_WALK;
		}
		break;
	case SPIDER_DEAD:
		break;
	}
}

/* 0x800f5244: nothing in the image calls it: a random number thrown away when state is
   negative, then state 0 */
void func_800f5244(enemy_t *e, int state)
{
	if(state < 0)
	{
		randrng(100);
	}
	e->ai_state = 0;
}

/* 0x800f5278: the set-up the spawn table asks for (type 0x2007) */
void func_800f5278(enemy_t *e, short state)
{
	e->ai_state = state;
	switch(state)
	{
	case SPIDER_DROP:
		e->frame = 0x2c;
		break;
	case SPIDER_STEP:
		e->frame = 1;
		break;
	case SPIDER_PATH_LOOP:
	case SPIDER_PATH:
		e->frame = 0;
		path_place(e, spider_path, e->path, 1.0f);
		e->counter = 0;
		break;
	case SPIDER_WALK:
		e->frame = 0;
		break;
	}
}

/* 0x800f5314-0x800f5658: a limb comes off, three groups each (the joint's pivot is where they
   fly from); limbs 3 and 6 are never called */
void spider_drop_limb1(enemy_t *e)
{
	detach_group(&e->groups[5], e->groups[3].pivot, e->y, e->scale);
	detach_group(&e->groups[3], e->groups[3].pivot, e->y, e->scale);
	detach_group(&e->groups[4], e->groups[3].pivot, e->y, e->scale);
}

void spider_drop_limb2(enemy_t *e)
{
	detach_group(&e->groups[28], e->groups[6].pivot, e->y, e->scale);
	detach_group(&e->groups[6], e->groups[6].pivot, e->y, e->scale);
	detach_group(&e->groups[29], e->groups[6].pivot, e->y, e->scale);
}

void spider_drop_limb3(enemy_t *e)
{
	detach_group(&e->groups[24], e->groups[26].pivot, e->y, e->scale);
	detach_group(&e->groups[26], e->groups[26].pivot, e->y, e->scale);
	detach_group(&e->groups[25], e->groups[26].pivot, e->y, e->scale);
}

void spider_drop_limb4(enemy_t *e)
{
	detach_group(&e->groups[8], e->groups[30].pivot, e->y, e->scale);
	detach_group(&e->groups[30], e->groups[30].pivot, e->y, e->scale);
	detach_group(&e->groups[7], e->groups[30].pivot, e->y, e->scale);
}

void spider_drop_limb5(enemy_t *e)
{
	detach_group(&e->groups[0], e->groups[2].pivot, e->y, e->scale);
	detach_group(&e->groups[2], e->groups[2].pivot, e->y, e->scale);
	detach_group(&e->groups[1], e->groups[2].pivot, e->y, e->scale);
}

void spider_drop_limb6(enemy_t *e)
{
	detach_group(&e->groups[18], e->groups[20].pivot, e->y, e->scale);
	detach_group(&e->groups[20], e->groups[20].pivot, e->y, e->scale);
	detach_group(&e->groups[19], e->groups[20].pivot, e->y, e->scale);
}

void spider_drop_limb7(enemy_t *e)
{
	detach_group(&e->groups[14], e->groups[16].pivot, e->y, e->scale);
	detach_group(&e->groups[16], e->groups[16].pivot, e->y, e->scale);
	detach_group(&e->groups[15], e->groups[16].pivot, e->y, e->scale);
}

void spider_drop_limb8(enemy_t *e)
{
	detach_group(&e->groups[10], e->groups[12].pivot, e->y, e->scale);
	detach_group(&e->groups[12], e->groups[12].pivot, e->y, e->scale);
	detach_group(&e->groups[11], e->groups[12].pivot, e->y, e->scale);
}
