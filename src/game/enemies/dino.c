/*
 * dino.c - the Rickety Town dinosaur (object type 0x2040, assets DINO.ZM / dino.za / dino.pth
 * and the dino*.WMS texmaps). One original file, 0x80124c68-0x80126410, tables at 0x80199d80
 * (texmaps) and 0x8019a4a0 (submeshes, dino_data.c). The model has 860 vertices, 914 normals and
 * 1405 faces in 21 submeshes, the animation 592 frames, the path 522 points.
 *
 * Groups 17 to 20 are stumps, hidden until a part is shot off: the second hit on a part swaps
 * the stump in for it (its animation is the part's own, see load_dino) and the parts below
 * fly off. Groups 8 and 9 are the head (the original's own message calls it "Zomb head"),
 * 1 to 3 and 4 to 6 the two limbs that come off, 7 the part whose stump is 17.
 *
 * States: 0 idle, 0x8801 walking its path, 1 walking at the player, 2/0x208 stopping and
 * biting, 3 waiting, 0x201-0x203 the three attacks, 4-7 the scripted moves the spawn table
 * can ask for, 0x8803 a pause on the path, 0x1001-0x1006 hit reactions, 0x2001/0x2002 dying
 * (0x2002 after the acid gun) and then 0x4000 dead. Bit 0x100 of a requested state keeps it
 * moving along its path whatever it does (flags2 0x100).
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

extern void	draw_effects(enemy_t *);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);	/* text at a world position */
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern int	randrng(int);
extern void	attach_effect(enemy_t *, void *);
extern void	*attach_flame(float, float, group_t *);
extern void	*attach_spray(float *, float *, group_t *);
extern void	path_place(enemy_t *, void *, short, float);
extern void	path_follow(enemy_t *, void *, int, float);
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern int	face_camera(enemy_t *, float);
extern int	group_near_camera(group_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal_xz(enemy_t *, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);		/* the original passes a third word the routine ignores */
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);
extern void	add_smoke(float *, unsigned int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern void	detach_group(group_t *, float *, float, float *);	/* model.c: a part flies off */
extern void	replace_group(enemy_t *, group_t *, group_t *);	/* a group takes over another's place */
extern int	mark_die();
extern int	remove_die();
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern int	printf(const char *, ...);
extern enemy_t	*debug_enemy;
extern char	text_buffer[];
extern int	violence_level;

extern texmap_t	dino_texmap[57];		/* dino_data.c */
extern submesh_t submesh_info_dino[21];

float	*dino_verts;			/* 0x801e5940 */
face_t	*dino_faces;			/* 0x80212e7c */
float	*dino_normals;			/* 0x801e439c */
void	*dino_path;			/* 0x802372a4 */

/* 0x8019a740: where the blood sprays from a shot-off part (a base and a direction each) */
float	dino_head_spray_dir[3] = { 0.075f, 0.075f, 0.0f };		/* 0x8019a740 */
float	dino_head_spray_base[3] = { 0.475f, 0.05f, 0.225f };		/* 0x8019a74c */
float	dino_limb4_spray_dir[3] = { 0.075f, 0.0f, 0.0f };		/* 0x8019a758 */
float	dino_limb4_spray_base[3] = { 0.325f, 0.325f, 0.0f };		/* 0x8019a764 */
float	dino_limb1_spray_dir[3] = { -0.075f, 0.0f, 0.0f };		/* 0x8019a770 */
float	dino_limb1_spray_base[3] = { 0.325f, 0.375f, 0.0f };		/* 0x8019a77c */

#define DINO_GROUPS	0x15
#define DINO_NVERTS	0x35c
#define DINO_NFACES	0x57d
#define DINO_NNORMALS	0x392
#define DINO_FRAMES	0x250
#define DINO_PATH_LEN	0x20a
#define DINO_HURT	0xf0		/* the damage of a bite or a swipe */

/* the states */
#define DINO_IDLE	0
#define DINO_HUNT	1		/* walks at the player */
#define DINO_STAND	2
#define DINO_WAIT	3
#define DINO_MOVE4	4		/* scripted moves from the spawn table */
#define DINO_MOVE5	5
#define DINO_MOVE6	6
#define DINO_MOVE7	7
#define DINO_ATTACK1	0x201
#define DINO_ATTACK2	0x202
#define DINO_ATTACK3	0x203
#define DINO_BITE	0x208		/* bites again and again */
#define DINO_HIT1	0x1001
#define DINO_HIT2	0x1002
#define DINO_HIT3	0x1003
#define DINO_HIT4	0x1004
#define DINO_HIT5	0x1005
#define DINO_HIT6	0x1006
#define DINO_DIE	0x2001
#define DINO_DIE_ACID	0x2002
#define DINO_DEAD	0x4000
#define DINO_PATH	0x8801		/* walks its path */
#define DINO_PATH_PAUSE	0x8803
#define DINO_ON_PATH	0x100		/* a requested state's bit: keep following the path */

int	dino_die(game_object_t *, player_stats_t *);
void	dino_remove(game_object_t *);
void	dino_setup(enemy_t *, unsigned short);
void	dino_steer(enemy_t *);
void	dino_end_path(enemy_t *);
void	dino_drop_limb1(enemy_t *);
void	dino_drop_limb4(enemy_t *);

/* 0x80124c68: the draw callback */
void dino_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_dino, DINO_GROUPS, dino_verts, DINO_NVERTS, dino_faces, DINO_NFACES, dino_normals, DINO_NNORMALS);
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

/* 0x80124e64: the hit test: the bounding sphere, then the submesh */
int dino_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_dino, DINO_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
			p->hit_group = group;
			p->die = (void *)dino_die;
		}
	}
	return(0);
}

/* 0x80124f20: the shot lands: blood, a cry, the acid gun's smoke or the flame thrower's fire;
   at the top violence level the part hit turns gory and a second hit shoots it off (the head
   in two steps); then the damage, points, death or a hit reaction by the part hit */
int dino_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;
	int	group;
	int	acid;
	int	i;

	e = (enemy_t *)obj->data;
	acid = 0;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return(0);
	}
	burst_blood(p->hit_point, e->unk98, 0x800000);
	add_chunks(e->hit_group->pos, e->unk98);
	add_world_sprite(p->hit_point, 0, 0);
	play_enemy_sound(e, randrng(13) + 1, "carny", 2);
	group = p->hit_group;
	g = &e->groups[group];
	if(p->shot_weapon == 0x1000005)
	{
		for(i = 0; i < DINO_GROUPS; i++)
		{
			e->groups[i].flags |= 0x20;
			add_smoke(e->groups[i].pivot, 0xd0ff00);
		}
		e->health = 0.01f;
		acid = 1;
	}
	if(p->shot_weapon == 0x1000004)
	{
		if(!(g->flags & 2))
		{
			g->flags |= 2;
			attach_effect(e, attach_flame(0.0f, (submesh_info_dino[group].radius + submesh_info_dino[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 8:
		case 9:
			e->unka8++;
			switch(e->unka8)
			{
			case 0:
				printf("This should never happen! Zomb head damage at 0\n");
				break;
			case 1:
				e->groups[8].flags |= 0xf0000000;
				break;
			case 2:
				replace_group(e, &e->groups[8], &e->groups[18]);
				e->groups[9].flags |= 0xf0000000;
				attach_effect(e, attach_spray(dino_head_spray_base, dino_head_spray_dir, &e->groups[18]));
				break;
			}
			break;
		case 7:
			if(g->flags & 0xf0000000)
			{
				replace_group(e, &e->groups[7], &e->groups[17]);
			}
			else
			{
				g->flags |= 0xf0000000;
			}
			break;
		case 1:
		case 2:
		case 3:
			if(g->flags & 0xf0000000)
			{
				replace_group(e, &e->groups[1], &e->groups[20]);
				dino_drop_limb1(e);
			}
			else
			{
				g->flags |= 0xf0000000;
			}
			break;
		case 4:
		case 5:
		case 6:
			if(g->flags & 0xf0000000)
			{
				replace_group(e, &e->groups[4], &e->groups[19]);
				dino_drop_limb4(e);
			}
			else
			{
				g->flags |= 0xf0000000;
			}
			break;
		case 0:
		case 13:
			e->groups[13].flags |= 0xf0000000;
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
			e->killer = p;
			mark_die(obj->id, p);
			if(acid)
			{
				e->ai_state = DINO_DIE_ACID;
				e->frame = 0x1fa;
				e->flags &= 0xfffd;
			}
			else
			{
				e->ai_state = DINO_DIE;
				e->frame = 0x197;
				e->flags &= 0xfffd;
			}
		}
		else
		{
			award_points(p->index, 50);
		}
	}
	if(e->ai_state & 0xe000)
	{
		return(0);
	}
	switch(group)
	{
	case 10:
	case 11:
	case 12:
		start_hit_reaction(e, 0x17c, DINO_HIT5, submesh_info_dino, 0xd);
		break;
	case 14:
	case 15:
	case 16:
		start_hit_reaction(e, 0x165, DINO_HIT6, submesh_info_dino, 0xd);
		break;
	case 8:
	case 18:
		if(randrng(100) < 50)
		{
			start_hit_reaction(e, 0x130, DINO_HIT3, submesh_info_dino, 0xd);
		}
		else
		{
			start_hit_reaction(e, 0x151, DINO_HIT4, submesh_info_dino, 0xd);
		}
		break;
	case 7:
	case 13:
	case 17:
		i = randrng(100);
		if(i < 50)
		{
			start_hit_reaction(e, 0xec, DINO_HIT1, submesh_info_dino, 0xd);
		}
		else
		{
			start_hit_reaction(e, 0x109, DINO_HIT2, submesh_info_dino, 0xd);
		}
		break;
	}
	return(0);
}

/* 0x80125414 */
void load_dino_texmap(void)
{
	load_texmap(dino_texmap, 0x39, "dino");
}

/* 0x80125444: the model, the animation and the path; the four stumps take the animations of
   the parts they stand in for */
void load_dino(void)
{
	load_model((void **)&dino_verts, DINO_NVERTS, (void **)&dino_faces, DINO_NFACES, (void **)&dino_normals, DINO_NNORMALS, "DINO.ZM", 0xce8f88d3);
	load_anim(submesh_info_dino, DINO_GROUPS, DINO_FRAMES, "dino.za", 0xdf1a8c94);
	load_path(&dino_path, DINO_PATH_LEN, "dino.pth", 0x7a807d08);
	free(submesh_info_dino[17].anim);
	free(submesh_info_dino[18].anim);
	free(submesh_info_dino[19].anim);
	free(submesh_info_dino[20].anim);
	submesh_info_dino[17].anim = submesh_info_dino[7].anim;
	submesh_info_dino[18].anim = submesh_info_dino[8].anim;
	submesh_info_dino[19].anim = submesh_info_dino[4].anim;
	submesh_info_dino[20].anim = submesh_info_dino[1].anim;
}

/* 0x8012556c: the stumps' borrowed animations are dropped first so they are not freed twice */
void remove_dino_assets(void)
{
	submesh_info_dino[17].anim = 0;
	submesh_info_dino[18].anim = 0;
	submesh_info_dino[19].anim = 0;
	submesh_info_dino[20].anim = 0;
	remove_surface(dino_texmap, 0x39);
	remove_path(&dino_path, DINO_PATH_LEN, "dino.pth");
	remove_animation(submesh_info_dino, DINO_GROUPS, DINO_FRAMES, "dino.za");
	remove_model((void **)&dino_verts, DINO_NVERTS, (void **)&dino_faces, DINO_NFACES, (void **)&dino_normals, DINO_NNORMALS, "DINO.ZM");
}

/* 0x8012562c: the init callback: the record, 21 groups with the four stumps hidden, 7.5
   health by difficulty, idle */
int dino_init(game_object_t *obj)
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
	obj->hit = dino_hit;
	obj->die = dino_die;
	obj->remove = dino_remove;
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
	e->x = 0;
	e->y = 0;
	e->z = 0;
	e->ry = 3.1415927f;
	e->unkcc = 1.94f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 7.5f);
	e->groups = malloc(DINO_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < DINO_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[17].flags = 0;
	e->groups[18].flags = 0;
	e->groups[19].flags = 0;
	e->groups[20].flags = 0;
	e->unka8 = 0;
	init_groups(e, submesh_info_dino, DINO_GROUPS);
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
	dino_setup(e, 0);
	return(0);
}

/* 0x801257cc: the remove callback */
void dino_remove(game_object_t *obj)
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

/* 0x80125824: the think callback */
void dino_think(game_object_t *obj)
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
	animate_loose_groups(e, DINO_GROUPS);
	if(e->flags2 & DINO_ON_PATH)
	{
		path_follow(e, dino_path, e->path, 1.0f);
		if(e->path >= e->unk92)
		{
			remove_die(obj->id, 0);
		}
	}
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case DINO_HIT3:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x151)
		{
			break;
		}
		e->frame = frame;
		blend_to_frame(e, submesh_info_dino, 0xd, e->resume_frame, 2);
		e->ai_state = e->resume_state;
		break;
	case DINO_HIT4:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x165)
		{
			break;
		}
		e->frame = frame;
		blend_to_frame(e, submesh_info_dino, 0xd, e->resume_frame, 2);
		e->ai_state = e->resume_state;
		break;
	case DINO_HIT1:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x109)
		{
			break;
		}
		e->frame = frame;
		blend_to_frame(e, submesh_info_dino, 0xd, e->resume_frame, 2);
		e->ai_state = e->resume_state;
		break;
	case DINO_HIT2:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x130)
		{
			break;
		}
		e->frame = frame;
		blend_to_frame(e, submesh_info_dino, 0xd, e->resume_frame, 2);
		e->ai_state = e->resume_state;
		break;
	case DINO_HIT5:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x197)
		{
			break;
		}
		e->frame = frame;
		blend_to_frame(e, submesh_info_dino, 0xd, e->resume_frame, 2);
		e->ai_state = e->resume_state;
		break;
	case DINO_HIT6:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x17c)
		{
			break;
		}
		e->frame = frame;
		blend_to_frame(e, submesh_info_dino, 0xd, e->resume_frame, 2);
		e->ai_state = e->resume_state;
		break;
	case DINO_PATH:
		e->frame++;
		if(e->frame >= 0x1c5)
		{
			e->frame = 0x1aa;
		}
		e->path++;
		path_place(e, dino_path, e->path, 1.0f);
		if(e->path < e->unk92)
		{
			break;
		}
		dino_end_path(e);
		break;
	case DINO_HUNT:
		e->frame++;
		if(e->frame >= 0x1c5)
		{
			e->frame = 0x1aa;
		}
		face_camera(e, 0.2f);
		dino_steer(e);
		break;
	case DINO_BITE:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame == 0x25)
		{
			if(group_near_camera(&e->groups[8], 20.0f))
			{
				hurt_player(e->target, DINO_HURT, 3, 0.0f);
			}
		}
		if(e->frame >= 0x33)
		{
			e->frame = 0x1c;
		}
		break;
	case DINO_ATTACK1:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame == 0x25)
		{
			if(group_near_camera(&e->groups[8], 20.0f))
			{
				hurt_player(e->target, DINO_HURT, 3, 0.0f);
			}
		}
		if(e->frame < 0x33)
		{
			break;
		}
		dino_steer(e);
		break;
	case DINO_ATTACK2:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame == 0xd8)
		{
			if(group_near_camera(&e->groups[0], 40.0f))
			{
				hurt_player(e->target, DINO_HURT, 3, 0.0f);
			}
		}
		if(e->frame < 0xec)
		{
			break;
		}
		dino_steer(e);
		break;
	case DINO_ATTACK3:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame == 0xb7)
		{
			if(group_near_camera(&e->groups[0], 40.0f))
			{
				hurt_player(e->target, DINO_HURT, 3, 0.0f);
			}
		}
		if(e->frame < 0xcc)
		{
			break;
		}
		dino_steer(e);
		break;
	case DINO_WAIT:
		e->frame++;
		if(e->frame < 0x250)
		{
			break;
		}
		dino_steer(e);
		break;
	case DINO_STAND:
		e->frame++;
		if(e->frame >= 0x1c)
		{
			e->frame = 0;
		}
		break;
	case DINO_MOVE4:
		e->frame++;
		if(e->frame < 0x65)
		{
			break;
		}
		dino_end_path(e);
		break;
	case DINO_MOVE5:
		e->frame++;
		if(e->frame < 0xab)
		{
			break;
		}
		dino_end_path(e);
		break;
	case DINO_MOVE6:
		e->frame++;
		if(e->frame < 0x1df)
		{
			break;
		}
		dino_end_path(e);
		break;
	case DINO_MOVE7:
		e->frame++;
		if(e->frame < 0x1fa)
		{
			break;
		}
		dino_end_path(e);
		break;
	case DINO_PATH_PAUSE:
		e->frame++;
		if(e->frame < 0x1df)
		{
			break;
		}
		e->ai_state = DINO_PATH;
		e->frame = 0x1aa;
		break;
	case DINO_DIE:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x1aa)
		{
			break;
		}
		e->frame = frame;
		e->ai_state = DINO_DEAD;
		break;
	case DINO_DIE_ACID:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x242)
		{
			break;
		}
		e->frame = frame;
		e->ai_state = DINO_DEAD;
		break;
	case DINO_IDLE:
		e->frame++;
		if(e->frame >= 0x250)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x80125eb4: the state set-up: each state's first frame; the path states start along the
   path; the path bit is kept apart in flags2 */
void dino_setup(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	if(state & DINO_ON_PATH)
	{
		e->ai_state = state & ~DINO_ON_PATH;
		e->flags2 |= DINO_ON_PATH;
	}
	switch(e->ai_state)
	{
	case DINO_ATTACK1:
		e->frame = 0x1c;
		break;
	case DINO_HUNT:
		e->frame = 0x1aa;
		break;
	case DINO_PATH:
		e->frame = 0x1aa;
		path_place(e, dino_path, e->path, 1.0f);
		break;
	case DINO_PATH_PAUSE:
		e->frame = 0x1c5;
		path_place(e, dino_path, e->path, 1.0f);
		break;
	case DINO_MOVE4:
		e->frame = 0x33;
		break;
	case DINO_MOVE5:
		e->frame = 0x65;
		break;
	case DINO_MOVE6:
		e->frame = 0x1c5;
		break;
	case DINO_MOVE7:
		e->frame = 0x1df;
		break;
	case DINO_IDLE:
	default:
		e->frame = 0;
		break;
	}
}

/* 0x80125fe4: off the path: keep a playing target, walk at a point 28 short of the camera
   and pick the next state from the steering result (1 wait, 2 one of the three attacks, 0
   walk) */
void dino_steer(enemy_t *e)
{
	player_stats_t *p;
	int	r, n;

	if(e->target->state != 4)
	{
		p = get_target_player();
		if(p->state == 4)
		{
			e->target = p;
		}
	}
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0, -28.0f);
	r = step_toward_goal_xz(e, 0.8f);
	switch(r)
	{
	case 1:
		if(e->ai_state == DINO_WAIT)
		{
			e->frame = 0x242;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_dino, 0xd, 0x242, 2);
		e->ai_state = DINO_WAIT;
		break;
	case 2:
		if(e->frame > 0)
		{
			e->frame--;
		}
		n = randrng(100);
		if(n < 35)
		{
			blend_to_frame(e, submesh_info_dino, 0xd, 0x1c, r);
			e->ai_state = DINO_ATTACK1;
		}
		else if(n < 65)
		{
			blend_to_frame(e, submesh_info_dino, 0xd, 0xcc, r);
			e->ai_state = DINO_ATTACK2;
		}
		else
		{
			blend_to_frame(e, submesh_info_dino, 0xd, 0xab, r);
			e->ai_state = DINO_ATTACK3;
		}
		break;
	case 0:
		if(e->ai_state == DINO_HUNT)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_dino, 0xd, 0x1aa, 2);
		e->ai_state = DINO_HUNT;
		break;
	}
}

/* 0x801261dc: a path or a scripted move ran out: a spawn entry marked 2 stands, one marked
   0x208 bites, the rest hunt */
void dino_end_path(enemy_t *e)
{
	switch((unsigned short)e->unk6)
	{
	case DINO_BITE:
		if(e->frame > 0)
		{
			e->frame--;
		}
		e->unk6 = 0;
		blend_to_frame(e, submesh_info_dino, 0xd, 0x1c, 2);
		e->ai_state = DINO_BITE;
		break;
	case 2:
		if(e->frame > 0)
		{
			e->frame--;
		}
		e->unk6 = 0;
		blend_to_frame(e, submesh_info_dino, 0xd, 0, 2);
		e->ai_state = DINO_STAND;
		break;
	default:
		e->unk6 = 0;
		dino_steer(e);
		break;
	}
}

/* 0x801262a0: the first limb comes off: blood sprays, groups 3 and 2 fly from group 2 */
void dino_drop_limb1(enemy_t *e)
{
	attach_effect(e, attach_spray(dino_limb1_spray_base, dino_limb1_spray_dir, &e->groups[17]));
	attach_effect(e, attach_spray(dino_limb1_spray_base, dino_limb1_spray_dir, &e->groups[7]));
	detach_group(&e->groups[3], e->groups[2].pos, e->y, e->scale);
	detach_group(&e->groups[2], e->groups[2].pos, e->y, e->scale);
}

/* 0x80126358: the second limb comes off: groups 6 and 5 fly from group 5 */
void dino_drop_limb4(enemy_t *e)
{
	attach_effect(e, attach_spray(dino_limb4_spray_base, dino_limb4_spray_dir, &e->groups[17]));
	attach_effect(e, attach_spray(dino_limb4_spray_base, dino_limb4_spray_dir, &e->groups[7]));
	detach_group(&e->groups[6], e->groups[5].pos, e->y, e->scale);
	detach_group(&e->groups[5], e->groups[5].pos, e->y, e->scale);
}
