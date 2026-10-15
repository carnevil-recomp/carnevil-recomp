/*
 * rodz.c - the Freak Show's "rodz" (object type 0x2026, assets RODZ.ZM / rodz.pth, the rodz*.WMS
 * texmaps and the "rodz" sound bank). One original file, 0x8011dd80-0x8011f4e0, its tables at
 * 0x80196ea0 (texmaps) and 0x80197580 (submeshes, rodz_data.c). The model has 0x354 vertices,
 * 0x5da faces and 0x36b normals in 22 submeshes. It has no animation file of its own: it is
 * built on the tort's skeleton and load_rodz points every submesh at one of the tort's
 * animations (tort.c's load_tort has to run first).
 *
 * A humanoid freak (screws, cuffs and metal in its texmaps) that walks at the player and fights
 * with its hands like the tort, the right hand sometimes holding a sword (spawn bit 0x100) that
 * it can throw (0x203). Shot it plays a hurt cue, takes the damage and reacts by the part hit; at
 * the highest violence six parts are swapped for damaged ones and the rest turn gory. Dead it
 * falls (0x2001), 500 points.
 *
 * The code at 0x8011f4e0-0x8011fa7c (object type 0x202a) is not part of this file: it is Junior's
 * puke (puke2/puke_o/puke_g.WMS, in junior.c), whose tables follow the junior file's and which
 * junior's think releases.
 */
#include <stdlib.h>
#include <glide/glide.h>
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "texmap.h"
#include "credits.h"			/* debug_mode, player_stats_t */
#include "levelsnd.h"
#include "camera.h"			/* camera_x */

/* weapons.h is not included: it brings effects.h, whose burst_blood takes two arguments, and
   this file passes the original's third (0x800000), as teen.c does */
#define WEAPON_SWORD	0x10			/* weapons.h: Freak Show SWRD.ZM */

extern void	draw_effects(enemy_t *);
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	attach_effect(enemy_t *, void *);
extern void	*attach_flame(float, float, group_t *);
extern void	detach_effect(enemy_t *, group_t *);
extern void	*attach_held_object(group_t *, int, float, float, float, float, float, float, float *);
extern void	throw_at_camera(float *, int, float *);
extern void	replace_group(enemy_t *, group_t *, group_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern int	randrng(int);
extern int	face_camera(enemy_t *, float);
extern int	group_near_camera(group_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal_xz(enemy_t *, float);
extern void	path_place(enemy_t *, void *, short, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern int	mark_die();
extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern void	remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern enemy_t	*debug_enemy;
extern char	text_buffer[];
extern int	violence_level;

extern texmap_t	rodz_texmap[55];		/* rodz_data.c */
extern submesh_t submesh_info_rodz[22];
extern int	rodz_hits;
extern submesh_t submesh_info_tort[21];		/* tort_data.c: the animations it borrows */

float	*rodz_verts;			/* 0x801e5bd0 */
face_t	*rodz_faces;			/* 0x8022b188 */
float	*rodz_normals;			/* 0x801e5944 */
void	*rodz_path;			/* 0x80237410 */

#define RODZ_GROUPS	22
#define RODZ_NVERTS	0x354
#define RODZ_NFACES	0x5da
#define RODZ_NNORMALS	0x36b
#define RODZ_FRAMES	0x231		/* the tort's animation */
#define RODZ_PATH_LEN	0x14a
#define RODZ_HAND	6		/* the right hand: the held sword and most blows */
#define RODZ_HIT	11		/* the submesh it is hit through and measured by */
#define RODZ_LHAND	13		/* the left hand */
#define RODZ_REACH	15.0f		/* a blow lands when the hand is this near the player */
#define RODZ_BLOW	0x104		/* the damage of a blow */
#define HURT_STAB	3		/* hurt_player kinds: the plain mark ... */
#define HURT_SLASH	7		/* ... and the slash, mirrored when param is set */

/* the states */
#define RODZ_LOOP	0		/* plays the whole animation round */
#define RODZ_RUN	1		/* frames 0x32-0x81, steering at the player */
#define RODZ_STAND	2		/* frames 0x19-0x31 on the spot */
#define RODZ_APPROACH	3		/* frames 0x19-0x31, steering at the player */
#define RODZ_STAND4	4		/* as 2 */
#define RODZ_LOOP5	5		/* loops 0x218-0x230 */
#define RODZ_STRIKE	0x201		/* both hands, 0x171-0x189 (a hit at 0x17b) */
#define RODZ_STAB	0x202		/* 0x1ec-0x1f7 (a hit at 0x1fa) */
#define RODZ_THROW	0x203		/* throws its sword at 0x20e, 0x202-0x217 */
#define RODZ_SWIPE	0x204		/* 0-0x18 (a slash at 9) */
#define RODZ_COMBO	0x205		/* 0x127-0x152, four slashes */
#define RODZ_COMBO2	0x206		/* 0x1a3-0x1bb, a slash with each hand */
#define RODZ_PATH	0x801		/* runs its path (frames 0x32-0x81) */
#define RODZ_DIE	0x2001		/* falls, 0xde-0xf7 */
#define RODZ_GONE	0x4000

int	rodz_die(game_object_t *, player_stats_t *);
void	rodz_remove(game_object_t *);
void	rodz_setup(enemy_t *, int);
void	rodz_approach(enemy_t *);
void	rodz_end_run(enemy_t *);

/* 0x8011dd80: the draw callback */
void rodz_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(0x20);
	draw_model(e, submesh_info_rodz, RODZ_GROUPS, rodz_verts, RODZ_NVERTS, rodz_faces, RODZ_NFACES, rodz_normals, RODZ_NNORMALS);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
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

/* 0x8011df8c: the hit test: the bounding sphere, then the submesh */
int rodz_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_rodz, RODZ_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)rodz_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x8011e048: the shot lands: blood; the flame thrower sets the part alight; at the highest
   violence parts 3, 4, 15, 16, 18 and 19 are swapped for their damaged versions and the rest
   turn gory; then a hurt cue and the damage (death 500 points, else 50) and a hit reaction by
   part */
int rodz_die(game_object_t *obj, player_stats_t *p)
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
	burst_blood(p->hit_point, e->y, 0x800000);
	add_chunks(e->hit_group->pos, e->y);
	add_world_sprite(p->hit_point, 0, 0);
	group = p->hit_group;
	g = &e->groups[group];
	if(p->shot_weapon == 0x1000004)
	{
		if(!(g->flags & 2))
		{
			g->flags |= 2;
			attach_effect(e, attach_flame(0.0f, (submesh_info_rodz[group].radius + submesh_info_rodz[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 15:
			replace_group(e, &e->groups[15], &e->groups[14]);
			break;
		case 19:
			replace_group(e, &e->groups[19], &e->groups[0]);
			break;
		case 3:
			replace_group(e, &e->groups[3], &e->groups[7]);
			break;
		case 18:
			replace_group(e, &e->groups[18], &e->groups[20]);
			break;
		case 4:
			replace_group(e, &e->groups[4], &e->groups[21]);
			break;
		case 16:
			replace_group(e, &e->groups[16], &e->groups[10]);
			break;
		default:
			g->flags |= 0xf0000000;
			break;
		}
	}
	if(!(e->ai_state & 0x6000))
	{
		play_enemy_sound(e, randrng(10) + 1, "rodz", 2);
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			award_points(p->index, 500);
			e->killer = p;
			mark_die(obj->id, p);
			e->ai_state = RODZ_DIE;
			e->frame = 0xde;
			e->flags &= 0xfffd;
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
	rodz_hits++;
	switch(group)
	{
	case 10:
	case 16:
		start_hit_reaction(e, 0xf8, 0x1007, submesh_info_rodz, 0xb);
		break;
	case 11:
	case 12:
	case 14:
	case 15:
		if(rodz_hits & 1)
		{
			start_hit_reaction(e, 0x1bc, 0x1001, submesh_info_rodz, 0xb);
		}
		else
		{
			start_hit_reaction(e, 0x1cf, 0x1002, submesh_info_rodz, 0xb);
		}
		break;
	case 0:
	case 1:
	case 2:
	case 13:
	case 17:
	case 18:
	case 19:
	case 20:
		if(rodz_hits & 1)
		{
			start_hit_reaction(e, 0xb4, 0x1003, submesh_info_rodz, 0xb);
		}
		else
		{
			start_hit_reaction(e, 0xc9, 0x1004, submesh_info_rodz, 0xb);
		}
		break;
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 21:
		if(rodz_hits & 1)
		{
			start_hit_reaction(e, 0x82, 0x1005, submesh_info_rodz, 0xb);
		}
		else
		{
			start_hit_reaction(e, 0x9d, 0x1006, submesh_info_rodz, 0xb);
		}
		break;
	}
	return(0);
}

/* 0x8011e408 */
void load_rodz_texmap(void)
{
	load_texmap(rodz_texmap, 0x37, "rodz");
}

/* 0x8011e438: the path and the model; every submesh takes one of the tort's animations */
void load_rodz(void)
{
	load_path(&rodz_path, RODZ_PATH_LEN, "rodz.pth", 0x1bbf0a45);
	load_model((void **)&rodz_verts, RODZ_NVERTS, (void **)&rodz_faces, RODZ_NFACES, (void **)&rodz_normals, RODZ_NNORMALS, "RODZ.ZM", (int)0x8fe3297e);
	submesh_info_rodz[0].anim = submesh_info_tort[16].anim;
	submesh_info_rodz[1].anim = submesh_info_tort[13].anim;
	submesh_info_rodz[2].anim = submesh_info_tort[10].anim;
	submesh_info_rodz[3].anim = submesh_info_tort[9].anim;
	submesh_info_rodz[4].anim = submesh_info_tort[1].anim;
	submesh_info_rodz[21].anim = submesh_info_tort[1].anim;
	submesh_info_rodz[5].anim = submesh_info_tort[2].anim;
	submesh_info_rodz[6].anim = submesh_info_tort[3].anim;
	submesh_info_rodz[7].anim = submesh_info_tort[9].anim;
	submesh_info_rodz[8].anim = submesh_info_tort[4].anim;
	submesh_info_rodz[9].anim = submesh_info_tort[5].anim;
	submesh_info_rodz[10].anim = submesh_info_tort[6].anim;
	submesh_info_rodz[11].anim = submesh_info_tort[8].anim;
	submesh_info_rodz[12].anim = submesh_info_tort[7].anim;
	submesh_info_rodz[19].anim = submesh_info_tort[16].anim;
	submesh_info_rodz[18].anim = submesh_info_tort[0].anim;
	submesh_info_rodz[20].anim = submesh_info_tort[0].anim;
	submesh_info_rodz[17].anim = submesh_info_tort[11].anim;
	submesh_info_rodz[13].anim = submesh_info_tort[12].anim;
	submesh_info_rodz[16].anim = submesh_info_tort[6].anim;
	submesh_info_rodz[14].anim = submesh_info_tort[19].anim;
	submesh_info_rodz[15].anim = submesh_info_tort[14].anim;
}

/* 0x8011e5f8: the assets go when the level is unloaded */
void remove_rodz_assets(void)
{
	remove_surface(rodz_texmap, 0x37);
	remove_path(&rodz_path, RODZ_PATH_LEN, "rodz.pth");
	remove_model((void **)&rodz_verts, RODZ_NVERTS, (void **)&rodz_faces, RODZ_NFACES, (void **)&rodz_normals, RODZ_NNORMALS, "RODZ.ZM");
}

/* 0x8011e674: the init callback: 7.5 health at 0.9 scale, 23 units to the left, the six spare
   parts (0, 7, 10, 14, 20, 21) hidden */
int rodz_init(game_object_t *obj)
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
	obj->hit = rodz_hit;
	obj->die = rodz_die;
	obj->remove = rodz_remove;
	e = (enemy_t *)data;
	e->unk4 = 0x14;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0x2c;
	e->target = get_target_player();
	e->unk54 = 0;
	e->rx = 0;
	e->rz = 0;
	e->y = 0;
	e->z = 0;
	e->ry = 3.1415927f;
	e->x = -23.0f;
	e->unkcc = 1.0f;
	set_enemy_scale(e, 0.9f);
	set_enemy_health(e, 7.5f);
	e->groups = malloc(RODZ_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < RODZ_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[0].flags = 0;
	e->groups[7].flags = 0;
	e->groups[20].flags = 0;
	e->groups[21].flags = 0;
	e->groups[10].flags = 0;
	e->groups[14].flags = 0;
	e->groups[16].flags |= 0x40;
	init_groups(e, submesh_info_rodz, RODZ_GROUPS);
	g = e->groups;
	e->hit_group = &g[RODZ_HIT];
	g[RODZ_HIT].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	rodz_setup(e, 0);
	return(0);
}

/* 0x8011e838: the remove callback */
void rodz_remove(game_object_t *obj)
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

/* a hit reaction plays to its last frame, then it blends back to what it was doing */
#define HIT_REACTION(last) \
	frame = e->frame; \
	e->frame = frame + 1; \
	if(e->frame < last) \
	{ \
		break; \
	} \
	e->frame = frame; \
	blend_to_frame(e, submesh_info_rodz, 0xb, e->resume_frame, 5); \
	e->ai_state = e->resume_state; \
	break;

/* 0x8011e890: the think callback */
void rodz_think(game_object_t *obj)
{
	enemy_t	*e;
	short	frame;

	if(debug_mode & 0x10000)
	{
		return;
	}
	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, RODZ_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1001:
		HIT_REACTION(0x1cf)
	case 0x1002:
		HIT_REACTION(0x1ec)
	case 0x1005:
		HIT_REACTION(0x9d)
	case 0x1006:
		HIT_REACTION(0xb4)
	case 0x1003:
		HIT_REACTION(0xc9)
	case 0x1004:
		HIT_REACTION(0xde)
	case 0x1007:
		HIT_REACTION(0x115)
	case RODZ_RUN:
		if(++e->frame >= 0x82)
		{
			e->frame = 0x32;
		}
		face_camera(e, 0.2f);
		rodz_approach(e);
		break;
	case RODZ_SWIPE:
		face_camera(e, 0.2f);
		if(++e->frame == 9 && group_near_camera(&e->groups[RODZ_HAND], RODZ_REACH))
		{
			hurt_player(e->target, RODZ_BLOW, HURT_SLASH, 3.1415927f);
		}
		if(e->frame < 0x19)
		{
			break;
		}
		rodz_approach(e);
		break;
	case RODZ_COMBO:
		face_camera(e, 0.2f);
		if(++e->frame == 0x134 && group_near_camera(&e->groups[RODZ_HAND], RODZ_REACH))
		{
			hurt_player(e->target, RODZ_BLOW, HURT_SLASH, 0.0f);
		}
		if(e->frame == 0x13c && group_near_camera(&e->groups[RODZ_HAND], RODZ_REACH))
		{
			hurt_player(e->target, RODZ_BLOW, HURT_SLASH, 3.1415927f);
		}
		if(e->frame == 0x146 && group_near_camera(&e->groups[RODZ_HAND], RODZ_REACH))
		{
			hurt_player(e->target, RODZ_BLOW, HURT_SLASH, 0.0f);
		}
		if(e->frame == 0x14d && group_near_camera(&e->groups[RODZ_HAND], RODZ_REACH))
		{
			hurt_player(e->target, RODZ_BLOW, HURT_SLASH, 3.1415927f);
		}
		if(e->frame < 0x153)
		{
			break;
		}
		rodz_approach(e);
		break;
	case RODZ_COMBO2:
		face_camera(e, 0.2f);
		if(++e->frame == 0x1ae && group_near_camera(&e->groups[RODZ_HAND], RODZ_REACH))
		{
			hurt_player(e->target, RODZ_BLOW, HURT_SLASH, 3.1415927f);
		}
		if(e->frame == 0x1b8 && group_near_camera(&e->groups[RODZ_LHAND], RODZ_REACH))
		{
			hurt_player(e->target, RODZ_BLOW, HURT_SLASH, 0.0f);
		}
		if(e->frame < 0x1bc)
		{
			break;
		}
		rodz_approach(e);
		break;
	case RODZ_STRIKE:
		face_camera(e, 0.2f);
		if(++e->frame == 0x17b)
		{
			if(group_near_camera(&e->groups[15], RODZ_REACH) || group_near_camera(&e->groups[14], RODZ_REACH))
			{
				hurt_player(e->target, RODZ_BLOW, HURT_STAB, 0.0f);
			}
		}
		if(e->frame < 0x18a)
		{
			break;
		}
		rodz_approach(e);
		break;
	case RODZ_STAB:
		face_camera(e, 0.2f);
		if(++e->frame == 0x1fa && group_near_camera(&e->groups[RODZ_HAND], RODZ_REACH))
		{
			hurt_player(e->target, RODZ_BLOW, HURT_STAB, 0.0f);
		}
		if(e->frame < 0x1f8)
		{
			break;
		}
		rodz_end_run(e);
		break;
	case RODZ_APPROACH:
		if(++e->frame >= 0x32)
		{
			e->frame = 0x19;
		}
		face_camera(e, 0.2f);
		rodz_approach(e);
		break;
	case RODZ_THROW:
		face_camera(e, 0.2f);
		if(++e->frame == 0x20e && (e->groups[RODZ_HAND].flags & 0x2000))
		{
			throw_at_camera(e->groups[RODZ_HAND].pos, WEAPON_SWORD, e->scale);
			detach_effect(e, &e->groups[RODZ_HAND]);
		}
		if(e->frame < 0x218)
		{
			break;
		}
		rodz_end_run(e);
		break;
	case RODZ_PATH:
		if(++e->frame >= 0x82)
		{
			e->frame = 0x32;
		}
		path_place(e, rodz_path, e->path, 1.0f);
		if(++e->path <= e->unk92)
		{
			break;
		}
		rodz_end_run(e);
		break;
	case RODZ_DIE:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0xf8)
		{
			break;
		}
		e->frame = frame;
		e->ai_state = RODZ_GONE;
		break;
	case RODZ_LOOP:
		if(++e->frame >= RODZ_FRAMES)
		{
			e->frame = 0;
		}
		break;
	case RODZ_LOOP5:
		if(++e->frame >= RODZ_FRAMES)
		{
			e->frame = 0x218;
		}
		break;
	case RODZ_STAND:
	case RODZ_STAND4:
		if(++e->frame >= 0x32)
		{
			e->frame = 0x19;
		}
		break;
	}
}

/* 0x8011f070: the set-up: the state bit 0x100 (a sword in the right hand) goes into flags2,
   then the state's first frame */
void rodz_setup(enemy_t *e, int state)
{
	e->ai_state = state;
	if(state & 0x100)
	{
		e->ai_state = state & 0xfeff;
		e->flags2 |= 0x100;
		detach_effect(e, &e->groups[RODZ_HAND]);
		attach_effect(e, attach_held_object(&e->groups[RODZ_HAND], WEAPON_SWORD, 0.14f, 0.03f, 0.0f, -1.7f, -3.0f, 0.0f, e->scale));
	}
	switch(e->ai_state)
	{
	case RODZ_THROW:
		e->frame = 0x202;
		break;
	case RODZ_PATH:
		e->frame = 0x32;
		path_place(e, rodz_path, e->path, 1.0f);
		break;
	case RODZ_STAND4:
		e->frame = 0x19;
		break;
	case RODZ_LOOP5:
		e->frame = 0x218;
		break;
	case RODZ_STAB:
		e->frame = 0x1ec;
		break;
	case RODZ_RUN:
		e->frame = 0x32;
		break;
	case RODZ_LOOP:
	default:
		e->frame = 0;
		break;
	}
}

/* 0x8011f1dc: go for the player: keep a playing target, steer to a point in front of the
   camera and pick the state by how close it got (0 still far: run, 1 there: approach on the
   spot, 2 in reach: a swipe, combo, two-handed combo or strike) */
void rodz_approach(enemy_t *e)
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
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0.0f, -12.0f);
	r = step_toward_goal_xz(e, 0.6f);
	switch(r)
	{
	case 1:
		if(e->ai_state == RODZ_APPROACH)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_rodz, 0xb, 0x19, 5);
		e->ai_state = RODZ_APPROACH;
		break;
	case 2:
		if(e->frame > 0)
		{
			e->frame--;
		}
		r = randrng(100);
		if(r < 25)
		{
			blend_to_frame(e, submesh_info_rodz, 0xb, 0, 5);
			e->ai_state = RODZ_SWIPE;
		}
		else if(r < 50)
		{
			blend_to_frame(e, submesh_info_rodz, 0xb, 0x127, 5);
			e->ai_state = RODZ_COMBO;
		}
		else if(r < 75)
		{
			blend_to_frame(e, submesh_info_rodz, 0xb, 0x1a3, 5);
			e->ai_state = RODZ_COMBO2;
			e->counter = 0;
		}
		else
		{
			blend_to_frame(e, submesh_info_rodz, 0xb, 0x171, 5);
			e->ai_state = RODZ_STRIKE;
		}
		break;
	case 0:
		if(e->ai_state == RODZ_RUN)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_rodz, 0xb, 0x32, 5);
		e->ai_state = RODZ_RUN;
		break;
	}
}

/* 0x8011f410: at the end of a run, a stab or a throw: the spawn entry's 2 makes it stand,
   0x10 makes it throw its sword first (then stand), else it goes for the player again */
void rodz_end_run(enemy_t *e)
{
	switch((unsigned short)e->unk6)
	{
	case 0x10:
		e->unk6 = 2;
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_rodz, 0xb, 0x202, 5);
		e->ai_state = RODZ_THROW;
		break;
	case 2:
		e->unk6 = 0;
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_rodz, 0xb, 0x19, 5);
		e->ai_state = RODZ_STAND;
		break;
	default:
		e->unk6 = 0;
		rodz_approach(e);
		break;
	}
}
