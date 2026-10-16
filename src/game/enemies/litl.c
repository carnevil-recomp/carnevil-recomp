/*
 * litl.c - the Big Top's LITL (object type 0x2065, the level's first spawn; LITL.ZM / litl.za /
 * litl.pth and the litl*.WMS texmaps: a face, a hat, ears, a frilled collar, arms and hands, and
 * a handled tool, with gory variants; its calls are in the sound bank "ozob"). One original
 * file, 0x80136840-0x8013831c (the clown's file ends before it with that file's two arm-drop
 * helpers; TOKE's file starts at 0x80138320), rodata 0x801747b8-0x8017484f, tables
 * 0x8019ff48-0x801a06a4 (litl_data.c). The model has 469 vertices, 528 normals and 810 faces in
 * 16 submeshes, the animation 346 frames.
 *
 * States: 0 standing (the whole animation, looped); 1 walking at the player (frames 0x11-0x1c);
 * 2 standing in place; 3 stopped in reach (then it goes for the player again); 4 a two-handed
 * attack (0xf2-0x105) that hurts on frames 0xf8 and 0xfe; 6, 0x802 and 0x803 idling for 21, 21
 * and 41 frames, 7 idling until spawn slots 0x4b-0x4d are done; 0x201-0x203 three swings
 * (0xcf-0xe2) that hurt on frame 0xd9; 0x204 and 0x206 looping attacks, 0x205 a single one;
 * 0x207 throwing its held object at the camera (frame 0x14f); 0x801 and 0x8805 walking its path,
 * 0x8804 walking it to the player and hurting at its end; 0x1001-0x1006 hit reactions;
 * 0x2001-0x2003 deaths (then 0x4000). The set-up's state bits: 0x100 and 0x80 pick the death,
 * 0x40 leaves it at 0.1 health, 0x20 gives it an object to hold and throw (kind 0x30 or 0x31).
 *
 * The exported names are the address names the shared files (spawn.c) use: func_80136840 is the
 * draw callback, func_80136fdc the init, func_801371cc the think, func_80137ae8 the state set-up.
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
extern enemy_t	*debug_enemy;

extern texmap_t	litl_texmap[35];		/* litl_data.c */
extern submesh_t submesh_info_litl[16];
extern int	litl_sounds[20], litl_hurt_sounds[20], litl_die_sounds[5];

extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);	/* text at a world position */
extern void	func_800c8938(float *, float, unsigned int);			/* a debug sphere */
extern void	func_800d14a4(float, float);					/* a debug map dot */
extern void	func_800c99e4(enemy_t *, int);
extern void	draw_effects(enemy_t *);			/* run the attached effects */
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	attach_effect(enemy_t *, void *);
extern void	detach_effect(enemy_t *, group_t *);
extern void	*attach_flame(float, float, group_t *);
extern void	*attach_held_object(group_t *, int, float, float, float, float, float, float, float *);
extern void	throw_at_camera(float *, int, float *);
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern player_stats_t *attack_target(enemy_t *);
extern int	randrng(int);
extern void	path_follow_turned(enemy_t *, float *, int, float);	/* enemy.c */
extern void	animate_loose_groups(enemy_t *, int);
extern int	face_camera(enemy_t *, float);
extern int	group_near_camera(group_t *, float);		/* a group within reach of the player */
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal_xz(enemy_t *, float);
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);	/* hit reaction */
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern void	detach_group(group_t *, float *, float, float *);	/* a part flies off */
extern int	wave_slot_done(int);
extern int	mark_die();							/* spawn.c: (id, killer) */
extern int	remove_die();
extern void	init_groups(enemy_t *, submesh_t *, int);
extern unsigned int next_cue_priority(void);
extern void	load_texmap(texmap_t *, int, char *), remove_surface(texmap_t *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);

float	*litl_verts;			/* 0x801e5b7c */
face_t	*litl_faces;			/* 0x8021f0b8 */
float	*litl_normals;			/* 0x801e5834 */
float	*litl_path;			/* 0x802373d0 */

#define LITL_GROUPS	0x10
#define LITL_NVERTS	0x1d5
#define LITL_NFACES	0x32a
#define LITL_NNORMALS	0x210
#define LITL_FRAMES	0x15a
#define LITL_PATH_LEN	0x8f
#define LITL_REACH	20.0f		/* group 13 this near the player lands a blow */
#define LITL_BLOW	0xd2

int	litl_hit(game_object_t *, player_stats_t *);
int	litl_die(game_object_t *, player_stats_t *);
void	litl_remove(game_object_t *);
void	func_80137ae8(enemy_t *, unsigned short);
void	litl_go_for_player(enemy_t *);
void	litl_next_move(enemy_t *);

/* 0x80136840: the draw callback (type 0x2065) */
void func_80136840(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_litl, LITL_GROUPS, litl_verts, LITL_NVERTS, litl_faces, LITL_NFACES, litl_normals, LITL_NNORMALS);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[15].pos, e->radius, 0x600000);
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

/* 0x80136a3c: the hit test: the bounding sphere, then the submesh */
int litl_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_litl, LITL_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
			p->hit_group = group;
			p->die = (void *)litl_die;
		}
	}
	return(0);
}

/* 0x80136af8: the shot lands: blood; the flame thrower sets the part alight; at the highest
   violence the head goes gory (a shot on group 0: groups 0 and 15, else groups 15 and 7, the
   second gory set); damage, 50 points and a call, or 500 points, a death call and a death by the
   set-up's bits; else a hit reaction by the part */
int litl_die(game_object_t *obj, player_stats_t *p)
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
	if(p->shot_weapon == 0x1000004)
	{
		if(!(g->flags & 2))
		{
			g->flags |= 2;
			attach_effect(e, attach_flame(0.0f, (submesh_info_litl[group].radius + submesh_info_litl[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		if(group == 0)
		{
			e->groups[0].flags |= 0x10000000;
			e->groups[15].flags |= 0x10000000;
		}
		else
		{
			e->groups[15].flags |= 0x20000000;
			e->groups[7].flags |= 0x20000000;
		}
	}
	if(!(e->ai_state & 0x6000))
	{
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			award_points(p->index, 500);
			play_enemy_sound(e, litl_die_sounds[randrng(5)], "ozob", 8);
			e->killer = p;
			mark_die(obj->id, p);
			randrng(100);
			if(e->flags2 & 0x100)
			{
				e->ai_state = 0x2001;
				e->frame = 0x106;
				e->flags &= 0xfffd;
			}
			else if(e->flags2 & 0x80)
			{
				e->ai_state = 0x2003;
				e->frame = 0x12f;
				e->flags &= 0xfffd;
			}
			else
			{
				e->ai_state = 0x2002;
				e->frame = 0x11f;
				e->flags &= 0xfffd;
			}
		}
		else
		{
			award_points(p->index, 50);
			play_enemy_sound(e, litl_hurt_sounds[randrng(20)], "ozob", 2);
		}
	}
	if(!(e->ai_state & 0xe000))
	{
		switch(group)
		{
		case 8:
		case 10:
		case 11:
			start_hit_reaction(e, 0xb3, 0x1005, submesh_info_litl, 7);
			break;
		case 1:
		case 3:
		case 4:
			start_hit_reaction(e, 0xb3, 0x1006, submesh_info_litl, 7);
			break;
		case 0:
			if(randrng(100) < 50)
			{
				start_hit_reaction(e, 0xb3, 0x1003, submesh_info_litl, 7);
			}
			else
			{
				start_hit_reaction(e, 0xb3, 0x1004, submesh_info_litl, 7);
			}
			break;
		case 7:
		case 15:
			if(randrng(100) < 50)
			{
				start_hit_reaction(e, 0xb3, 0x1001, submesh_info_litl, 7);
			}
			else
			{
				start_hit_reaction(e, 0xb3, 0x1002, submesh_info_litl, 7);
			}
			break;
		}
	}
	return(0);
}

/* 0x80136e6c */
void load_litl_texmap(void)
{
	load_texmap(litl_texmap, 0x23, "litl");
}

/* 0x80136e9c: the model, the animation and the path */
void load_litl(void)
{
	load_model((void **)&litl_verts, LITL_NVERTS, (void **)&litl_faces, LITL_NFACES, (void **)&litl_normals, LITL_NNORMALS, "LITL.ZM", 0x33f1a98);
	load_anim(submesh_info_litl, LITL_GROUPS, LITL_FRAMES, "litl.za", 0x9f4ba339);
	load_path((void **)&litl_path, LITL_PATH_LEN, "litl.pth", 0x6e8242fe);
}

/* 0x80136f44 */
void remove_litl_assets(void)
{
	remove_surface(litl_texmap, 0x23);
	remove_path((void **)&litl_path, LITL_PATH_LEN, "litl.pth");
	remove_animation(submesh_info_litl, LITL_GROUPS, LITL_FRAMES, "litl.za");
	remove_model((void **)&litl_verts, LITL_NVERTS, (void **)&litl_faces, LITL_NFACES, (void **)&litl_normals, LITL_NNORMALS, "LITL.ZM");
}

/* 0x80136fdc: the init callback: 3.5 health, the hit part group 7 (drawn with its alpha) */
int func_80136fdc(game_object_t *obj)
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
	obj->hit = litl_hit;
	obj->die = litl_die;
	obj->remove = litl_remove;
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
	e->x = -16.0f;
	e->unkcc = 0.74f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 3.5f);
	e->groups = malloc(LITL_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < LITL_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[7].flags |= 0x40;
	e->unka8 = 0;
	init_groups(e, submesh_info_litl, LITL_GROUPS);
	g = e->groups;
	e->hit_group = &g[7];
	g[7].floor = 0;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	func_80137ae8(e, 0);
	return(0);
}

/* 0x80137174: the remove callback */
void litl_remove(game_object_t *obj)
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

/* 0x801371cc: the think callback */
void func_801371cc(game_object_t *obj)
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
	animate_loose_groups(e, LITL_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1001:
		if(++e->frame >= 0xcf)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_litl, 7, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1002:
		if(++e->frame >= 0xcf)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_litl, 7, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1003:
		if(++e->frame >= 0xcf)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_litl, 7, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1004:
		if(++e->frame >= 0xcf)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_litl, 7, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1005:
		if(++e->frame >= 0xcf)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_litl, 7, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1006:
		if(++e->frame >= 0xcf)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_litl, 7, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x801:
	case 0x8805:
		path_follow_turned(e, litl_path, e->path, 1.0f);
		if(e->path > e->unk92)
		{
			litl_next_move(e);
		}
		if(++e->frame >= 0x1d)
		{
			e->frame = 0x11;
		}
		break;
	case 0x8804:
		path_follow_turned(e, litl_path, e->path, 1.0f);
		if(e->path >= e->unk92)
		{
			if(group_near_camera(&e->groups[13], LITL_REACH))
			{
				hurt_player(e->target, LITL_BLOW, 3, 0.0f);
			}
			remove_die(obj->id, 0);
		}
		if(++e->frame >= 0x99)
		{
			e->frame = 0x86;
		}
		break;
	case 1:
		if(++e->frame >= 0x1d)
		{
			e->frame = 0x11;
		}
		face_camera(e, 0.2f);
		litl_go_for_player(e);
		break;
	case 0x201:
		face_camera(e, 0.2f);
		if(++e->frame == 0xd9 && group_near_camera(&e->groups[13], LITL_REACH))
		{
			hurt_player(e->target, LITL_BLOW, 10, 0.0f);
		}
		if(e->frame >= 0xe3)
		{
			litl_go_for_player(e);
		}
		break;
	case 0x202:
		face_camera(e, 0.2f);
		if(++e->frame == 0xd9 && group_near_camera(&e->groups[13], LITL_REACH))
		{
			hurt_player(e->target, LITL_BLOW, 10, 0.0f);
		}
		if(e->frame >= 0xe3)
		{
			litl_go_for_player(e);
		}
		break;
	case 0x203:
		face_camera(e, 0.2f);
		if(++e->frame == 0xd9 && group_near_camera(&e->groups[13], LITL_REACH))
		{
			hurt_player(e->target, LITL_BLOW, 10, 0.0f);
		}
		if(e->frame >= 0xe3)
		{
			litl_go_for_player(e);
		}
		break;
	case 0x204:
		if(++e->frame == 0x24 && group_near_camera(&e->groups[13], LITL_REACH))
		{
			hurt_player(e->target, LITL_BLOW, 10, 0.0f);
		}
		if(e->frame == 0x31 && group_near_camera(&e->groups[13], LITL_REACH))
		{
			hurt_player(e->target, LITL_BLOW, 10, 0.0f);
		}
		if(e->frame >= 0x45)
		{
			e->frame = 0x1d;
		}
		break;
	case 0x205:
		if(++e->frame == 0xa8 && group_near_camera(&e->groups[13], LITL_REACH))
		{
			hurt_player(attack_target(e), LITL_BLOW, 10, 0.0f);
		}
		if(e->frame >= 0xb3)
		{
			litl_next_move(e);
		}
		break;
	case 0x206:
		if(++e->frame == 0x7c && group_near_camera(&e->groups[13], LITL_REACH))
		{
			hurt_player(e->target, LITL_BLOW, 10, 0.0f);
		}
		if(e->frame >= 0x86)
		{
			e->frame = 0x64;
		}
		break;
	case 0x207:
		face_camera(e, 0.2f);
		if(++e->frame == 0x14f && (e->groups[6].flags & 0x2000))
		{
			throw_at_camera(e->groups[6].pos, e->unkac, e->scale);
			detach_effect(e, &e->groups[6]);
		}
		if(e->frame >= 0x15a)
		{
			litl_next_move(e);
		}
		break;
	case 3:
		if(++e->frame >= 0x11)
		{
			litl_go_for_player(e);
		}
		break;
	case 0x802:
		if(++e->frame >= 0x11)
		{
			e->frame = 0;
		}
		if(++e->counter >= 0x15)
		{
			litl_next_move(e);
		}
		break;
	case 0x803:
		if(++e->frame >= 0x11)
		{
			e->frame = 0;
		}
		if(++e->counter >= 0x29)
		{
			litl_next_move(e);
		}
		break;
	case 6:
		if(++e->frame >= 0x11)
		{
			e->frame = 0;
		}
		if(++e->counter >= 0x15)
		{
			litl_next_move(e);
		}
		break;
	case 7:
		if(++e->frame >= 0x11)
		{
			e->frame = 0;
		}
		if(wave_slot_done(0x4b) && wave_slot_done(0x4c) && wave_slot_done(0x4d))
		{
			litl_next_move(e);
		}
		break;
	case 4:
		if(++e->frame == 0xf8 && group_near_camera(&e->groups[13], LITL_REACH))
		{
			hurt_player(attack_target(e), LITL_BLOW, 7, 3.1415927f);
		}
		if(e->frame == 0xfe && group_near_camera(&e->groups[13], LITL_REACH))
		{
			hurt_player(attack_target(e), LITL_BLOW, 7, 0.0f);
		}
		if(e->frame >= 0x106)
		{
			e->frame = 0xf2;
		}
		break;
	case 2:
		if(++e->frame >= 0x11)
		{
			e->frame = 0;
		}
		break;
	case 0x2001:
		if(++e->frame >= 0x11f)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		break;
	case 0x2002:
		if(++e->frame >= 0x12f)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		break;
	case 0x2003:
		if(++e->frame >= 0x143)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		break;
	case 0:
		if(++e->frame >= LITL_FRAMES)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x80137ae8: the state set-up: the spawn entry's bits 0x100 and 0x80 (the death it takes) and
   0x40 (0.1 health) go into flags2; 0x20 gives it an object to hold on group 6 (kind 0x30 or
   0x31) that state 0x207 throws; then each state's first frame */
void func_80137ae8(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	if(state & 0x100)
	{
		e->ai_state = state & 0xfeff;
		e->flags2 |= 0x100;
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
		e->health = 0.1f;
	}
	if(e->ai_state & 0x20)
	{
		e->ai_state &= 0xffdf;
		e->flags2 |= 0x20;
		if(randrng(100) < 50)
		{
			e->unkac = 0x30;
		}
		else
		{
			e->unkac = 0x31;
		}
		attach_effect(e, attach_held_object(&e->groups[6], e->unkac, 0.14f, 0.03f, 0.0f, -1.7f, -3.0f, 0.0f, e->scale));
	}
	switch(e->ai_state)
	{
	case 0x201:
		e->frame = 0xcf;
		break;
	case 1:
		e->frame = 0x11;
		break;
	case 4:
		e->frame = 0xe3;
		break;
	case 0x801:
		e->frame = 0x11;
		path_follow_turned(e, litl_path, e->path, 1.0f);
		break;
	case 0x8804:
		e->frame = 0x86;
		path_follow_turned(e, litl_path, e->path, 1.0f);
		break;
	case 0x8805:
		e->frame = 0x45;
		path_follow_turned(e, litl_path, e->path, 1.0f);
		break;
	case 6:
		e->frame = 0;
		e->counter = 0;
		break;
	case 7:
		e->frame = 0;
		e->counter = 0;
		break;
	case 0x207:
		e->frame = 0x143;
		break;
	case 0x802:
		e->frame = 0;
		e->counter = 0;
		path_follow_turned(e, litl_path, e->path, 1.0f);
		break;
	case 0x803:
		e->frame = 0;
		e->counter = 0;
		path_follow_turned(e, litl_path, e->path, 1.0f);
		break;
	case 0x204:
		e->frame = 0x1d;
		break;
	case 0x205:
		e->frame = 0x99;
		break;
	case 0x206:
		e->frame = 0x64;
		break;
	case 0:
	default:
		e->frame = 0;
		break;
	}
}

/* 0x80137dcc: going for the player: a playing player to aim at, a goal by the camera; walking
   (1) on the way, stopped (3) when blocked, one of three swings (0x201-0x203) in reach */
void litl_go_for_player(enemy_t *e)
{
	player_stats_t	*p;
	int		step, r;

	if(e->target->state != 4)
	{
		p = get_target_player();
		if(p->state == 4)
		{
			e->target = p;
		}
	}
	set_goal_near_camera(e, camera_pos, e->goal, 2.0f, 0.0f, -14.0f);
	step = step_toward_goal_xz(e, 0.9f);
	switch(step)
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
		blend_to_frame(e, submesh_info_litl, 7, 0, 2);
		e->ai_state = 3;
		break;
	case 2:
		play_enemy_sound(e, litl_sounds[randrng(20)], "ozob", 0);
		if(e->frame > 0)
		{
			e->frame--;
		}
		r = randrng(100);
		if(r < 35)
		{
			blend_to_frame(e, submesh_info_litl, 7, 0xcf, step);
			e->ai_state = 0x201;
		}
		else if(r < 65)
		{
			blend_to_frame(e, submesh_info_litl, 7, 0xcf, step);
			e->ai_state = 0x202;
		}
		else
		{
			blend_to_frame(e, submesh_info_litl, 7, 0xcf, step);
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
		blend_to_frame(e, submesh_info_litl, 7, 0x11, 2);
		e->ai_state = 1;
		break;
	}
}

/* 0x80137ff0: the next move after a run, an idle or a throw, by the spawn entry's next state
   (unk6): throw (0x207, 0x208 then 2), idle (2), walk the path (0x801, 0x806 then go), walk it
   to the player (0x8804), leave (5); otherwise go for the player */
void litl_next_move(enemy_t *e)
{
	play_enemy_sound(e, litl_sounds[randrng(20)], "ozob", 0);
	switch((unsigned short)e->unk6)
	{
	case 0x801:
		if(e->frame > 0)
		{
			e->frame--;
		}
		e->unk6 = 0;
		blend_to_frame(e, submesh_info_litl, 7, 0x11, 2);
		e->ai_state = 0x801;
		break;
	case 0x207:
		if(e->frame > 0)
		{
			e->frame--;
		}
		e->unk6 = 0;
		blend_to_frame(e, submesh_info_litl, 7, 0x143, 2);
		e->ai_state = 0x207;
		break;
	case 0x208:
		if(e->frame > 0)
		{
			e->frame--;
		}
		e->unk6 = 2;
		blend_to_frame(e, submesh_info_litl, 7, 0x143, 2);
		e->ai_state = 0x207;
		break;
	case 0x806:
		if(e->frame > 0)
		{
			e->frame--;
		}
		e->unk6 = 5;
		blend_to_frame(e, submesh_info_litl, 7, 0x11, 2);
		e->ai_state = 0x801;
		break;
	case 5:
		remove_die((unsigned short)e->id, 0);
		break;
	case 0x8804:
		if(e->frame > 0)
		{
			e->frame--;
		}
		e->unk6 = 0;
		blend_to_frame(e, submesh_info_litl, 7, 0x86, 1);
		e->ai_state = 0x8804;
		break;
	case 2:
		if(e->frame > 0)
		{
			e->frame--;
		}
		e->unk6 = 0;
		blend_to_frame(e, submesh_info_litl, 7, 0, 2);
		e->ai_state = 2;
		break;
	default:
		e->unk6 = 0;
		litl_go_for_player(e);
		break;
	}
}

/* 0x8013825c, 0x801382bc: groups 6 and 5 knocked loose from group 5, and groups 13 and 12 from
   group 12 (no caller in the image) */
void litl_drop_right_arm(enemy_t *e)
{
	detach_group(&e->groups[6], e->groups[5].pos, e->y, e->scale);
	detach_group(&e->groups[5], e->groups[5].pos, e->y, e->scale);
}

void litl_drop_left_arm(enemy_t *e)
{
	detach_group(&e->groups[13], e->groups[12].pos, e->y, e->scale);
	detach_group(&e->groups[12], e->groups[12].pos, e->y, e->scale);
}
