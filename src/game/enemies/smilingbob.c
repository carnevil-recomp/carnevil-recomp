/*
 * smilingbob.c - Smiling Bob, the Rickety Town zombie (object type 0x2044, assets SMIL.ZM /
 * smil.za / smil.pth and the smil*.WMS texmaps). One original file, 0x80129d58-0x8012b7d4,
 * tables at 0x8019b9f8 (texmaps) and 0x8019c218 (submeshes, smilingbob_data.c). The model has
 * 984 vertices, 1036 normals and 1640 faces in 23 submeshes, the animation 567 frames, the
 * path 1102 points. Submeshes 18-21 are the loose halves of the arms and the head (they share
 * the animation of 9, 13, 11 and 16 and are hidden until shot off), 22 is the part a sliding
 * Bob (0x84) hits with.
 *
 * He walks his path (0x801) or slides along it (0x84, spawn bit 0x80: half his parts hidden,
 * 3.5 health), and off the path walks at the player (1), swings (0x201, or 0x202 once his arm
 * is gone, bit 0x100) and stands (3). Shots knock parts off at violence 3 (the arms in two
 * steps, the head in two hits); the hit reactions are 0x1001-0x1006 and 0x1087, the deaths
 * 0x2001 (shot), 0x2002 (acid) and 0x2083 (sliding), then 0x4000.
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
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);	/* text at a world point */
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);		/* objects.c */
extern void	set_enemy_health(enemy_t *, float);		/* health by difficulty */
extern player_stats_t *get_target_player(void);		/* objects.c */
extern int	randrng(int);
extern void	attach_effect(enemy_t *, void *);
extern void	*attach_flame(float, float, group_t *);
extern void	*attach_spray(float *, float *, group_t *);
extern void	path_place(enemy_t *, void *, short, float);	/* start along a path */
extern void	path_follow(enemy_t *, void *, short, float);	/* step along a path */
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern int	face_camera(enemy_t *, float);			/* enemy.c */
extern int	group_near_camera(group_t *, float);		/* a group within reach of the player */
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);	/* blend to a frame */
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);	/* hit reaction */
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal_xz(enemy_t *, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);	/* the shot hits the sphere */
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);		/* effects.c takes two; this file passes a third (0x800000), as the original does */
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);	/* sprites.c */
extern void	add_smoke(float *, unsigned int);
extern void	replace_group(enemy_t *, group_t *, group_t *);	/* a part comes off (not decompiled yet) */
extern void	detach_group(group_t *, float *, float, float *);	/* a part falls off */
extern float	shot_damage_of(player_stats_t *);		/* the shot's damage */
extern void	award_points(int, int);			/* award points */
extern int	mark_die();					/* spawn.c; the original passes the player too */
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern enemy_t	*debug_enemy;
extern char	text_buffer[];
extern int	violence_level;

float	*smil_verts;			/* 0x801e5b80 */
face_t	*smil_faces;			/* 0x8021f0bc */
float	*smil_normals;			/* 0x801e583c */
void	*smil_path;			/* 0x802373d4 */
float	smil_spray_dir[3] = { 0.125f, 0.0f, 0.0f };	/* 0x8019c4f8: the blood from his neck */
float	smil_spray_base[3] = { 0.05f, 0.0f, 0.0f };	/* 0x8019c504 */
int	smil_taunt_sounds[14] = { 1, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16 };	/* 0x8019c510 */
int	smil_hurt_sounds[14] = { 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 31 };	/* 0x8019c548 */

extern texmap_t	smilingbob_texmap[0x41];		/* smilingbob_data.c */
extern submesh_t submesh_info_smil[0x17];
int	smilingbob_die(game_object_t *, player_stats_t *);
void	smilingbob_remove(game_object_t *);
void	smilingbob_setup(enemy_t *, unsigned short);
void	smilingbob_steer(enemy_t *);
void	smilingbob_end_path(enemy_t *);

/* the states */
#define SMIL_IDLE	0
#define SMIL_WALK	1		/* at the player */
#define SMIL_STAND	2		/* where his path left him */
#define SMIL_WAIT	3		/* in reach, between swings */
#define SMIL_LIE	5
#define SMIL_SWING	0x201
#define SMIL_SWING1	0x202		/* one-armed */
#define SMIL_SLIDE	0x84		/* sliding along the path (spawn bit 0x80) */
#define SMIL_SLIDE_GRAB	0x86		/* ... reaching at the end of it (bit 0x40) */
#define SMIL_SLIDE_BITE	0x87		/* ... or biting (bit 0x20) */
#define SMIL_PATH	0x801
#define SMIL_DEAD	0x4000

/* 0x80129d58: the draw callback */
void smilingbob_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_smil, 0x17, smil_verts, 0x3d8, smil_faces, 0x668, smil_normals, 0x40c);
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

/* 0x80129f54: the hit test: the bounding sphere, then the submesh */
int smilingbob_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_smil, 0x17, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)smilingbob_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x8012a010: the shot lands. Part 22 (the slide's hitting part) only clangs. Otherwise blood,
   acid and flame as usual; at violence 3 parts come off: the head (16) in two hits (the second
   breaks it off with a spray), the left (8-10) and right (12-14) arms in two (marked, then off),
   the jaw (11) and the hand (7) when hit; then damage, death (sliding, acid or shot) or a hit
   reaction by part */
int smilingbob_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;
	int	group;
	int	i;
	int	acid;

	e = (enemy_t *)obj->data;
	acid = 0;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return(0);
	}
	group = p->hit_group;
	g = &e->groups[group];
	if(group == 0x16)
	{
		play_rotating_cue(0x95, "generic1");
		return(0);
	}
	burst_blood(p->hit_point, e->unk98, 0x800000);
	add_chunks(e->hit_group->pos, e->unk98);
	add_world_sprite(p->hit_point, 0, 0);
	if(p->shot_weapon == 0x1000005)
	{
		for(i = 0; i < 0x17; i++)
		{
			if(i != 0x16)
			{
				e->groups[i].flags |= 0x20;
				add_smoke(e->groups[i].pivot, 0xd0ff00);
			}
		}
		e->health = 0.01f;
		acid = 1;
	}
	if(p->shot_weapon == 0x1000004)
	{
		if(!(g->flags & 2))
		{
			g->flags |= 2;
			attach_effect(e, attach_flame(0.0f, (submesh_info_smil[group].radius + submesh_info_smil[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 16:
			e->groups[15].flags |= 0xf0000000;
			e->unka8++;
			switch(e->unka8)
			{
			case 0:
				printf("This should never happen! Zomb head damage at 0\n");
				break;
			case 1:
				g = &e->groups[group];
				g->flags |= 0xf0000000;
				break;
			case 2:
				replace_group(e, &e->groups[16], &e->groups[21]);
				attach_effect(e, attach_spray(smil_spray_base, smil_spray_dir, &e->groups[21]));
				break;
			}
			break;
		case 11:
			if(!(g->flags & 0xf0000000))
			{
				g->flags |= 0xf0000000;
				break;
			}
			replace_group(e, &e->groups[11], &e->groups[20]);
			e->groups[7].flags |= 0x20000000;
			break;
		case 7:
			e->groups[7].flags |= 0x10000000;
			break;
		case 12:
		case 13:
		case 14:
			if(e->groups[12].flags & 0x20000000)
			{
				break;
			}
			if(e->groups[12].flags & 0x10000000)
			{
				e->groups[12].flags = (e->groups[12].flags & ~0x10000000) | 0x20000000;
				replace_group(e, &e->groups[13], &e->groups[19]);
				break;
			}
			e->groups[12].flags |= 0x10000000;
			break;
		case 8:
		case 9:
		case 10:
			if(e->groups[8].flags & 0x20000000)
			{
				break;
			}
			if(e->groups[8].flags & 0x10000000)
			{
				e->groups[8].flags = (e->groups[8].flags & ~0x10000000) | 0x20000000;
				replace_group(e, &e->groups[9], &e->groups[18]);
				break;
			}
			e->groups[8].flags |= 0x10000000;
			break;
		default:
			g->flags |= 0xf0000000;
			break;
		}
	}
	if(!(e->ai_state & 0x6000))
	{
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			award_points(p->index, 500);
			play_enemy_sound(e, 0x1a, "bob", 8);
			e->killer = p;
			mark_die(obj->id, p);
			if(e->flags2 & 0x80)
			{
				e->ai_state = 0x2083;
				e->frame = 0x1b8;
			}
			else if(acid)
			{
				e->ai_state = 0x2002;
				e->frame = 0x20f;
			}
			else
			{
				e->ai_state = 0x2001;
				e->frame = 0x7a;
			}
			e->flags &= 0xfffd;
		}
		else
		{
			award_points(p->index, 50);
			play_enemy_sound(e, smil_hurt_sounds[randrng(14)], "bob", 2);
		}
	}
	if(e->ai_state & 0xe000)
	{
		return(0);
	}
	if(e->flags2 & 0x80)
	{
		start_hit_reaction(e, 0x17e, 0x1087, submesh_info_smil, 0x16);
		return(0);
	}
	switch(group)
	{
	case 7:
		start_hit_reaction(e, 0x66, 0x1003, submesh_info_smil, 7);
		break;
	case 15:
	case 16:
	case 21:
		start_hit_reaction(e, 0xeb, 0x1004, submesh_info_smil, 7);
		break;
	case 11:
	case 20:
		if(randrng(100) < 50)
		{
			start_hit_reaction(e, 0xbc, 0x1001, submesh_info_smil, 7);
		}
		else
		{
			start_hit_reaction(e, 0xd0, 0x1002, submesh_info_smil, 7);
		}
		break;
	case 1:
	case 2:
	case 3:
		start_hit_reaction(e, 0xa9, 0x1006, submesh_info_smil, 7);
		break;
	case 4:
	case 5:
	case 6:
		start_hit_reaction(e, 0x95, 0x1005, submesh_info_smil, 7);
		break;
	default:
		start_hit_reaction(e, 0xbc, 0x1001, submesh_info_smil, 7);
		break;
	}
	return(0);
}

/* 0x8012a600: the texmaps */
void load_smilingbob_texmap(void)
{
	load_texmap(smilingbob_texmap, 0x41, "smil");
}

/* 0x8012a630: the model, the animation and the path. The loose parts (18-21) move with the
   parts they break off from, so their own animation tracks are freed and shared */
void load_smilingbob(void)
{
	load_model((void **)&smil_verts, 0x3d8, (void **)&smil_faces, 0x668, (void **)&smil_normals, 0x40c, "SMIL.ZM", 0x3226392d);
	load_anim(submesh_info_smil, 0x17, 0x237, "smil.za", 0x4e0f8050);
	load_path(&smil_path, 0x44e, "smil.pth", 0x9d535756);
	free(submesh_info_smil[18].anim);
	free(submesh_info_smil[19].anim);
	free(submesh_info_smil[20].anim);
	free(submesh_info_smil[21].anim);
	submesh_info_smil[18].anim = submesh_info_smil[9].anim;
	submesh_info_smil[19].anim = submesh_info_smil[13].anim;
	submesh_info_smil[20].anim = submesh_info_smil[11].anim;
	submesh_info_smil[21].anim = submesh_info_smil[16].anim;
}

/* 0x8012a758: the shared tracks are cleared first so they are freed once */
void remove_smilingbob_assets(void)
{
	submesh_info_smil[18].anim = 0;
	submesh_info_smil[19].anim = 0;
	submesh_info_smil[20].anim = 0;
	submesh_info_smil[21].anim = 0;
	remove_surface(smilingbob_texmap, 0x41);
	remove_path(&smil_path, 0x44e, "smil.pth");
	remove_animation(submesh_info_smil, 0x17, 0x237, "smil.za");
	remove_model((void **)&smil_verts, 0x3d8, (void **)&smil_faces, 0x668, (void **)&smil_normals, 0x40c, "SMIL.ZM");
}

/* 0x8012a818: the init callback: the record, the groups (the loose parts 18-22 hidden), the
   first state */
int smilingbob_init(game_object_t *obj)
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
	obj->hit = smilingbob_hit;
	obj->die = smilingbob_die;
	obj->remove = smilingbob_remove;
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
	e->unkcc = 0.92f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 7.5f);
	e->groups = malloc(0x17 * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < 0x17; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[18].flags = 0;
	e->groups[19].flags = 0;
	e->groups[20].flags = 0;
	e->groups[21].flags = 0;
	e->groups[22].flags = 0;
	e->unka8 = 0;
	e->unkac = 1;
	init_groups(e, submesh_info_smil, 0x17);
	g = e->groups;
	e->hit_group = &g[7];
	g[7].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	smilingbob_setup(e, 0);
	return(0);
}

/* 0x8012a9d0: the remove callback */
void smilingbob_remove(game_object_t *obj)
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

/* 0x8012aa28: the think callback. A sliding Bob follows his path whatever his state; the
   swings hurt a player in reach of a hand (group 14 or 10) on their strike frames */
void smilingbob_think(game_object_t *obj)
{
	enemy_t	*e;
	short	frame;

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
	animate_loose_groups(e, 0x17);
	if((e->flags2 & 0x80) && !(e->ai_state & 0x6000))
	{
		path_follow(e, smil_path, e->path, 1.0f);
		if(e->path >= e->unk92)
		{
			e->path--;
		}
	}
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1004:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0xfa)
		{
			break;
		}
		goto resume;
	case 0x1003:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x7a)
		{
			break;
		}
		goto resume;
	case 0x1001:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0xd0)
		{
			break;
		}
		goto resume;
	case 0x1002:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0xeb)
		{
			break;
		}
		goto resume;
	case 0x1005:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0xa9)
		{
			break;
		}
		goto resume;
	case 0x1006:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0xbc)
		{
			break;
		}
		goto resume;
	case 0x1087:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x18c)
		{
			break;
		}
	resume:
		e->frame = frame;
		blend_to_frame(e, submesh_info_smil, 7, e->resume_frame, 2);
		e->ai_state = e->resume_state;
		break;
	case SMIL_WALK:
		e->frame++;
		if(e->frame >= 0x66)
		{
			e->frame = 0x55;
		}
		face_camera(e, 0.2f);
		smilingbob_steer(e);
		break;
	case SMIL_SWING:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame == 0xe)
		{
			if(group_near_camera(&e->groups[14], 32.0f))
			{
				hurt_player(e->target, 0xdc, 3, 0.0f);
			}
		}
		if(e->frame == 0x29)
		{
			if(group_near_camera(&e->groups[14], 32.0f))
			{
				hurt_player(e->target, 0xdc, 3, 0.0f);
			}
		}
		if(e->frame == 0x45)
		{
			if(group_near_camera(&e->groups[14], 32.0f))
			{
				hurt_player(e->target, 0xdc, 3, 0.0f);
			}
		}
		if(e->frame < 0x55)
		{
			break;
		}
		smilingbob_steer(e);
		break;
	case SMIL_SWING1:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame == 0x191)
		{
			if(group_near_camera(&e->groups[14], 32.0f))
			{
				hurt_player(e->target, 0xdc, 3, 0.0f);
			}
		}
		if(e->frame == 0x198)
		{
			if(group_near_camera(&e->groups[10], 32.0f))
			{
				hurt_player(e->target, 0xdc, 3, 0.0f);
			}
		}
		if(e->frame == 0x19f)
		{
			if(group_near_camera(&e->groups[10], 32.0f))
			{
				hurt_player(e->target, 0xdc, 3, 0.0f);
			}
		}
		if(e->frame < 0x1b8)
		{
			break;
		}
		smilingbob_steer(e);
		break;
	case SMIL_WAIT:
		e->frame++;
		if(e->frame < 0x20f)
		{
			break;
		}
		smilingbob_steer(e);
		break;
	case SMIL_STAND:
		e->frame++;
		if(e->frame >= 0x55)
		{
			e->frame = 0;
		}
		break;
	case SMIL_LIE:
		e->frame++;
		if(e->frame >= 0x17e)
		{
			e->frame = 0x110;
		}
		break;
	case SMIL_SLIDE:
		e->frame++;
		if(e->frame >= 0x110)
		{
			e->frame = 0xfa;
		}
		if((e->flags2 & 0x40) && e->path >= e->unk92 - 0x13)
		{
			e->ai_state = SMIL_SLIDE_GRAB;
			e->frame = 0x1cc;
			play_enemy_sound(e, smil_taunt_sounds[randrng(14)], "bob", 0);
		}
		if((e->flags2 & 0x20) && e->path >= e->unk92 - 0x10)
		{
			e->ai_state = SMIL_SLIDE_BITE;
			e->frame = 0x1e7;
			play_enemy_sound(e, smil_taunt_sounds[randrng(14)], "bob", 0);
		}
		if(e->unkac)
		{
			if(group_near_camera(&e->groups[22], 32.0f))
			{
				e->unkac = 0;
				hurt_player(e->target, 0xdc, 3, 0.0f);
			}
		}
		break;
	case SMIL_SLIDE_GRAB:
		e->frame++;
		if(e->frame == 0x1d4)
		{
			if(group_near_camera(&e->groups[10], 32.0f))
			{
				hurt_player(e->target, 0xdc, 3, 0.0f);
			}
		}
		if(e->frame >= 0x1e7)
		{
			e->frame = 0x1cc;
		}
		break;
	case SMIL_SLIDE_BITE:
		e->frame++;
		if(e->frame == 0x1ee)
		{
			if(group_near_camera(&e->groups[14], 32.0f))
			{
				hurt_player(e->target, 0xdc, 3, 0.0f);
			}
		}
		if(e->frame >= 0x200)
		{
			e->frame = 0x1e7;
		}
		break;
	case SMIL_PATH:
		e->frame++;
		if(e->frame >= 0x66)
		{
			e->frame = 0x55;
		}
		path_place(e, smil_path, e->path, 1.0f);
		e->path++;
		if(e->path >= e->unk92)
		{
			smilingbob_end_path(e);
		}
		break;
	case 0x2001:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x95)
		{
			break;
		}
		goto dead;
	case 0x2002:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x237)
		{
			break;
		}
		goto dead;
	case 0x2083:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x1cc)
		{
			break;
		}
	dead:
		e->frame = frame;
		e->ai_state = SMIL_DEAD;
		break;
	case SMIL_IDLE:
		e->frame++;
		if(e->frame >= 0x237)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x8012b260: the state set-up. The spawn entry's high bits pick a variant: 0x80 the slide
   (the legs and the lower body hidden, the slide's hitting part shown, 3.5 health), 0x100 the
   one-armed Bob, 0x40 and 0x20 the slide's grab and bite at the end of the path */
void smilingbob_setup(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	if(state & 0x80)
	{
		e->groups[22].flags = 0xa000;
		e->groups[0].flags = 0;
		e->groups[17].flags = 0;
		e->groups[1].flags = 0;
		e->groups[2].flags = 0;
		e->groups[3].flags = 0;
		e->groups[4].flags = 0;
		e->groups[5].flags = 0;
		e->groups[6].flags = 0;
		e->flags2 |= 0x80;
		set_enemy_health(e, 3.5f);
	}
	if(e->ai_state & 0x100)
	{
		e->groups[0].flags = 0;
		e->groups[17].flags = 0;
		e->ai_state &= ~0x100;
		e->flags2 |= 0x100;
	}
	if(e->ai_state & 0x40)
	{
		e->ai_state &= ~0x40;
		e->flags2 |= 0x40;
		e->groups[0].flags = 0xa000;
	}
	if(e->ai_state & 0x20)
	{
		e->ai_state &= ~0x20;
		e->flags2 |= 0x20;
		e->groups[0].flags = 0xa000;
		e->groups[17].flags = 0xa000;
	}
	switch(e->ai_state)
	{
	case SMIL_PATH:
		e->frame = 0x55;
		path_place(e, smil_path, e->path, 1.0f);
		break;
	case SMIL_WALK:
		e->frame = 0x55;
		break;
	case SMIL_LIE:
		e->frame = 0x110;
		break;
	case SMIL_SLIDE:
		e->frame = 0xfa;
		path_place(e, smil_path, e->path, 1.0f);
		break;
	case SMIL_IDLE:
	case SMIL_SWING:
	default:
		e->frame = 0;
		break;
	}
}

/* 0x8012b43c: off the path: keep a playing target, walk at a point 18 short of the camera on
   the ground and pick the next state from the steering result (1 wait, 2 swing, 0 walk); a
   swing starts with a taunt */
void smilingbob_steer(enemy_t *e)
{
	player_stats_t *p;
	int	r;

	if(e->target->state != 4)
	{
		p = get_target_player();
		if(p->state == 4)
		{
			e->target = p;
		}
	}
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0, -18.0f);
	r = step_toward_goal_xz(e, 0.5f);
	switch(r)
	{
	case 1:
		if(e->ai_state == SMIL_WAIT)
		{
			e->frame = 0x200;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_smil, 7, 0x200, 2);
		e->ai_state = SMIL_WAIT;
		break;
	case 2:
		if(e->frame > 0)
		{
			e->frame--;
		}
		if(e->flags2 & 0x100)
		{
			if(e->ai_state == SMIL_SWING1)
			{
				e->frame = 0x18c;
			}
			else
			{
				blend_to_frame(e, submesh_info_smil, 7, 0x18c, r);
				e->ai_state = SMIL_SWING1;
			}
		}
		else
		{
			if(e->ai_state == SMIL_SWING)
			{
				e->frame = 0;
			}
			else
			{
				blend_to_frame(e, submesh_info_smil, 7, 0, r);
				e->ai_state = SMIL_SWING;
			}
		}
		play_enemy_sound(e, smil_taunt_sounds[randrng(14)], "bob", 0);
		break;
	case 0:
		if(e->ai_state == SMIL_WALK)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_smil, 7, 0x55, 2);
		e->ai_state = SMIL_WALK;
		break;
	}
}

/* 0x8012b674: the path ran out: a spawn entry marked 2 stands, the rest hunt */
void smilingbob_end_path(enemy_t *e)
{
	if((unsigned short)e->unk6 == 2)
	{
		e->unk6 = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_smil, 7, 0, 2);
		e->ai_state = SMIL_STAND;
	}
	else
	{
		e->unk6 = 0;
		smilingbob_steer(e);
	}
}

/* 0x8012b6e4: the right arm falls off (the hand, the forearm, the upper arm) */
void func_8012b6e4(enemy_t *e)
{
	detach_group(&e->groups[14], e->groups[13].pos, e->y, e->scale);
	detach_group(&e->groups[13], e->groups[13].pos, e->y, e->scale);
	detach_group(&e->groups[12], e->groups[12].pos, e->y, e->scale);
}

/* 0x8012b75c: the left arm falls off */
void func_8012b75c(enemy_t *e)
{
	detach_group(&e->groups[10], e->groups[9].pos, e->y, e->scale);
	detach_group(&e->groups[9], e->groups[9].pos, e->y, e->scale);
	detach_group(&e->groups[8], e->groups[8].pos, e->y, e->scale);
}
