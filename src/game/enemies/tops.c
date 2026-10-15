/*
 * tops.c - the Freak Show knife thrower TOPS (object type 0x2020; TOPS.ZM / tops.za / tops.pth
 * and the tops*.WMS texmaps; its cries are in the sound bank "flapjack"). One original file,
 * 0x801163e8-0x80118728 (the Freak Show script's callbacks precede it), rodata
 * 0x801734b8-0x8017362c, tables 0x801946d8-0x80195078 (tops_data.c). The model has 778
 * vertices, 801 normals and 1333 faces in 21 submeshes, the animation 513 frames.
 *
 * Frames: 0-512 the whole animation (state 0 loops it), 0x22-0x44 the throw, 0x76-0xe1 and
 * 0xe1-0xf5 two deaths, 0xf5-0x104 a stand, 0x104-0x12b and 0x146-0x159 walks, 0x159-0x177 the
 * run, 0x177-0x1e4 the attacks, 0x1de-0x1e4 a death loop, 0x1e4-0x201 another death. Half of
 * them spawn holding a knife (flags2 0x80, a held object on group 4).
 *
 * States: 2 running at the player, 3 and 8 running in place (8 sinking), 4 running for 40 frames,
 * 5 throwing knives at a fixed point, 6 standing, 7 walking at the player; 0x201-0x203 knife
 * slashes and 0x204 a two-handed strike (with a knife), 0x208-0x20a punches (without), 0x205
 * a strike with group 20, 0x206 a knife throw at the camera; 0x803-0x806 and 0x8801 walking its
 * path; 0x1001-0x1003 hit reactions; 0x2001-0x2004 deaths (then 0x4000). The set-up's state bits:
 * 0x80 take a knife, 0x40 drop it, 0x20 and 0x100 kept in flags2 (0x100: lying, 35 units lower).
 *
 * The exported names are the address names the shared files (spawn.c, wave.c) use:
 * tops_draw is the draw callback, tops_init the init, tops_think the think,
 * tops_setup the state set-up.
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

extern texmap_t	tops_texmap[50];		/* tops_data.c */
extern submesh_t submesh_info_tops[21];
extern float	tops_spray1a[3], tops_spray1b[3], tops_spray2a[3], tops_spray2b[3];
extern float	tops_spray3a[3], tops_spray3b[3], tops_spray4a[3], tops_spray4b[3];
extern int	tops_hurt_sounds[20], tops_die_sounds[4];

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
extern void	*attach_spray(float *, float *, group_t *);
extern void	*attach_held_object(group_t *, int, float, float, float, float, float, float, float *);
extern void	throw_at_camera(float *, int, float *);
extern void	throw_at(float *, int, float *, float *);
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern int	randrng(int);
extern void	path_place(enemy_t *, void *, short, float);
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
extern void	replace_group(enemy_t *, group_t *, group_t *);	/* swap a shot part for its damaged version */
extern void	detach_group(group_t *, float *, float, float *);	/* a part flies off */
extern int	mark_die();							/* spawn.c: (id, killer) */
extern int	remove_die();
extern void	init_groups(enemy_t *, submesh_t *, int);
extern unsigned int next_cue_priority(void);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);

float	*tops_verts;			/* 0x801e5bf0 */
face_t	*tops_faces;			/* 0x8023729c */
float	*tops_normals;			/* 0x801e5b40 */
void	*tops_path;			/* 0x80237434 */

#define TOPS_GROUPS	0x15
#define TOPS_NVERTS	0x30a
#define TOPS_NFACES	0x535
#define TOPS_NNORMALS	0x321
#define TOPS_FRAMES	0x201
#define TOPS_PATH_LEN	0x247
#define TOPS_KNIFE	0x13		/* the held object kinds: the knife it holds ... */
#define TOPS_KNIVES	0x10		/* ... and the ones it throws at a point */

int	tops_die(game_object_t *, player_stats_t *);
void	tops_remove(game_object_t *);
void	tops_setup(enemy_t *, unsigned short);
void	tops_go_for_player(enemy_t *);
void	tops_next_state(enemy_t *);
void	tops_drop_left_arm(enemy_t *);
void	tops_drop_right_arm(enemy_t *);

/* 0x801163e8: the draw callback (type 0x2020) */
void tops_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_tops, TOPS_GROUPS, tops_verts, TOPS_NVERTS, tops_faces, TOPS_NFACES, tops_normals, TOPS_NNORMALS);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[7].pos, e->radius, 0x600000);
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

/* 0x801165e4: the hit test: the bounding sphere, then the submesh */
int tops_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_tops, TOPS_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
			p->hit_group = group;
			p->die = (void *)tops_die;
		}
	}
	return(0);
}

/* 0x801166a0: the shot lands: blood; a hit while running (states 5, 6) stops it; the flame
   thrower sets the part alight; at the highest violence parts come off (groups 1-2 and 16/19 in
   two shots, group 7 knocks both arms loose one time in eight); damage, 50 points and a cry, or
   500 points, a death cry and one of four deaths; else a hit reaction by the part */
int tops_die(game_object_t *obj, player_stats_t *p)
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
	if((unsigned short)(e->ai_state - 5) < 2)
	{
		e->ai_state = 7;
		e->frame = 0x146;
	}
	if(p->shot_weapon == 0x1000004)
	{
		if(!(g->flags & 2))
		{
			g->flags |= 2;
			attach_effect(e, attach_flame(0.0f, (submesh_info_tops[group].radius + submesh_info_tops[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 1:
		case 2:
			e->unka8++;
			switch(e->unka8)
			{
			case 0:
				printf("This should never happen! Zomb head damage at 0\n");
				break;
			case 1:
				g = &e->groups[group];
				g->flags |= 0xf0000000;
				e->groups[10].flags |= 0xf0000000;
				break;
			case 2:
				replace_group(e, &e->groups[1], &e->groups[2]);
				attach_effect(e, attach_spray(tops_spray1b, tops_spray1a, &e->groups[2]));
				break;
			}
			break;
		case 16:
		case 19:
			e->unkac++;
			switch(e->unkac)
			{
			case 0:
				printf("This should never happen! Tops sack damage at 0\n");
				break;
			case 1:
				g = &e->groups[group];
				g->flags |= 0xf0000000;
				e->groups[9].flags |= 0xf0000000;
				break;
			case 2:
				replace_group(e, &e->groups[16], &e->groups[19]);
				attach_effect(e, attach_spray(tops_spray2b, tops_spray2a, &e->groups[19]));
				break;
			}
			break;
		case 7:
			if(g->flags & 0xf0000000)
			{
				if(randrng(8) == 1)
				{
					replace_group(e, &e->groups[7], &e->groups[0]);
					e->groups[10].flags = 0;
					tops_drop_left_arm(e);
					tops_drop_right_arm(e);
					e->groups[1].flags |= 0xf0000000;
				}
				break;
			}
			g->flags |= 0xf0000000;
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
			play_enemy_sound(e, tops_die_sounds[randrng(4)], "flapjack", 8);
			e->killer = p;
			mark_die(obj->id, p);
			if(e->flags2 & 0x20)
			{
				e->ai_state = 0x2002;
				e->frame = 0xe1;
				e->flags &= 0xfffd;
			}
			else if(e->flags2 & 0x100)
			{
				e->ai_state = 0x2003;
				e->frame = 0x1de;
				e->flags &= 0xfffd;
			}
			else if(group == 1 || group == 2 || group == 10 || group == 7 || group == 0 || group == 8)
			{
				if(randrng(100) < 50)
				{
					e->ai_state = 0x2001;
					e->frame = 0x76;
					e->flags &= 0xfffd;
				}
				else
				{
					e->ai_state = 0x2004;
					e->frame = 0x1e4;
					e->flags &= 0xfffd;
				}
			}
			else
			{
				e->ai_state = 0x2002;
				e->frame = 0xe1;
				e->flags &= 0xfffd;
			}
		}
		else
		{
			play_enemy_sound(e, tops_hurt_sounds[randrng(20)], "flapjack", 2);
			award_points(p->index, 50);
		}
	}
	if(e->ai_state & 0xe000)
	{
		return(0);
	}
	switch(group)
	{
	case 17:
		start_hit_reaction(e, 0x55, 0x1002, submesh_info_tops, 0x11);
		break;
	case 1:
		start_hit_reaction(e, 0x44, 0x1003, submesh_info_tops, 0x11);
		break;
	case 7:
	default:
		start_hit_reaction(e, 0x6b, 0x1001, submesh_info_tops, 0x11);
		break;
	}
	return(0);
}

/* 0x80116c20 */
void load_tops_texmap(void)
{
	load_texmap(tops_texmap, 0x32, "tops");
}

/* 0x80116c50: the model, the animation and the path; groups 19, 2 and 0 take the animations of
   groups 16, 1 and 7 */
void load_tops(void)
{
	load_model((void **)&tops_verts, TOPS_NVERTS, (void **)&tops_faces, TOPS_NFACES, (void **)&tops_normals, TOPS_NNORMALS, "TOPS.ZM", 0x47f8b1f9);
	load_anim(submesh_info_tops, TOPS_GROUPS, TOPS_FRAMES, "tops.za", 0x9c0a4d5b);
	load_path(&tops_path, TOPS_PATH_LEN, "tops.pth", 0xdb0f128a);
	free(submesh_info_tops[19].anim);
	free(submesh_info_tops[2].anim);
	free(submesh_info_tops[0].anim);
	submesh_info_tops[19].anim = submesh_info_tops[16].anim;
	submesh_info_tops[2].anim = submesh_info_tops[1].anim;
	submesh_info_tops[0].anim = submesh_info_tops[7].anim;
}

/* 0x80116d58: the borrowed animations are dropped first so they are not freed twice */
void remove_tops_assets(void)
{
	submesh_info_tops[19].anim = 0;
	submesh_info_tops[2].anim = 0;
	submesh_info_tops[0].anim = 0;
	remove_surface(tops_texmap, 0x32);
	remove_path(&tops_path, TOPS_PATH_LEN, "tops.pth");
	remove_animation(submesh_info_tops, TOPS_GROUPS, TOPS_FRAMES, "tops.za");
	remove_model((void **)&tops_verts, TOPS_NVERTS, (void **)&tops_faces, TOPS_NFACES, (void **)&tops_normals, TOPS_NNORMALS, "TOPS.ZM");
}

/* 0x80116e10: the init callback (type 0x2020): the record, 21 groups (0, 2 and 19 hidden: the
   damaged versions), 7.5 health, a knife in its hand half the time, the first state */
int tops_init(game_object_t *obj)
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
	obj->hit = tops_hit;
	obj->die = tops_die;
	obj->remove = tops_remove;
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
	e->unkcc = 0.92f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 7.5f);
	e->groups = malloc(TOPS_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < TOPS_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[19].flags = 0;
	e->groups[2].flags = 0;
	e->groups[0].flags = 0;
	e->unka8 = 0;
	e->unkac = 0;
	init_groups(e, submesh_info_tops, TOPS_GROUPS);
	g = e->groups;
	e->hit_group = &g[17];
	g[17].floor = 0;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	if(randrng(100) < 50)
	{
		e->flags2 = 0x80;
		attach_effect(e, attach_held_object(&e->groups[4], TOPS_KNIFE, 0.06f, 0.05f, 0.16f, 1.5f, -0.2f, 0.0f, e->scale));
	}
	else
	{
		e->flags2 = 0;
	}
	tops_setup(e, 0);
	return(0);
}

/* 0x80117028: the remove callback */
void tops_remove(game_object_t *obj)
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

/* 0x80117080: the think callback */
void tops_think(game_object_t *obj)
{
	enemy_t	*e;
	float	at[3];

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
	animate_loose_groups(e, TOPS_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 4:
		if(++e->frame >= 0x177)
		{
			e->frame = 0x159;
		}
		if(++e->counter >= 0x28)
		{
			tops_next_state(e);
		}
		break;
	case 0x804:
		e->path++;
		path_place(e, tops_path, e->path, 1.0f);
		if(e->path >= e->unk92)
		{
			e->path--;
		}
		if(++e->frame >= 0x22)
		{
			tops_next_state(e);
		}
		break;
	case 0x8801:
		if(++e->frame >= 0x22)
		{
			e->frame = 0;
		}
		path_place(e, tops_path, e->path, 1.0f);
		if(++e->path >= e->unk92)
		{
			tops_next_state(e);
		}
		break;
	case 0x805:
		if(++e->frame >= 0x12b)
		{
			e->frame = 0x104;
		}
		path_place(e, tops_path, e->path, 1.0f);
		if(++e->path >= e->unk92)
		{
			tops_next_state(e);
		}
		break;
	case 0x803:
		if(++e->frame >= 0x159)
		{
			e->frame = 0x146;
		}
		path_place(e, tops_path, e->path, 1.0f);
		if(++e->path >= e->unk92)
		{
			tops_next_state(e);
		}
		break;
	case 0x806:
		if(++e->frame >= 0x159)
		{
			e->frame = 0x146;
		}
		path_place(e, tops_path, e->path, 1.0f);
		if(++e->path >= e->unk92)
		{
			remove_die(obj->id, 0);
		}
		break;
	case 0x1003:
		if(++e->frame >= 0x55)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_tops, 0x11, e->resume_frame, 5);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1002:
		if(++e->frame >= 0x6b)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_tops, 0x11, e->resume_frame, 5);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1001:
		if(++e->frame >= 0x76)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_tops, 0x11, e->resume_frame, 5);
			e->ai_state = e->resume_state;
		}
		break;
	case 7:
		if(++e->frame >= 0x159)
		{
			e->frame = 0x146;
		}
		if(!(e->flags2 & 0x100))
		{
			face_camera(e, 0.2f);
		}
		tops_go_for_player(e);
		break;
	case 0x208:
		if(!(e->flags2 & 0x100))
		{
			face_camera(e, 0.2f);
		}
		if(++e->frame == 0x1c9 && group_near_camera(&e->groups[20], 25.0f))
		{
			hurt_player(e->target, 0xf0, 3, 0);
		}
		if(e->frame >= 0x1cc)
		{
			tops_go_for_player(e);
		}
		break;
	case 0x209:
		if(!(e->flags2 & 0x100))
		{
			face_camera(e, 0.2f);
		}
		if(++e->frame == 0x1d3 && group_near_camera(&e->groups[4], 25.0f))
		{
			hurt_player(e->target, 0xf0, 3, 0);
		}
		if(e->frame >= 0x1d6)
		{
			tops_go_for_player(e);
		}
		break;
	case 0x20a:
		if(!(e->flags2 & 0x100))
		{
			face_camera(e, 0.2f);
		}
		if(++e->frame == 0x1d7 && group_near_camera(&e->groups[4], 25.0f))
		{
			hurt_player(e->target, 0x18, 3, 0);
		}
		if(e->frame == 0x1db && group_near_camera(&e->groups[20], 25.0f))
		{
			hurt_player(e->target, 0x18, 3, 0);
		}
		if(e->frame >= 0x1de)
		{
			if(++e->counter < 6)
			{
				e->frame = 0x1d6;
			}
			else
			{
				tops_go_for_player(e);
			}
		}
		break;
	case 0x201:
		if(!(e->flags2 & 0x100))
		{
			face_camera(e, 0.2f);
		}
		if(++e->frame == 0x181 && group_near_camera(&e->groups[4], 25.0f))
		{
			hurt_player(e->target, 0xf0, 7, 0);
		}
		if(e->frame >= 0x18b)
		{
			tops_go_for_player(e);
		}
		break;
	case 0x202:
		if(!(e->flags2 & 0x100))
		{
			face_camera(e, 0.2f);
		}
		if(++e->frame == 0x194 && group_near_camera(&e->groups[4], 25.0f))
		{
			hurt_player(e->target, 0xf0, 7, 0);
		}
		if(e->frame >= 0x19f)
		{
			tops_go_for_player(e);
		}
		break;
	case 0x203:
		if(!(e->flags2 & 0x100))
		{
			face_camera(e, 0.2f);
		}
		if(++e->frame == 0x1b8 && group_near_camera(&e->groups[4], 25.0f))
		{
			hurt_player(e->target, 0xf0, 0xa, 0);
		}
		if(e->frame >= 0x1c2)
		{
			tops_go_for_player(e);
		}
		break;
	case 0x204:
		if(!(e->flags2 & 0x100))
		{
			face_camera(e, 0.2f);
		}
		if(++e->frame == 0x1a7 && (group_near_camera(&e->groups[1], 25.0f) || group_near_camera(&e->groups[2], 25.0f)))
		{
			hurt_player(e->target, 0xf0, 3, 0);
		}
		if(e->frame >= 0x1b1)
		{
			tops_go_for_player(e);
		}
		break;
	case 0x206:
		if(!(e->flags2 & 0x100))
		{
			face_camera(e, 0.2f);
		}
		if(++e->frame == 0x30 && (e->groups[4].flags & 0x2000))
		{
			throw_at_camera(e->groups[4].pos, TOPS_KNIFE, e->scale);
			detach_effect(e, &e->groups[4]);
		}
		if(e->frame >= 0x44)
		{
			tops_next_state(e);
		}
		break;
	case 5:
		if(++e->frame == 0x30 && (e->groups[4].flags & 0x2000))
		{
			at[0] = 661.0f;
			at[1] = 16.0f;
			at[2] = 109.0f;
			throw_at(e->groups[4].pos, TOPS_KNIVES, e->scale, at);
			detach_effect(e, &e->groups[4]);
		}
		if(e->frame >= 0x44)
		{
			e->frame = 0x22;
			attach_effect(e, attach_held_object(&e->groups[4], TOPS_KNIVES, 0.06f, 0.05f, 0.16f, 1.5f, -0.2f, 0.0f, e->scale));
		}
		break;
	case 6:
		if(++e->frame >= 0x104)
		{
			e->frame = 0xf5;
		}
		break;
	case 0x205:
		if(!(e->flags2 & 0x100))
		{
			face_camera(e, 0.2f);
		}
		if(++e->frame == 0x13a && group_near_camera(&e->groups[20], 25.0f))
		{
			hurt_player(e->target, 0xf0, 7, 0);
		}
		if(e->frame >= 0x146)
		{
			tops_next_state(e);
		}
		break;
	case 2:
		if(++e->frame >= 0x177)
		{
			tops_go_for_player(e);
		}
		break;
	case 3:
		if(++e->frame >= 0x177)
		{
			e->frame = 0x159;
		}
		break;
	case 8:
		e->y -= 0.1f;
		if(++e->frame >= 0x177)
		{
			e->frame = 0x159;
		}
		break;
	case 0x2001:
		if(++e->frame >= 0xe1)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		break;
	case 0x2002:
		if(++e->frame >= 0xf5)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		break;
	case 0x2003:
		if(++e->frame >= 0x1e4)
		{
			e->frame = 0x1de;
		}
		break;
	case 0x2004:
		if(++e->frame >= 0x201)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		break;
	case 0:
		if(++e->frame >= 0x201)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x80117c88: the state set-up: the bits 0x80 (take a knife), 0x40 (drop it), 0x20 and 0x100
   (kept in flags2; 0x100 turns it over) come off the state, then the state's first frame, its
   next state (unk6) and its path position; the floor its parts fall to */
void tops_setup(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	if(state & 0x80)
	{
		e->ai_state = state & 0xff7f;
		if(!(e->flags2 & 0x80))
		{
			e->flags2 |= 0x80;
			attach_effect(e, attach_held_object(&e->groups[4], TOPS_KNIFE, 0.06f, 0.05f, 0.16f, 1.5f, -0.2f, 0.0f, e->scale));
		}
	}
	if(e->ai_state & 0x40)
	{
		e->ai_state &= 0xffbf;
		if(e->flags2 & 0x80)
		{
			e->flags2 &= 0xff7f;
			detach_effect(e, &e->groups[4]);
		}
	}
	if(e->ai_state & 0x20)
	{
		e->ai_state &= 0xffdf;
		e->flags2 |= 0x20;
	}
	if(e->ai_state & 0x100)
	{
		e->ai_state &= 0xfeff;
		e->flags2 |= 0x100;
		e->rz = 3.1415927f;
	}
	switch(e->ai_state)
	{
	case 0x802:
		e->ai_state = 4;
		e->unk6 = 0x804;
		e->frame = 0x159;
		e->counter = 0;
		path_place(e, tops_path, e->path, 1.0f);
		break;
	case 0x205:
		e->frame = 0x12b;
		break;
	case 0x208:
		e->frame = 0x1c2;
		break;
	case 0x209:
		e->frame = 0x1cc;
		break;
	case 0x20a:
		e->frame = 0x1d6;
		break;
	case 0x201:
		e->frame = 0x177;
		break;
	case 0x202:
		e->frame = 0x18b;
		break;
	case 0x203:
		e->frame = 0x1b1;
		break;
	case 0x204:
		e->frame = 0x19f;
		break;
	case 0x206:
		e->frame = 0x22;
		break;
	case 0x207:
		e->frame = 0x22;
		e->unk6 = 8;
		break;
	case 0x20c:
		e->frame = 0x22;
		e->unk6 = 3;
		break;
	case 5:
		e->frame = 0x22;
		if(e->flags2 & 0x80)
		{
			e->flags2 &= 0xff7f;
			detach_effect(e, &e->groups[4]);
		}
		attach_effect(e, attach_held_object(&e->groups[4], TOPS_KNIVES, 0.06f, 0.05f, 0.16f, 1.5f, -0.2f, 0.0f, e->scale));
		break;
	case 6:
		e->frame = 0xf5;
		break;
	case 0x804:
		e->frame = 0;
		e->unk6 = 0x803;
		path_place(e, tops_path, e->path, 1.0f);
		break;
	case 0x803:
	case 0x806:
		e->frame = 0x146;
		path_place(e, tops_path, e->path, 1.0f);
		break;
	case 7:
		e->frame = 0x146;
		break;
	case 0x8801:
		e->frame = 0;
		path_place(e, tops_path, e->path, 1.0f);
		break;
	case 2:
		e->frame = 0x159;
		break;
	case 0x805:
		e->frame = 0x104;
		break;
	case 0:			/* the init's first state */
	default:
		e->frame = 0;
		break;
	}
	if(e->flags2 & 0x100)
	{
		e->unk98 = e->y - 35.0f;
	}
	else
	{
		e->unk98 = e->y;
	}
}

/* 0x80118104: at the player: a new target if its own is out, a goal near the camera (closer
   with a knife), a step; still walking: 7 (walk) or, when its run (2) ends, the run again; there:
   a knife attack (0x201-0x204) or a punch (0x208-0x20a, 0x204), the two-handed 0x204 when
   group 10 is gone */
void tops_go_for_player(enemy_t *e)
{
	player_stats_t	*p;
	int	r;

	if(e->target->state != 4)
	{
		p = get_target_player();
		if(p->state == 4)
		{
			e->target = p;
		}
	}
	if(e->flags2 & 0x80)
	{
		set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0.0f, -20.0f);
	}
	else
	{
		set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0.0f, -16.0f);
	}
	switch(step_toward_goal_xz(e, 0.6f))
	{
	case 1:
		if(e->ai_state == 2)
		{
			e->frame = 0x159;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_tops, 0x11, 0x159, 5);
		e->ai_state = 2;
		break;
	case 2:
		if(e->frame > 0)
		{
			e->frame--;
		}
		r = randrng(100);
		if(!e->groups[10].flags)
		{
			blend_to_frame(e, submesh_info_tops, 0x11, 0x19f, 5);
			e->ai_state = 0x204;
		}
		else if(e->flags2 & 0x80)
		{
			if(r < 25)
			{
				blend_to_frame(e, submesh_info_tops, 0x11, 0x177, 5);
				e->ai_state = 0x201;
			}
			else if(r < 50)
			{
				blend_to_frame(e, submesh_info_tops, 0x11, 0x18b, 5);
				e->ai_state = 0x202;
			}
			else if(r < 75)
			{
				blend_to_frame(e, submesh_info_tops, 0x11, 0x1b1, 5);
				e->ai_state = 0x203;
			}
			else
			{
				blend_to_frame(e, submesh_info_tops, 0x11, 0x19f, 5);
				e->ai_state = 0x204;
			}
		}
		else
		{
			if(r < 25)
			{
				blend_to_frame(e, submesh_info_tops, 0x11, 0x1c2, 5);
				e->ai_state = 0x208;
			}
			else if(r < 50)
			{
				blend_to_frame(e, submesh_info_tops, 0x11, 0x1cc, 5);
				e->ai_state = 0x209;
			}
			else if(r < 75)
			{
				blend_to_frame(e, submesh_info_tops, 0x11, 0x1d6, 5);
				e->ai_state = 0x20a;
				e->counter = 0;
			}
			else
			{
				blend_to_frame(e, submesh_info_tops, 0x11, 0x19f, 5);
				e->ai_state = 0x204;
			}
		}
		break;
	case 0:
		if(e->ai_state != 7)
		{
			if(e->frame > 0)
			{
				e->frame--;
			}
			blend_to_frame(e, submesh_info_tops, 0x11, 0x146, 5);
			e->ai_state = 7;
		}
		break;
	}
}

/* 0x80118454: the end of a state: its queued next state (unk6), else at the player */
void tops_next_state(enemy_t *e)
{
	switch((unsigned short)e->unk6)
	{
	case 0x803:
		tops_setup(e, 0x803);
		e->unk6 = 0;
		break;
	case 0x804:
		tops_setup(e, 0x804);
		e->unk6 = 0x803;
		break;
	case 0x206:
		tops_setup(e, 0x206);
		e->unk6 = 0;
		break;
	case 0x8801:
		tops_setup(e, 0x8801);
		e->unk6 = 0;
		break;
	case 0x207:
		tops_setup(e, 0x206);
		e->unk6 = 8;
		break;
	case 0x20c:
		tops_setup(e, 0x206);
		e->unk6 = 3;
		break;
	case 8:
		e->unk6 = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_tops, 0x11, 0x159, 2);
		e->ai_state = 8;
		break;
	case 3:
		e->unk6 = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_tops, 0x11, 0x159, 2);
		e->ai_state = 3;
		break;
	default:
		e->unk6 = 0;
		tops_go_for_player(e);
		break;
	}
}

/* 0x801185ec: groups 4, 3 and 8 knocked loose, a spray on group 0 */
void tops_drop_left_arm(enemy_t *e)
{
	attach_effect(e, attach_spray(tops_spray4b, tops_spray4a, e->groups));
	detach_group(&e->groups[4], e->groups[3].pos, e->unk98, e->scale);
	detach_group(&e->groups[3], e->groups[3].pos, e->unk98, e->scale);
	detach_group(&e->groups[8], e->groups[3].pos, e->unk98, e->scale);
}

/* 0x80118688: groups 20, 5 and 6 knocked loose, a spray on group 0 */
void tops_drop_right_arm(enemy_t *e)
{
	attach_effect(e, attach_spray(tops_spray3b, tops_spray3a, e->groups));
	detach_group(&e->groups[20], e->groups[5].pos, e->unk98, e->scale);
	detach_group(&e->groups[5], e->groups[5].pos, e->unk98, e->scale);
	detach_group(&e->groups[6], e->groups[5].pos, e->unk98, e->scale);
}
