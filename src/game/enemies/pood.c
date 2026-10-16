/*
 * pood.c - the Big Top's poodle (object type 0x2064; the game calls it POOD: assets POOD.ZM /
 * pood.za / pood.pth, the pood*.WMS texmaps - fur, puffs, nails, teeth, tongue, ears, tail,
 * frill - and the "pood" sound bank). One original file, 0x80130bf8-0x80131f0c, its tables at
 * 0x8019de50 (texmaps), 0x8019e410 (submeshes), 0x8019e690 (sprays) and 0x8019e6d8 (calls),
 * pood_data.c. The model has 859 vertices, 1243 faces and 836 normals in 20 submeshes, the
 * animation 166 frames, the path 198 points. The word at 0x80131f0c is not code.
 *
 * It runs its path (0x8801) and hunts the player: it steers at a point 28 units in front of the
 * camera, walks (1), crouches when it gets there (3, then idles in 2) and, in reach, bites with
 * one of three attacks (0x201-0x203: 235 damage on frame 19 when its head, part 0, is within 20
 * units). State 5 waits 40 frames first; 4 is a loop it holds. 5.5 health; a kill is 500 points
 * (it falls one of two ways, 0x2001 / 0x2002, then 0x4000), a hit that does not kill 50 and,
 * while it is not dying, a flinch (0x1001 or 0x1002). At the highest violence a shot on its head
 * or neck (parts 0, 1 or the spare 19) swaps part 1 for 19, or, once that has happened, part 6
 * for 18, drops it to 0.1 health and sends the head flying with a spray.
 */
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
extern void	path_follow_turned(enemy_t *, void *, int, float);
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
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern char	text_buffer[];
extern enemy_t	*debug_enemy;
extern int	violence_level;

extern texmap_t	pood_texmap[46];		/* pood_data.c */
extern submesh_t submesh_info_pood[20];
extern float	pood_spray1a[3], pood_spray1b[3];
extern int	pood_attack_sounds[12], pood_hurt_sounds[3], pood_die_sounds[3];

float	*pood_verts;			/* 0x801e5b50 */
face_t	*pood_faces;			/* 0x8021f070 */
float	*pood_normals;			/* 0x801e580c */
void	*pood_path;			/* 0x8023737c */

#define POOD_GROUPS	20
#define POOD_TEXMAPS	46
#define POOD_NVERTS	0x35b
#define POOD_NFACES	0x4db
#define POOD_NNORMALS	0x344
#define POOD_FRAMES	0xa6
#define POOD_PATH_LEN	0xc6
#define POOD_HEAD	0		/* the part that bites */
#define POOD_HIT_GROUP	4		/* the submesh it is measured by */
#define POOD_SPARE_HEAD	19		/* hidden; it replaces part 1 */
#define POOD_SPARE_NECK	18		/* hidden; it replaces part 6 */
#define POOD_BITE	235		/* the damage of a bite */

int	pood_die(game_object_t *, player_stats_t *);
void	pood_remove(game_object_t *);
void	func_80131a9c(enemy_t *, unsigned short);
void	pood_approach(enemy_t *);
void	pood_next(enemy_t *);

/* 0x80130bf8: the draw callback */
void func_80130bf8(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_pood, POOD_GROUPS, pood_verts, POOD_NVERTS, pood_faces, POOD_NFACES, pood_normals, POOD_NNORMALS);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[5].pos, e->radius, 0x600000);
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

/* 0x80130df4: the hit test: the bounding sphere, then the spare head, then the submeshes */
int pood_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, &submesh_info_pood[POOD_SPARE_HEAD], 1, p);
		if(group == -1)
		{
			group = find_submesh_hit(e, submesh_info_pood, POOD_GROUPS, p);
		}
		else
		{
			group = POOD_SPARE_HEAD;
		}
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)pood_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x80130edc: the shot lands: blood, chunks, the flame thrower's fire; at the highest violence
   the head and neck parts; then the damage, points and a cry, and a flinch while it lives */
int pood_die(game_object_t *obj, player_stats_t *p)
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
		attach_effect(e, attach_flame(0.0f, (submesh_info_pood[group].radius + submesh_info_pood[group].radius) * e->scale[0], g));
	}
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 0:
		case 1:
		case POOD_SPARE_HEAD:
			g = e->groups;
			if(g[1].flags & 0x2000)
			{
				replace_group(e, &g[1], &g[POOD_SPARE_HEAD]);
			}
			else if(g[POOD_SPARE_HEAD].flags & 0x2000)
			{
				e->health = 0.1f;
				replace_group(e, &e->groups[6], &e->groups[POOD_SPARE_NECK]);
				detach_group(&e->groups[POOD_SPARE_HEAD], e->groups[POOD_SPARE_HEAD].pos, e->y, e->scale);
				detach_group(&e->groups[0], e->groups[POOD_SPARE_HEAD].pos, e->y, e->scale);
				attach_effect(e, attach_spray(pood_spray1b, pood_spray1a, &e->groups[POOD_SPARE_NECK]));
			}
			break;
		}
	}
	if(!(e->ai_state & 0x6000))
	{
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			award_points(p->index, 500);
			play_enemy_sound(e, pood_die_sounds[randrng(3)], "pood", 8);
			e->killer = p;
			mark_die(obj->id, p);
			if(randrng(100) < 50)
			{
				e->ai_state = 0x2001;
				e->frame = 0x2a;
				e->flags &= 0xfffd;
			}
			else
			{
				e->ai_state = 0x2002;
				e->frame = 0x92;
				e->flags &= 0xfffd;
			}
		}
		else
		{
			award_points(p->index, 50);
			play_enemy_sound(e, pood_hurt_sounds[randrng(3)], "pood", 2);
		}
	}
	if(!(e->ai_state & 0xe000))
	{
		if(randrng(100) < 50)
		{
			start_hit_reaction(e, 0x52, 0x1001, submesh_info_pood, 4);
		}
		else
		{
			start_hit_reaction(e, 0x68, 0x1002, submesh_info_pood, 4);
		}
	}
	return(0);
}

/* 0x80131218 */
void load_pood_texmap(void)
{
	load_texmap(pood_texmap, POOD_TEXMAPS, "pood");
}

/* 0x80131248: the model, animation and path; the spare neck and head take their animation
   from parts 6 and 1 */
void load_pood(void)
{
	load_model((void **)&pood_verts, POOD_NVERTS, (void **)&pood_faces, POOD_NFACES, (void **)&pood_normals, POOD_NNORMALS, "POOD.ZM", 0xbf10f977);
	load_anim(submesh_info_pood, POOD_GROUPS, POOD_FRAMES, "pood.za", 0x67f3dd8e);
	load_path(&pood_path, POOD_PATH_LEN, "pood.pth", 0x2c5506e3);
	free(submesh_info_pood[POOD_SPARE_NECK].anim);
	free(submesh_info_pood[POOD_SPARE_HEAD].anim);
	submesh_info_pood[POOD_SPARE_NECK].anim = submesh_info_pood[6].anim;
	submesh_info_pood[POOD_SPARE_HEAD].anim = submesh_info_pood[1].anim;
}

/* 0x80131330: the assets go when the level is unloaded (the shared animations are let go first) */
void func_80131330(void)
{
	submesh_info_pood[POOD_SPARE_NECK].anim = 0;
	submesh_info_pood[POOD_SPARE_HEAD].anim = 0;
	remove_surface(pood_texmap, POOD_TEXMAPS);
	remove_path(&pood_path, POOD_PATH_LEN, "pood.pth");
	remove_animation(submesh_info_pood, POOD_GROUPS, POOD_FRAMES, "pood.za");
	remove_model((void **)&pood_verts, POOD_NVERTS, (void **)&pood_faces, POOD_NFACES, (void **)&pood_normals, POOD_NNORMALS, "POOD.ZM");
}

/* 0x801313e0: the init callback: the record, health 5.5, the 20 groups (part 6 translucent,
   the spares hidden) */
int func_801313e0(game_object_t *obj)
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
	obj->hit = pood_hit;
	obj->die = pood_die;
	obj->remove = pood_remove;
	e = (enemy_t *)data;
	e->unk4 = 0x14;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0xc;
	e->target = get_target_player();
	e->unk54 = 0;
	e->rx = 0;
	e->rz = 0;
	e->x = 0;
	e->y = 0;
	e->z = 0;
	e->ry = 3.1415927f;
	e->unkcc = 1.0f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 5.5f);
	e->groups = malloc(POOD_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < POOD_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[6].flags |= 8;
	e->groups[POOD_SPARE_NECK].flags = 0;
	e->groups[POOD_SPARE_HEAD].flags = 0;
	e->unka8 = 0;
	init_groups(e, submesh_info_pood, POOD_GROUPS);
	g = e->groups;
	e->hit_group = &g[POOD_HIT_GROUP];
	g[POOD_HIT_GROUP].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	func_80131a9c(e, 5);
	return(0);
}

/* 0x8013157c: the remove callback */
void pood_remove(game_object_t *obj)
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

/* 0x801315d4: the think callback */
void func_801315d4(game_object_t *obj)
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
	animate_loose_groups(e, POOD_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1001:
		if(++e->frame >= 0x68)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_pood, 4, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1002:
		if(++e->frame >= 0x7e)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_pood, 4, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1003:
		if(++e->frame >= 0xb)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_pood, 4, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1004:
		if(++e->frame >= 0xb)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_pood, 4, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1005:
		if(++e->frame >= 0xb)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_pood, 4, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1006:
		if(++e->frame >= 0xb)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_pood, 4, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x8801:
		path_follow_turned(e, pood_path, e->path, 1.0f);
		if(e->path > e->unk92)
		{
			pood_next(e);
		}
		if(++e->frame >= 0xb)
		{
			e->frame = 0;
		}
		break;
	case 1:
		if(++e->frame >= 0xb)
		{
			e->frame = 0;
		}
		face_camera(e, 0.2f);
		pood_approach(e);
		break;
	case 0x201:
		face_camera(e, 0.2f);
		if(++e->frame == 0x13 && group_near_camera(&e->groups[POOD_HEAD], 20.0f))
		{
			hurt_player(e->target, POOD_BITE, 3, 0.0f);
		}
		if(e->frame >= 0x1a)
		{
			pood_approach(e);
		}
		break;
	case 0x202:
		face_camera(e, 0.2f);
		if(++e->frame == 0x13 && group_near_camera(&e->groups[POOD_HEAD], 20.0f))
		{
			hurt_player(e->target, POOD_BITE, 3, 0.0f);
		}
		if(e->frame >= 0x1a)
		{
			pood_approach(e);
		}
		break;
	case 0x203:
		face_camera(e, 0.2f);
		if(++e->frame == 0x13 && group_near_camera(&e->groups[POOD_HEAD], 20.0f))
		{
			hurt_player(e->target, POOD_BITE, 3, 0.0f);
		}
		if(e->frame >= 0x1a)
		{
			pood_approach(e);
		}
		break;
	case 3:
		if(++e->frame >= 0x92)
		{
			pood_approach(e);
		}
		break;
	case 2:
		if(++e->frame >= 0x92)
		{
			e->frame = 0x7e;
		}
		break;
	case 4:
		if(++e->frame >= 0x2a)
		{
			e->frame = 0x1a;
		}
		break;
	case 5:
		if(++e->counter >= 0x28)
		{
			e->frame++;
			pood_next(e);
		}
		break;
	case 0x2001:
		if(++e->frame >= 0x3c)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		break;
	case 0x2002:
		if(++e->frame >= POOD_FRAMES)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		break;
	case 0:
		if(++e->frame >= POOD_FRAMES)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x80131a9c: the spawn table's state and its first frame (0x8801 also takes its place on the
   path at once) */
void func_80131a9c(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	switch(state)
	{
	case 0x201:
		e->frame = 0xb;
		break;
	case 2:
		e->frame = 0x7e;
		break;
	case 4:
		e->frame = 0x1a;
		break;
	case 5:
		e->counter = 0;
		e->frame = 0x3c;
		break;
	case 0x8801:
		e->frame = 0;
		path_follow_turned(e, pood_path, e->path, 1.0f);
		break;
	case 1:
		e->frame = 0;
		break;
	case 0:
	default:
		e->frame = 0;
		break;
	}
}

/* 0x80131b7c: hunting: steer at a point 28 units in front of the camera; there, crouch (3); in
   reach, a call and one of three bites (0x201 35%, 0x202 30%, 0x203 35%); else walk (1) */
void pood_approach(enemy_t *e)
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
	r = step_toward_goal_xz(e, 2.0f);
	switch(r)
	{
	case 1:
		if(e->ai_state == 3)
		{
			e->frame = 0x7e;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_pood, 4, 0x7e, 2);
		e->ai_state = 3;
		break;
	case 2:
		play_enemy_sound(e, pood_attack_sounds[randrng(12)], "pood", 0);
		if(e->frame > 0)
		{
			e->frame--;
		}
		n = randrng(100);
		if(n < 35)
		{
			blend_to_frame(e, submesh_info_pood, 4, 0xb, 2);
			e->ai_state = 0x201;
		}
		else if(n < 65)
		{
			blend_to_frame(e, submesh_info_pood, 4, 0xb, 2);
			e->ai_state = 0x202;
		}
		else
		{
			blend_to_frame(e, submesh_info_pood, 4, 0xb, 2);
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
		blend_to_frame(e, submesh_info_pood, 4, 0, 2);
		e->ai_state = 1;
		break;
	}
}

/* 0x80131dac: the end of the path or of the wait: a call, then the idle (2) when the spawn
   entry asks for it, else the hunt */
void pood_next(enemy_t *e)
{
	play_enemy_sound(e, pood_attack_sounds[randrng(12)], "pood", 0);
	if((unsigned short)e->unk6 == 2)
	{
		e->unk6 = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_pood, 4, 0x7e, 2);
		e->ai_state = 2;
	}
	else
	{
		e->unk6 = 0;
		pood_approach(e);
	}
}

/* 0x80131e4c, 0x80131eac: two pairs of parts knocked loose (13 and 14 from 14's place, 12 and
   11 from 11's); nothing calls them */
void pood_drop_parts1(enemy_t *e)
{
	detach_group(&e->groups[13], e->groups[14].pos, e->y, e->scale);
	detach_group(&e->groups[14], e->groups[14].pos, e->y, e->scale);
}

void pood_drop_parts2(enemy_t *e)
{
	detach_group(&e->groups[12], e->groups[11].pos, e->y, e->scale);
	detach_group(&e->groups[11], e->groups[11].pos, e->y, e->scale);
}
