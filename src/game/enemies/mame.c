/*
 * mame.c - the Big Top's MAME (object type 0x2062; assets MAME.ZM / mame.za and the mame*.WMS
 * texmaps: hat, flower, head, neck, chest, pelvis, arms, legs, feet and "mamed_" damage maps; it
 * has no path and makes no sound of its own). One original file, 0x80131f10-0x8013357c, its
 * tables at 0x8019e720 (texmaps), 0x8019ebc0 (submeshes) and 0x8019ee40 (sprays), mame_data.c.
 * The model has 589 vertices, 1013 faces and 651 normals in 20 submeshes, the animation 1187
 * frames: one long take holding every loop the states play. The word at 0x8013357c is not code.
 *
 * The spawn entry picks one of its standing loops (2, 4, 6-0xb, plus 0x105 and 0x8c: 5 and 0xc
 * with the bits 0x100 and 0x80, which make any shot kill it and pick a special death); state 0
 * plays the whole take. Hunting (1, 3, from the end of 0xb back to 0xa) it steers at a point 28
 * units in front of the camera and swings (0x201-0x203: 10 damage on frame 12 when part 2 is
 * within 20 units). 7.5 health; a kill is 500 points and one of four deaths (0x2001, 0x2002 once
 * the head has gone, 0x2103 and 0x2084 for the 0x100 / 0x80 spawns), a hit 50 and a flinch by
 * the part hit (0x1001-0x1006). At the highest violence parts turn gory, the arms (7, 11) are
 * swapped for spares (17, 18) as their hands fly off, and the head (14-16) goes in two shots:
 * marked, then part 15 swapped for 19 with a spray, the health dropping to 0.1. The head code
 * still prints the zombie's "Zomb head damage at 0" message.
 */
#include <stdio.h>
#include <stdlib.h>
#include <glide/glide.h>
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "texmap.h"
#include "camera.h"			/* camera_x */
#include "credits.h"			/* debug_mode, player_stats_t */
#include "levelsnd.h"

extern void	draw_effects(enemy_t *);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern int	randrng(int);
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern int	face_camera(enemy_t *, float);
extern int	group_near_camera(group_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal_xz(enemy_t *, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern int	mark_die();
extern void	attach_effect(enemy_t *, void *);
extern void	*attach_flame(float, float, group_t *);
extern void	*attach_spray(float *, float *, group_t *);
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);
extern void	replace_group(enemy_t *, group_t *, group_t *);
extern void	detach_group(group_t *, float *, float, float *);
extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *);
extern char	text_buffer[];
extern enemy_t	*debug_enemy;
extern int	violence_level;

extern texmap_t	mame_texmap[37];		/* mame_data.c */
extern submesh_t submesh_info_mame[20];
extern float	mame_spray1a[3], mame_spray1b[3];

float	*mame_verts;			/* 0x801e5838 */
face_t	*mame_faces;			/* 0x80212de8 */
float	*mame_normals;			/* 0x801e0f50 */

#define MAME_GROUPS	20
#define MAME_TEXMAPS	37
#define MAME_NVERTS	0x24d
#define MAME_NFACES	0x3f5
#define MAME_NNORMALS	0x28b
#define MAME_FRAMES	0x4a3
#define MAME_STRIKE	2		/* the part its swing reaches with */
#define MAME_HIT_GROUP	6		/* the submesh it is measured by */
#define MAME_SWING	10		/* the damage of a swing */

int	mame_die(game_object_t *, player_stats_t *);
void	mame_remove(game_object_t *);
void	func_80133118(enemy_t *, unsigned short);
void	mame_approach(enemy_t *);
void	mame_drop_left_hand(enemy_t *);
void	mame_drop_right_hand(enemy_t *);

/* 0x80131f10: the draw callback */
void func_80131f10(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_mame, MAME_GROUPS, mame_verts, MAME_NVERTS, mame_faces, MAME_NFACES, mame_normals, MAME_NNORMALS);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[10].pos, e->radius, 0x600000);
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

/* 0x8013210c: the hit test: the bounding sphere, then the submeshes */
int mame_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_mame, MAME_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)mame_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x801321c8: the shot lands: blood, chunks, the flame thrower's fire; the gore of the highest
   violence; the damage and points; a flinch by the part hit while it lives */
int mame_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;
	int	group;

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
	if(p->shot_weapon == 0x1000004 && !(g->flags & 2))
	{
		g->flags |= 2;
		attach_effect(e, attach_flame(0.0f, (submesh_info_mame[group].radius + submesh_info_mame[group].radius) * e->scale[0], g));
	}
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 14:
		case 15:
		case 16:
			e->unka8++;
			switch(e->unka8)
			{
			case 0:
				printf("This should never happen! Zomb head damage at 0\n");
				break;
			case 1:
				e->groups[16].flags |= 0xf0000000;
				e->groups[15].flags |= 0xf0000000;
				e->groups[14].flags |= 0xf0000000;
				break;
			case 2:
				e->health = 0.1f;
				replace_group(e, &e->groups[15], &e->groups[19]);
				attach_effect(e, attach_spray(mame_spray1b, mame_spray1a, &e->groups[19]));
				break;
			}
			break;
		case 11:
		case 12:
		case 13:
			replace_group(e, &e->groups[11], &e->groups[18]);
			mame_drop_left_hand(e);
			break;
		case 7:
		case 8:
		case 9:
			replace_group(e, &e->groups[7], &e->groups[17]);
			mame_drop_right_hand(e);
			break;
		case 6:
			e->groups[6].flags |= 0xf0000000;
			break;
		default:
			g->flags |= 0xf0000000;
			break;
		}
	}
	if(!(e->ai_state & 0x6000))
	{
		if(e->flags2 & 0x100)
		{
			e->health = 0.01f;
		}
		if(e->flags2 & 0x80)
		{
			e->health = 0.01f;
		}
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			award_points(p->index, 500);
			e->killer = p;
			mark_die(obj->id, p);
			if(e->flags2 & 0x100)
			{
				e->ai_state = 0x2103;
				e->frame = 0x12b;
				e->flags &= 0xfffd;
			}
			else if(e->flags2 & 0x80)
			{
				e->ai_state = 0x2084;
				e->frame = 0x48a;
				e->flags &= 0xfffd;
			}
			else if(e->unka8 == 2)
			{
				e->ai_state = 0x2002;
				e->frame = 0x14a;
				e->flags &= 0xfffd;
			}
			else
			{
				e->ai_state = 0x2001;
				e->frame = 0x210;
				e->flags &= 0xfffd;
			}
		}
		else
		{
			award_points(p->index, 50);
		}
	}
	if(!(e->ai_state & 0xe000))
	{
		switch(group)
		{
		case 7:
		case 8:
		case 9:
			start_hit_reaction(e, 0x198, 0x1005, submesh_info_mame, 6);
			break;
		case 11:
		case 12:
		case 13:
			start_hit_reaction(e, 0x1d6, 0x1006, submesh_info_mame, 6);
			break;
		case 14:
		case 15:
		case 16:
		case 19:
			if(randrng(100) < 50)
			{
				start_hit_reaction(e, 0x9c, 0x1003, submesh_info_mame, 6);
			}
			else
			{
				start_hit_reaction(e, 0x9c, 0x1004, submesh_info_mame, 6);
			}
			break;
		case 6:
		case 10:
			if(randrng(100) < 50)
			{
				start_hit_reaction(e, 0xb0, 0x1001, submesh_info_mame, 6);
			}
			else
			{
				start_hit_reaction(e, 0x243, 0x1002, submesh_info_mame, 6);
			}
			break;
		default:
			start_hit_reaction(e, 0xb0, 0x1001, submesh_info_mame, 6);
			break;
		}
	}
	return(0);
}

/* 0x80132664 */
void load_mame_texmap(void)
{
	load_texmap(mame_texmap, MAME_TEXMAPS, "mame");
}

/* 0x80132694: the model and animation; the spare head and arms take their animation from parts
   15, 7 and 11 */
void load_mame(void)
{
	load_model((void **)&mame_verts, MAME_NVERTS, (void **)&mame_faces, MAME_NFACES, (void **)&mame_normals, MAME_NNORMALS, "MAME.ZM", 0x1a643497);
	load_anim(submesh_info_mame, MAME_GROUPS, MAME_FRAMES, "mame.za", 0x5eed52a2);
	free(submesh_info_mame[19].anim);
	free(submesh_info_mame[17].anim);
	free(submesh_info_mame[18].anim);
	submesh_info_mame[19].anim = submesh_info_mame[15].anim;
	submesh_info_mame[17].anim = submesh_info_mame[7].anim;
	submesh_info_mame[18].anim = submesh_info_mame[11].anim;
}

/* 0x8013277c: the assets go when the level is unloaded (the shared animations are let go first) */
void remove_mame_assets(void)
{
	submesh_info_mame[19].anim = 0;
	submesh_info_mame[17].anim = 0;
	submesh_info_mame[18].anim = 0;
	remove_surface(mame_texmap, MAME_TEXMAPS);
	remove_animation(submesh_info_mame, MAME_GROUPS, MAME_FRAMES, "mame.za");
	remove_model((void **)&mame_verts, MAME_NVERTS, (void **)&mame_faces, MAME_NFACES, (void **)&mame_normals, MAME_NNORMALS, "MAME.ZM");
}

/* 0x8013281c: the init callback: the record, health 7.5, the 20 groups (the spares hidden) */
int func_8013281c(game_object_t *obj)
{
	enemy_t	*e;
	void	*data;
	group_t	*g;
	int	i;

	data = malloc(sizeof(enemy_t));
	obj->data = data;
	if(!data)
	{
		return(1);
	}
	obj->hit = mame_hit;
	obj->die = mame_die;
	obj->remove = mame_remove;
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
	e->x = -24.0f;
	e->unkcc = 1.3f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 7.5f);
	e->groups = malloc(MAME_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < MAME_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[19].flags = 0;
	e->groups[17].flags = 0;
	e->groups[18].flags = 0;
	e->unka8 = 0;
	init_groups(e, submesh_info_mame, MAME_GROUPS);
	g = e->groups;
	e->hit_group = &g[MAME_HIT_GROUP];
	g[MAME_HIT_GROUP].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	func_80133118(e, 0);
	return(0);
}

/* 0x801329bc: the remove callback */
void mame_remove(game_object_t *obj)
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

#define RESUME(last) \
		if(++e->frame >= (last)) \
		{ \
			e->frame--; \
			blend_to_frame(e, submesh_info_mame, 6, e->resume_frame, 2); \
			e->ai_state = e->resume_state; \
		} \
		break;
#define SWING \
		face_camera(e, 0.2f); \
		if(++e->frame == 0xc && group_near_camera(&e->groups[MAME_STRIKE], 20.0f)) \
		{ \
			hurt_player(e->target, MAME_SWING, 3, 0.0f); \
		} \
		if(e->frame >= 0x62) \
		{ \
			mame_approach(e); \
		} \
		break;
#define LOOP(last, first) \
		if(++e->frame >= (last)) \
		{ \
			e->frame = (first); \
		} \
		break;
#define FALL(last) \
		if(++e->frame >= (last)) \
		{ \
			e->frame--; \
			e->ai_state = 0x4000; \
		} \
		break;

/* 0x80132a14: the think callback: each state plays its stretch of the take */
void func_80132a14(game_object_t *obj)
{
	enemy_t	*e;

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
	animate_loose_groups(e, MAME_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1003:
		RESUME(0xb0)
	case 0x1004:
		RESUME(0xb0)
	case 0x1001:
		RESUME(0xd3)
	case 0x1002:
		RESUME(0x270)
	case 0x1005:
		RESUME(0x1d6)
	case 0x1006:
		RESUME(0x210)
	case 0x8801:
		if(++e->path >= e->unk92)
		{
			e->path--;
		}
		if(++e->frame >= 0x62)
		{
			e->frame = 0;
		}
		break;
	case 1:
		if(++e->frame >= 0x62)
		{
			e->frame = 0;
		}
		face_camera(e, 0.2f);
		mame_approach(e);
		break;
	case 0x201:
		SWING
	case 0x202:
		SWING
	case 0x203:
		SWING
	case 3:
		if(++e->frame >= 0x62)
		{
			mame_approach(e);
		}
		break;
	case 2:
		if(++e->frame >= 0x62)
		{
			e->frame = 0;
		}
		break;
	case 4:
		LOOP(0x108, 0xd3)
	case 8:
		LOOP(0x375, 0x2c9)
	case 9:
		LOOP(0x3f7, 0x375)
	case 0xa:
		LOOP(0x426, 0x3f7)
	case 0xb:
		if(++e->frame >= 0x467)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_mame, 6, 0x3f7, 2);
			e->ai_state = 0xa;
		}
		break;
	case 6:
		LOOP(0x9c, 0x62)
	case 7:
		LOOP(0x2c9, 0x270)
	case 0x105:
		LOOP(0x12b, 0x108)
	case 0x8c:
		LOOP(0x48a, 0x467)
	case 0x2001:
		FALL(0x243)
	case 0x2002:
		FALL(0x198)
	case 0x2103:
		FALL(0x14a)
	case 0x2084:
		FALL(MAME_FRAMES)
	case 0:
		if(++e->frame >= MAME_FRAMES)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x80133118: the spawn table's state; the bits 0x100 and 0x80 are kept in flags2 (they are not
   stripped from the state) and each standing loop starts at its first frame */
void func_80133118(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	if(state & 0x100)
	{
		e->flags2 |= 0x100;
	}
	if(e->ai_state & 0x80)
	{
		e->flags2 |= 0x80;
	}
	switch(e->ai_state)
	{
	case 4:
		e->frame = 0xd3;
		break;
	case 6:
		e->frame = 0x62;
		break;
	case 7:
		e->frame = 0x270;
		break;
	case 8:
		e->frame = 0x2c9;
		break;
	case 9:
		e->frame = 0x375;
		break;
	case 0xa:
		e->frame = 0x3f7;
		break;
	case 0xb:
		e->frame = 0x426;
		break;
	case 0x8c:
		e->frame = 0x467;
		break;
	case 0x105:
		e->frame = 0x108;
		break;
	case 2:
	case 0x201:
	default:
		e->frame = 0;
		break;
	}
}

/* 0x80133260: hunting: steer at a point 28 units in front of the camera; there, stand (3); in
   reach, one of three swings (0x201 35%, 0x202 30%, 0x203 35%); else walk (1) */
void mame_approach(enemy_t *e)
{
	player_stats_t	*p;
	int	r;
	int	n;

	if(e->target->state != 4)
	{
		p = get_target_player();
		if(p->state == 4)
		{
			e->target = p;
		}
	}
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0, -28.0f);
	r = step_toward_goal_xz(e, 0.3f);
	switch(r)
	{
	case 1:
		if(e->ai_state == 3)
		{
			e->frame = 0;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_mame, 6, 0, 2);
		e->ai_state = 3;
		break;
	case 2:
		if(e->frame > 0)
		{
			e->frame--;
		}
		n = randrng(100);
		if(n < 35)
		{
			blend_to_frame(e, submesh_info_mame, 6, 0, 2);
			e->ai_state = 0x201;
		}
		else if(n < 65)
		{
			blend_to_frame(e, submesh_info_mame, 6, 0, 2);
			e->ai_state = 0x202;
		}
		else
		{
			blend_to_frame(e, submesh_info_mame, 6, 0, 2);
			e->ai_state = 0x203;
		}
		break;
	case 0:
		if(e->ai_state == 1)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_mame, 6, 0, 2);
		e->ai_state = 1;
		break;
	}
}

/* 0x80133458: stand (2), clearing the spawn entry's follow-up; nothing calls it */
void mame_stand(enemy_t *e)
{
	if(e->frame > 0)
	{
		e->frame--;
	}
	e->unk6 = 0;
	blend_to_frame(e, submesh_info_mame, 6, 0, 2);
	e->ai_state = 2;
}

/* 0x801334bc: the left hand flies off (parts 13 and 12 from 12's place) */
void mame_drop_left_hand(enemy_t *e)
{
	detach_group(&e->groups[13], e->groups[12].pos, e->y, e->scale);
	detach_group(&e->groups[12], e->groups[12].pos, e->y, e->scale);
}

/* 0x8013351c: the right hand flies off (parts 9 and 8 from 8's place) */
void mame_drop_right_hand(enemy_t *e)
{
	detach_group(&e->groups[9], e->groups[8].pos, e->y, e->scale);
	detach_group(&e->groups[8], e->groups[8].pos, e->y, e->scale);
}
