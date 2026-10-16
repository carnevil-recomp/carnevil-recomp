/*
 * teen.c - the Rickety Town teens (object type 0x2045, assets TEEN.ZM / teen.za and the
 * 69 "teen" texmaps). One original file, 0x8012b7d8-0x8012d4a0, tables at 0x8019c580
 * (texmaps), 0x8019ce20 (submeshes, teen_data.c) and 0x8019d0c0 (sounds, below). The model
 * has 918 vertices, 1007 normals and 1505 faces in 21 submeshes; the animation has 604
 * frames, shared by every action (each state plays its own stretch of it). Five submeshes
 * (15-19) are hidden at first: they are the damaged versions a shot swaps in
 * (replace_group), and they borrow the animation of the parts they replace. A teen walks at
 * the player (1) and strikes (0x201), throws food or a present from a hand (0x202, 0x203,
 * 0x205), dances or idles (2-7), waits (9) until the level script makes it melt and sink
 * (0xa, 0xb), and dies in one of two ways (0x2001, 0x2002). Each teen speaks with one of two
 * voices (the "teena" and "teenb" sound banks).
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

extern void	draw_effects(enemy_t *);			/* run the attached effects */
/* weapons.c (weapons.h is not included: it brings effects.h, whose burst_blood takes two
   arguments where this file passes three) */
extern void	throw_at_camera(float *, int, float *);
extern void	*attach_held_object(group_t *, int, float, float, float, float, float, float, float *);
#define WEAPON_FOOD	0x20			/* weapons.h: the four foods are 0x20-0x23, the present 0x24 */
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);	/* a string at a world position */
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);		/* objects.c */
extern void	set_enemy_health(enemy_t *, float);		/* health by difficulty */
extern player_stats_t *get_target_player(void);		/* objects.c */
extern int	randrng(int);
extern void	attach_effect(enemy_t *, void *);
extern void	*attach_flame(float, float, group_t *);	/* a flame on a group */
extern void	detach_effect(enemy_t *, group_t *);		/* drop the effect on a group */
extern void	detach_group(group_t *, float *, float, float *);	/* knock a group loose */
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern int	face_camera(enemy_t *, float);			/* enemy.c */
extern int	group_near_camera(group_t *, float);		/* a group within reach of the player */
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);	/* blend to a frame */
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);	/* hit reaction */
extern void	pick_random_action(enemy_t *, int *, submesh_t *, int, int);	/* a random action from a chance table */
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal_xz(enemy_t *, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);	/* the shot hits the sphere */
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
/* the teen's file passes burst_blood a third argument (0x800000) that the termite's does not;
   the routine (effects.c) reads only two */
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);	/* sprites.c */
extern void	add_smoke(float *, unsigned int);
extern void	replace_group(enemy_t *, group_t *, group_t *);	/* swap a shot part for its damaged version */
extern float	shot_damage_of(player_stats_t *);		/* the shot's damage */
extern void	award_points(int, int);			/* award points */
extern int	mark_die();					/* spawn.c; the original passes the player too */
extern int	remove_die();
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *);
extern enemy_t	*debug_enemy;
extern char	text_buffer[];
extern int	violence_level;
extern int	D_801e704c;					/* adjust.c: adjustment 0x3d */

float	*teen_verts;			/* 0x801e5b18 */
face_t	*teen_faces;			/* 0x80212e8c */
float	*teen_normals;			/* 0x801e57c0 */
enemy_t	*waiting_teen;			/* 0x802372dc: the last teen put in the waiting state (9) */

extern texmap_t	teen_texmap[0x45];		/* teen_data.c */
extern submesh_t submesh_info_teen[0x15];

/* the voices: the "teena" and "teenb" banks' cues (0x8019d0c0) */
int	teena_hurt_sounds[16] = { 20, 21, 22, 24, 25, 27, 28, 29, 30, 32, 33, 36, 38, 39, 41, 42 };	/* 0x8019d0c0 */
int	teena_action_sounds[3] = { 13, 14, 19 };				/* 0x8019d100 */
int	teena_walk_sounds[7] = { 1, 3, 7, 8, 11, 15, 17 };			/* 0x8019d10c */
int	teenb_hurt_sounds[11] = { 17, 18, 19, 20, 21, 22, 24, 25, 26, 27, 28 };	/* 0x8019d128 */
int	teenb_action_sounds[5] = { 9, 14, 32, 33, 35 };				/* 0x8019d154 */
int	teenb_walk_sounds[4] = { 2, 4, 7, 13 };					/* 0x8019d168 */
/* 0x8019d178: the random actions after one ends (pick_random_action): pairs of a cumulative
   chance and (frame << 16 | state): 25% state 4, then 6 from frame 0x4f, 5 from 0x39, 7
   from 0x81 */
int	teen_actions[10] = { 25, 4, 50, 0x4f0006, 75, 0x390005, 100, 0x810007, 100, 0x810007 };

/* the states */
#define TEEN_IDLE	0		/* frames 0-0x25b */
#define TEEN_WALK	1		/* at the player, 0x1f9-0x204 */
#define TEEN_DANCE	2		/* loops 0x72-0x80 */
#define TEEN_ACTION3	3		/* the actions, each a stretch of the animation */
#define TEEN_ACTION4	4		/* swaps part 13 for 17 and back */
#define TEEN_ACTION5	5
#define TEEN_ACTION6	6
#define TEEN_ACTION7	7
#define TEEN_FLEE	8		/* runs off, then removed */
#define TEEN_WAIT	9		/* loops 0x1c4-0x1dc until func_8012d43c */
#define TEEN_MELT	0xa
#define TEEN_SINK	0xb
#define TEEN_STRIKE	0x201
#define TEEN_THROW_FOOD	0x202		/* food from the right hand (group 10) */
#define TEEN_THROW	0x203		/* a random food or present from the right hand */
#define TEEN_THROW_LEFT	0x205		/* the same from the left hand (group 4) */
#define TEEN_DIE1	0x2001
#define TEEN_DIE2	0x2002
#define TEEN_GONE	0x4000

int	teen_die(game_object_t *, player_stats_t *);
void	teen_remove(game_object_t *);
void	teen_setup(enemy_t *, unsigned short);
void	teen_steer(enemy_t *);
void	teen_next_action(enemy_t *);

/* 0x8012b7d8: the draw callback */
void teen_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_teen, 0x15, teen_verts, 0x396, teen_faces, 0x5e1, teen_normals, 0x3ef);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[11].pos, e->radius, 0x600000);
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
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		if(e == debug_enemy)
		{
			draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xff0000);
		}
		else
		{
			draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
		}
	}
	if(debug_mode & 0x1000)
	{
		func_800c99e4(e, 0xff);
	}
}

/* 0x8012b9d4: the hit test: the bounding sphere, then the submesh */
int teen_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_teen, 0x15, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)teen_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x8012ba90: the shot lands: blood; the acid gun burns every part, the flame thrower sets the
   part alight; at the highest violence the part bleeds or is swapped for its damaged
   version (the head takes two shots); damage, points and a voice; death or a hit reaction
   chosen by the part. A teen marked 0x80 by its spawn entry only flinches (from state 9) */
int teen_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;
	int	group;
	int	i;

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return(0);
	}
	burst_blood(p->hit_point, e->unk98, 0x800000);
	add_chunks(e->hit_group->pos, e->unk98);
	add_world_sprite(p->hit_point, 0, 0);
	group = p->hit_group;
	g = &e->groups[group];
	if(p->shot_weapon == 0x1000005)
	{
		for(i = 0; i < 0x15; i++)
		{
			e->groups[i].flags |= 0x20;
			add_smoke(e->groups[i].pivot, 0xd0ff00);
		}
		e->health = 0.01f;
	}
	if(p->shot_weapon == 0x1000004)
	{
		if(!(g->flags & 2))
		{
			g->flags |= 2;
			attach_effect(e, attach_flame(0.0f, (submesh_info_teen[group].radius + submesh_info_teen[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 1:
		case 5:
		case 7:
		case 9:
			break;
		case 12:
		case 20:
			e->unka8++;
			switch(e->unka8)
			{
			case 0:
				printf("This should never happen! Zomb head damage at 0\n");
				break;
			case 1:
				g = &e->groups[group];
				g->flags |= 0xf0000000;
				e->groups[12].flags |= 0xf0000000;
				break;
			case 2:
				replace_group(e, &e->groups[20], &e->groups[18]);
				break;
			}
			break;
		case 11:
			if(g->flags & 0x20000000)
			{
				break;
			}
			if(g->flags & 0x10000000)
			{
				g->flags = (g->flags & ~0x10000000) | 0x20000000;
				if(randrng(100) < 20)
				{
					replace_group(e, &e->groups[11], &e->groups[19]);
				}
				break;
			}
			g->flags |= 0x10000000;
			break;
		case 6:
			if(g->flags & 0xf0000000)
			{
				replace_group(e, &e->groups[6], &e->groups[16]);
				break;
			}
			g->flags |= 0xf0000000;
			e->groups[7].flags |= 0xf0000000;
			break;
		case 2:
			if(g->flags & 0xf0000000)
			{
				replace_group(e, &e->groups[2], &e->groups[15]);
				break;
			}
			g->flags |= 0xf0000000;
			e->groups[1].flags |= 0xf0000000;
			break;
		case 14:
			g->flags |= 0xf0000000;
			e->groups[9].flags |= 0xf0000000;
			break;
		case 3:
			g->flags |= 0xf0000000;
			e->groups[5].flags |= 0xf0000000;
			break;
		default:
			g->flags |= 0xf0000000;
			break;
		}
	}
	if(e->flags2 & 0x80)
	{
		if(e->ai_state == TEEN_WAIT || e->ai_state == 0x100a)
		{
			start_hit_reaction(e, 0x205, 0x100a, submesh_info_teen, 0xd);
		}
		return(0);
	}
	if(!(e->ai_state & 0x6000))
	{
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			award_points(p->index, 500);
			if(e->unkb0 == 1)
			{
				play_enemy_sound(e, 0x2c, "teena", 8);
			}
			else
			{
				play_enemy_sound(e, 0x17, "teenb", 8);
			}
			e->killer = p;
			if(!(e->flags2 & 0x20))
			{
				mark_die(obj->id, p);
			}
			if(!(e->ai_state & 0x400))
			{
				if(e->flags2 & 0x40)
				{
					e->ai_state = TEEN_DIE2;
					e->frame = 0x248;
					e->flags &= 0xfffd;
				}
				else
				{
					e->ai_state = TEEN_DIE1;
					e->frame = 0x106;
					e->flags &= 0xfffd;
				}
			}
		}
		else
		{
			award_points(p->index, 50);
			if(e->unkb0 == 1)
			{
				play_enemy_sound(e, teena_hurt_sounds[randrng(16)], "teena", 2);
			}
			else
			{
				play_enemy_sound(e, teenb_hurt_sounds[randrng(11)], "teenb", 2);
			}
		}
	}
	if(e->ai_state & 0xe000)
	{
		return(0);
	}
	switch(group)
	{
	case 12:
	case 18:
	case 20:
		i = randrng(100);
		if(i < 50)
		{
			start_hit_reaction(e, 0xf4, 0x1008, submesh_info_teen, 0xd);
		}
		else
		{
			start_hit_reaction(e, 0xf4, 0x1009, submesh_info_teen, 0xd);
		}
		break;
	case 13:
		start_hit_reaction(e, 0xbb, 0x1002, submesh_info_teen, 0xd);
		break;
	case 3:
	case 4:
	case 5:
		start_hit_reaction(e, 0xe2, 0x1004, submesh_info_teen, 0xd);
		break;
	case 9:
	case 10:
	case 14:
		start_hit_reaction(e, 0xcf, 0x1005, submesh_info_teen, 0xd);
		break;
	case 17:
		break;
	default:
		start_hit_reaction(e, 0xa4, 0x1001, submesh_info_teen, 0xd);
		break;
	}
	return(0);
}

/* 0x8012c0bc */
void load_teen_texmap(void)
{
	load_texmap(teen_texmap, 0x45, "teen");
}

/* 0x8012c0ec: the model and the animation; the damaged parts 15-19 play the animation of
   the parts they replace (2, 6, 13, 20 and 11), so their own is dropped */
void load_teen(void)
{
	load_model((void **)&teen_verts, 0x396, (void **)&teen_faces, 0x5e1, (void **)&teen_normals, 0x3ef, "TEEN.ZM", 0x2f6e690e);
	load_anim(submesh_info_teen, 0x15, 0x25c, "teen.za", 0x4a59f73d);
	free(submesh_info_teen[18].anim);
	free(submesh_info_teen[15].anim);
	free(submesh_info_teen[16].anim);
	free(submesh_info_teen[17].anim);
	free(submesh_info_teen[19].anim);
	submesh_info_teen[18].anim = submesh_info_teen[20].anim;
	submesh_info_teen[15].anim = submesh_info_teen[2].anim;
	submesh_info_teen[16].anim = submesh_info_teen[6].anim;
	submesh_info_teen[17].anim = submesh_info_teen[13].anim;
	submesh_info_teen[19].anim = submesh_info_teen[11].anim;
}

/* 0x8012c214: the borrowed animations are forgotten before the rest is freed */
void remove_teen_assets(void)
{
	submesh_info_teen[18].anim = 0;
	submesh_info_teen[15].anim = 0;
	submesh_info_teen[16].anim = 0;
	submesh_info_teen[17].anim = 0;
	submesh_info_teen[19].anim = 0;
	remove_surface(teen_texmap, 0x45);
	remove_animation(submesh_info_teen, 0x15, 0x25c, "teen.za");
	remove_model((void **)&teen_verts, 0x396, (void **)&teen_faces, 0x5e1, (void **)&teen_normals, 0x3ef, "TEEN.ZM");
}

/* 0x8012c2c4: the init callback: the record, the groups (the damaged parts hidden), the
   "teena" voice, the first state */
int teen_init(game_object_t *obj)
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
	obj->hit = teen_hit;
	obj->die = teen_die;
	obj->remove = teen_remove;
	e = (enemy_t *)data;
	e->unk4 = 0x14;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0xc;
	e->flags2 = 0;
	e->target = get_target_player();
	e->unk54 = 0;
	e->rx = 0;
	e->rz = 0;
	e->y = 0;
	e->z = 0;
	e->ry = 3.1415927f;
	e->x = 24.0f;
	e->unkcc = 0.8f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 5.5f);
	e->groups = malloc(0x15 * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < 0x15; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[18].flags = 0;
	e->groups[15].flags = 0;
	e->groups[16].flags = 0;
	e->groups[17].flags = 0;
	e->groups[19].flags = 0;
	e->unka8 = 0;
	e->unkac = 0;
	e->unkb0 = 1;
	init_groups(e, submesh_info_teen, 0x15);
	g = e->groups;
	e->hit_group = &g[13];
	g[13].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	teen_setup(e, 0);
	return(0);
}

/* 0x8012c480: the remove callback */
void teen_remove(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(debug_enemy == e)
	{
		debug_enemy = 0;
	}
	free_effects(e);
	free(e->groups);
	free(e);
}

/* a hit reaction plays to its last frame, then the teen blends back to what it was doing */
#define HIT_REACTION(last) \
	frame = e->frame; \
	e->frame = frame + 1; \
	if(e->frame < last) \
	{ \
		break; \
	} \
	e->frame = frame; \
	blend_to_frame(e, submesh_info_teen, 0xd, e->resume_frame, 5); \
	e->ai_state = e->resume_state; \
	break;

/* 0x8012c4d8: the think callback */
void teen_think(game_object_t *obj)
{
	enemy_t	*e;
	short	frame;
	int	i;

	if(debug_mode & 0x10000)
	{
		return;
	}
	if(debug_mode & 0x20000)
	{
		return;
	}
	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, 0x15);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1008:
		HIT_REACTION(0x106)
	case 0x1009:
		HIT_REACTION(0x106)
	case 0x1001:
		HIT_REACTION(0xbb)
	case 0x1002:
		HIT_REACTION(0xcf)
	case 0x1004:
		HIT_REACTION(0xf4)
	case 0x1005:
		HIT_REACTION(0xe2)
	case 0x100a:
		HIT_REACTION(0x215)
	case TEEN_WALK:
		e->frame++;
		if(e->frame >= 0x205)
		{
			e->frame = 0x1f9;
		}
		face_camera(e, 0.2f);
		teen_steer(e);
		break;
	case TEEN_STRIKE:
		face_camera(e, 0.2f);
		switch(e->frame++)
		{
		case 0x11c:
		case 0x120:
		case 0x122:
		case 0x125:
		case 0x127:
		case 0x12a:
		case 0x12d:
		case 0x13a:
			play_enemy_sound_noint(e, 0x27, "teenb", 0);
			if(group_near_camera(&e->groups[10], 20.0f))
			{
				hurt_player(e->target, 0x1e, 3, 0.0f);
			}
			break;
		}
		if(e->frame < 0x144)
		{
			break;
		}
		teen_steer(e);
		break;
	case TEEN_THROW_FOOD:
		e->frame++;
		if(e->frame == 0x16e && (e->groups[10].flags & 0x2000))
		{
			throw_at_camera(e->groups[10].pos, WEAPON_FOOD, e->scale);
			detach_effect(e, &e->groups[10]);
		}
		if(e->frame < 0x186)
		{
			break;
		}
		teen_next_action(e);
		break;
	case TEEN_THROW:
		e->frame++;
		if(e->frame == 0x191 && (e->groups[10].flags & 0x2000))
		{
			throw_at_camera(e->groups[10].pos, e->unkac, e->scale);
			detach_effect(e, &e->groups[10]);
		}
		if(e->frame < 0x19d)
		{
			break;
		}
		remove_die((unsigned short)e->id, 0);
		break;
	case TEEN_THROW_LEFT:
		e->frame++;
		if(e->frame == 0x226 && (e->groups[4].flags & 0x2000))
		{
			throw_at_camera(e->groups[4].pos, e->unkac, e->scale);
			detach_effect(e, &e->groups[4]);
		}
		if(e->frame < 0x22c)
		{
			break;
		}
		remove_die((unsigned short)e->id, 0);
		break;
	case TEEN_FLEE:
		e->frame++;
		if(e->frame < 0x1c4)
		{
			break;
		}
		remove_die((unsigned short)e->id, 0);
		break;
	case TEEN_ACTION3:
		e->frame++;
		if(e->frame < 0x81)
		{
			break;
		}
		teen_steer(e);
		break;
	case 0x8204:
		e->frame++;
		if(e->frame < 0x248)
		{
			break;
		}
		teen_steer(e);
		break;
	case TEEN_ACTION4:
		e->frame++;
		if(e->frame == 9)
		{
			replace_group(e, &e->groups[13], &e->groups[17]);
		}
		if(e->frame == 0x28)
		{
			replace_group(e, &e->groups[17], &e->groups[13]);
		}
		if(e->frame < 0x39)
		{
			break;
		}
		teen_next_action(e);
		break;
	case TEEN_ACTION5:
		e->frame++;
		if(e->frame < 0x4f)
		{
			break;
		}
		teen_next_action(e);
		break;
	case TEEN_ACTION7:
		e->frame++;
		if(e->frame < 0xa4)
		{
			break;
		}
		teen_next_action(e);
		break;
	case TEEN_ACTION6:
		e->frame++;
		if(e->frame < 0x72)
		{
			break;
		}
		teen_next_action(e);
		break;
	case TEEN_DANCE:
		e->frame++;
		if(e->frame >= 0x81)
		{
			e->frame = 0x72;
		}
		break;
	case TEEN_DIE1:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x119)
		{
			break;
		}
		e->frame = frame;
		e->ai_state = TEEN_GONE;
		if(e->flags2 & 0x20)
		{
			remove_die(obj->id, e->killer);
		}
		break;
	case TEEN_DIE2:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x25c)
		{
			break;
		}
		e->frame = frame;
		e->ai_state = TEEN_GONE;
		if(e->flags2 & 0x20)
		{
			remove_die(obj->id, e->killer);
		}
		break;
	case TEEN_WAIT:
		e->frame++;
		if(e->frame >= 0x1dd)
		{
			e->frame = 0x1c4;
		}
		break;
	case TEEN_MELT:
		e->frame++;
		if(e->frame < 0x1ec)
		{
			break;
		}
		e->frame = 0x1ec;
		e->ai_state = TEEN_SINK;
		for(i = 0; i < 0x15; i++)
		{
			e->groups[i].flags |= 0x20;
		}
		e->y -= 7.0f;
		e->counter = 0;
		break;
	case TEEN_SINK:
		e->frame++;
		e->y -= 0.05f;
		if(e->frame == 0x1ed && e->counter == 0)
		{
			for(i = 0; i < 0x15; i++)
			{
				add_smoke(e->groups[i].pivot, 0xffd000);
			}
			e->counter = 1;
		}
		if(e->frame >= 0x1f9)
		{
			e->frame = 0x1ec;
		}
		break;
	case TEEN_IDLE:
		e->frame++;
		if(e->frame >= 0x25c)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x8012cce8: the state set-up: the spawn entry's flag bits (0x100 act from a chance table,
   or always action 7 without adjustment 0x3d; 0x80, 0x40, 0x20) move to flags2, then each
   state starts at its stretch of the animation; the throwers take a food or present in hand */
void teen_setup(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	if(state & 0x100)
	{
		e->ai_state = state & 0xfeff;
		e->flags2 |= 0x100;
		if(!D_801e704c)
		{
			e->ai_state = TEEN_ACTION7;
		}
	}
	if(e->ai_state & 0x80)
	{
		e->ai_state &= 0xff7f;
		e->flags2 |= 0x80;
	}
	if(e->ai_state & 0x40)
	{
		e->ai_state &= 0xffbf;
		e->flags2 |= 0x40;
	}
	if(e->ai_state & 0x20)
	{
		e->ai_state &= 0xffdf;
		e->flags2 |= 0x20;
	}
	switch(e->ai_state)
	{
	case TEEN_FLEE:
		e->frame = 0x19d;
		break;
	case TEEN_THROW:
		e->frame = 0x186;
		e->unkac = randrng(4) + WEAPON_FOOD;
		attach_effect(e, attach_held_object(&e->groups[10], e->unkac, 0.19f, 0.06f, 0.12f, 1.4f, 1.1f, 0.0f, e->scale));
		break;
	case TEEN_THROW_LEFT:
		e->frame = 0x215;
		e->unkac = randrng(4) + WEAPON_FOOD;
		attach_effect(e, attach_held_object(&e->groups[4], e->unkac, 0.19f, 0.06f, 0.12f, 1.4f, 1.1f, 0.0f, e->scale));
		break;
	case TEEN_THROW_FOOD:
		e->frame = 0x162;
		attach_effect(e, attach_held_object(&e->groups[10], WEAPON_FOOD, 0.19f, 0.06f, 0.12f, 1.4f, 1.1f, 0.0f, e->scale));
		break;
	case TEEN_STRIKE:
		e->frame = 0x119;
		break;
	case TEEN_WALK:
		e->frame = 0x1f9;
		break;
	case TEEN_ACTION6:
		e->frame = 0x4f;
		break;
	case TEEN_ACTION5:
		e->frame = 0x39;
		break;
	case TEEN_ACTION7:
		e->frame = 0x81;
		break;
	case 0x8204:
		e->frame = 0x22c;
		break;
	case TEEN_WAIT:
		e->frame = 0x1c4;
		waiting_teen = e;
		break;
	case TEEN_IDLE:
	case TEEN_ACTION4:
		e->frame = 0;
		break;
	default:
		e->frame = 0;
		break;
	}
}

/* 0x8012cff4: keep a playing target, walk at a point 18 short of the camera and pick the
   state from the result (1 at the point: action 3, 2 in reach: strike, 0 walk) */
void teen_steer(enemy_t *e)
{
	player_stats_t *p;
	int	r;
	short	frame;

	if(e->target->state != 4)
	{
		p = get_target_player();
		if(p->state == 4)
		{
			e->target = p;
		}
	}
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0, -18.0f);
	r = step_toward_goal_xz(e, 0.9f);
	switch(r)
	{
	case 1:
		if(e->ai_state == TEEN_ACTION3)
		{
			e->frame = 0x72;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_teen, 0xd, 0x72, 5);
		e->ai_state = TEEN_ACTION3;
		break;
	case 2:
		if(e->ai_state & 0x200)
		{
			e->frame = 0x119;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_teen, 0xd, 0x119, 5);
		e->ai_state = TEEN_STRIKE;
		break;
	case 0:
		if(e->ai_state == TEEN_WALK)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_teen, 0xd, 0x1f9, 5);
		e->ai_state = TEEN_WALK;
		break;
	}
}

/* 0x8012d1ac: an action ended: a teen flagged 0x100 picks another (or action 7 without
   adjustment 0x3d), a spawn entry marked 2 dances, the rest go for the player; with a line
   from its voice */
void teen_next_action(enemy_t *e)
{
	if(e->flags2 & 0x100)
	{
		if(D_801e704c)
		{
			pick_random_action(e, teen_actions, submesh_info_teen, 0xd, 5);
		}
		else
		{
			e->frame--;
			blend_to_frame(e, submesh_info_teen, 0xd, 0x81, 5);
			e->ai_state = TEEN_ACTION7;
		}
		if(e->unkb0 == 1)
		{
			play_enemy_sound(e, teena_action_sounds[randrng(3)], "teena", 0);
		}
		else
		{
			play_enemy_sound(e, teenb_action_sounds[randrng(5)], "teenb", 0);
		}
	}
	else if((unsigned short)e->unk6 == 2)
	{
		e->unk6 = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_teen, 0xd, 0x72, 2);
		e->ai_state = TEEN_DANCE;
	}
	else
	{
		e->unk6 = 0;
		teen_steer(e);
		if(e->unkb0 == 1)
		{
			play_enemy_sound(e, teena_walk_sounds[randrng(7)], "teena", 0);
		}
		else
		{
			play_enemy_sound(e, teenb_walk_sounds[randrng(4)], "teenb", 0);
		}
	}
}

/* 0x8012d34c: knock the right arm off (groups 10, 9 and 14, from the elbow) */
void teen_drop_right_arm(enemy_t *e)
{
	detach_group(&e->groups[10], e->groups[9].pos, e->y, e->scale);
	detach_group(&e->groups[9], e->groups[9].pos, e->y, e->scale);
	detach_group(&e->groups[14], e->groups[9].pos, e->y, e->scale);
}

/* 0x8012d3c4: knock the left arm off (groups 4, 5 and 3, from the elbow) */
void teen_drop_left_arm(enemy_t *e)
{
	detach_group(&e->groups[4], e->groups[5].pos, e->y, e->scale);
	detach_group(&e->groups[5], e->groups[5].pos, e->y, e->scale);
	detach_group(&e->groups[3], e->groups[5].pos, e->y, e->scale);
}

/* 0x8012d43c: a level-script callback: the waiting teen melts (state 0xa) with a cue */
void func_8012d43c(void)
{
	waiting_teen->ai_state = TEEN_MELT;
	waiting_teen->frame = 0x1dd;
	play_rotating_cue_flags(0x18, "rickity", 1);
	play_enemy_sound(waiting_teen, 0x17, "teenb", 0xc);
}
