/*
 * clown.c - the Big Top clown (object type 0x2063, assets CLWN.ZM / clwn.za / clwn.pth and the
 * clwn*.WMS texmaps: face, hair, frills, shoes, a mallet; its calls are in the "smeek" bank). One
 * original file, 0x801348e0-0x80136840, its tables at 0x8019f590 (clown_data.c), rodata
 * 0x80174690-0x801747b4. The model has 817 vertices, 1457 faces and 889 normals in 21 submeshes,
 * the animation 685 frames, the path 153 points. Five damaged parts (16-20) share the animation
 * of the whole ones (10, 1, 5, 13, 0) and are hidden until a shot swaps them in.
 *
 * It walks its path (0x8801 looping it, 0x8803 once, 0x802 holding first), comes down from above
 * (0x8006), taunts (0x8404, 0x8005), and goes for the player: it walks to 28 units in front of the
 * camera (3) and strikes in reach: the plain clown with one of three blows (0x201-0x203), the
 * spawn entry's 0x80 variant with three combinations (0x206-0x208, hand parts 8 and 15), the 0x100
 * variant hops (0x105) and lunges (0x304); 0x205 throws its mallet (part 9) at the camera. A blow
 * lands for 310 (or 260 for the combinations) when the hand is within 35 units. It has 7.5
 * health: a shot plays a hurt call and a reaction by part; the flame thrower sets the part alight;
 * at the highest violence the head turns gory, then is swapped with a spray, and limbs are swapped
 * for their damaged versions; dead (500 points) it falls (0x2001 for the 0x100 variant, else 0x2002).
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
extern int	group_near_camera(group_t *, float);
extern int	face_camera(enemy_t *, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal_xz(enemy_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	throw_at_camera(float *, int, float *);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern void	attach_effect(enemy_t *, void *);
extern void	*attach_flame(float, float, group_t *);
extern void	*attach_spray(float *, float *, group_t *);
extern void	replace_group(enemy_t *, group_t *, group_t *);
extern void	detach_group(group_t *, float *, float, float *);
extern int	mark_die();
extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern int	violence_level;
extern char	text_buffer[];
extern enemy_t	*debug_enemy;

extern texmap_t	clown_texmap[50];		/* clown_data.c */
extern submesh_t submesh_info_clown[21];
extern float	clown_spray_dir[3], clown_spray_base[3];
extern int	clown_taunt_sounds[12], clown_hurt_sounds[12], clown_death_sounds[12];

float	*clown_verts;			/* 0x801e5b74 */
face_t	*clown_faces;			/* 0x8021f088 */
float	*clown_normals;			/* 0x801e5824 */
void	*clown_path;			/* 0x802373c4 */

#define WEAPON_UNK33	0x33			/* weapons.h: the model at 0x8019fcf0, the clown's mallet (its part 9) */
#define CLOWN_GROUPS	0x15
#define CLOWN_NVERTS	0x331
#define CLOWN_NFACES	0x5b1
#define CLOWN_NNORMALS	0x379
#define CLOWN_FRAMES	0x2ad
#define CLOWN_PATH_LEN	0x99
#define CLOWN_HIT_GROUP	4		/* the submesh it is hit through */
#define CLOWN_HAND	8		/* the part most blows land with */
#define CLOWN_HAND2	15		/* the other hand of the combinations */
#define CLOWN_MALLET	9		/* the part 0x205 throws */
#define CLOWN_REACH	35.0f
#define CLOWN_BLOW	0x136		/* 310 damage */
#define CLOWN_BLOW2	0x104		/* 260, the combinations' */
#define CLOWN_ALT	0x80		/* the spawn entry's combination variant (the mallet hidden) */
#define CLOWN_HOPPER	0x100		/* the spawn entry's hopping variant */

/* the states */
#define CLOWN_STAND	1
#define CLOWN_HOLD	2
#define CLOWN_WALK	3
#define CLOWN_HOP_STAND	0x104
#define CLOWN_HOP	0x105
#define CLOWN_BLOW_A	0x201
#define CLOWN_BLOW_B	0x202
#define CLOWN_BLOW_C	0x203
#define CLOWN_THROW	0x205
#define CLOWN_COMBO_A	0x206
#define CLOWN_COMBO_B	0x207
#define CLOWN_COMBO_C	0x208
#define CLOWN_COMBO_D	0x209
#define CLOWN_LUNGE	0x304
#define CLOWN_PATH_WAIT	0x802
#define CLOWN_TAUNT2	0x8005
#define CLOWN_DESCEND	0x8006
#define CLOWN_TAUNT	0x8404
#define CLOWN_PATH	0x8801
#define CLOWN_PATH_ONCE	0x8803

int	clown_die(game_object_t *, player_stats_t *);
void	clown_remove(game_object_t *);
void	func_80135f98(enemy_t *, unsigned short);
void	clown_approach(enemy_t *);
void	clown_end_path(enemy_t *);

/* 0x801348e0: the draw callback (type 0x2063) */
void func_801348e0(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_clown, CLOWN_GROUPS, clown_verts, CLOWN_NVERTS, clown_faces, CLOWN_NFACES, clown_normals, CLOWN_NNORMALS);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[6].pos, e->radius, 0x600000);
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

/* 0x80134adc: the hit test: the bounding sphere, then the submesh */
int clown_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_clown, CLOWN_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)clown_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x80134b98: the shot lands: blood and chunks (above violence 0); the flame thrower sets the
   part alight; at the highest violence the head turns gory and, the next time, is swapped with a
   spray, and limbs are swapped for their damaged versions; then, alive, the damage (dead: 500
   points, a death call, the fall; else 50 points and a hurt call) and a hit reaction by part */
int clown_die(game_object_t *obj, player_stats_t *p)
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
	if(violence_level > 0)
	{
		burst_blood(p->hit_point, e->unk98, 0x800000);
		add_chunks(e->hit_group->pos, e->unk98);
	}
	add_world_sprite(p->hit_point, 0, 0);
	group = p->hit_group;
	g = &e->groups[group];
	if(p->shot_weapon == 0x1000004)
	{
		if(!(g->flags & 2))
		{
			g->flags |= 2;
			attach_effect(e, attach_flame(0.0f, (submesh_info_clown[group].radius + submesh_info_clown[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 5:
			e->unka8++;
			switch(e->unka8)
			{
			case 0:
				printf("This should never happen! Zomb head damage at 0\n");
				break;
			case 1:
				e->groups[5].flags |= 0xf0000000;
				break;
			case 2:
				replace_group(e, &e->groups[5], &e->groups[18]);
				attach_effect(e, attach_spray(clown_spray_base, clown_spray_dir, &e->groups[18]));
				break;
			}
			break;
		case 6:
			g->flags |= 0xf0000000;
			e->groups[4].flags |= 0xf0000000;
			break;
		case 0:
		case 7:
		case 8:
			replace_group(e, &e->groups[0], &e->groups[20]);
			e->groups[7].flags |= 0xf0000000;
			break;
		case 13:
		case 14:
		case 15:
			replace_group(e, &e->groups[13], &e->groups[19]);
			e->groups[14].flags |= 0xf0000000;
			break;
		case 1:
		case 2:
		case 3:
			replace_group(e, &e->groups[1], &e->groups[17]);
			e->groups[2].flags |= 0xf0000000;
			break;
		case 10:
		case 11:
		case 12:
			replace_group(e, &e->groups[10], &e->groups[16]);
			e->groups[11].flags |= 0xf0000000;
			break;
		case 4:
		case 9:
		default:
			g->flags |= 0xf0000000;
			break;
		}
	}
	if(!(*(int *)&e->ai_state & 0x6400))	/* read as a word with the flags beside it */
	{
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			award_points(p->index, 500);
			play_enemy_sound(e, clown_death_sounds[randrng(12)], "smeek", 8);
			e->killer = p;
			mark_die(obj->id, p);
			randrng(100);
			if(e->flags2 & CLOWN_HOPPER)
			{
				e->ai_state = 0x2001;
				e->frame = 0x1b7;
				e->flags &= 0xfffd;
			}
			else
			{
				e->ai_state = 0x2002;
				e->frame = 0x140;
				e->flags &= 0xfffd;
			}
		}
		else
		{
			award_points(p->index, 50);
			play_enemy_sound(e, clown_hurt_sounds[randrng(12)], "smeek", 2);
		}
	}
	if(e->ai_state & 0xe000)
	{
		return(0);
	}
	if(e->flags2 & CLOWN_HOPPER)
	{
		start_hit_reaction(e, 0x7d, 0x1107, submesh_info_clown, 4);
	}
	else
	{
		switch(group)
		{
		case 10:
		case 11:
		case 12:
		case 16:
			start_hit_reaction(e, 0x15f, 0x1005, submesh_info_clown, 4);
			break;
		case 1:
		case 2:
		case 3:
		case 17:
			start_hit_reaction(e, 0x175, 0x1006, submesh_info_clown, 4);
			break;
		case 5:
		case 18:
			if(randrng(100) < 50)
			{
				start_hit_reaction(e, 0x189, 0x1003, submesh_info_clown, 4);
			}
			else
			{
				start_hit_reaction(e, 0x189, 0x1004, submesh_info_clown, 4);
			}
			break;
		case 4:
		case 6:
			if(randrng(100) < 50)
			{
				start_hit_reaction(e, 0xb8, 0x1001, submesh_info_clown, 4);
			}
			else
			{
				start_hit_reaction(e, 0xb8, 0x1002, submesh_info_clown, 4);
			}
			break;
		case 13:
		case 14:
		case 15:
		case 19:
			start_hit_reaction(e, 0x197, 0x1008, submesh_info_clown, 4);
			break;
		case 0:
		case 7:
		case 8:
		case 20:
			start_hit_reaction(e, 0x1a6, 0x1009, submesh_info_clown, 4);
			break;
		}
	}
	return(0);
}

/* 0x801350b4 */
void load_clown_texmap(void)
{
	load_texmap(clown_texmap, 0x32, "clwn");
}

/* 0x801350e4: the model, the animation and the path; the damaged parts 18, 19, 20, 16 and 17 let
   their own animation go and share that of the whole parts 5, 13, 0, 10 and 1 */
void load_clown(void)
{
	load_model((void **)&clown_verts, CLOWN_NVERTS, (void **)&clown_faces, CLOWN_NFACES, (void **)&clown_normals, CLOWN_NNORMALS, "CLWN.ZM", 0x29c00691);
	load_anim(submesh_info_clown, CLOWN_GROUPS, CLOWN_FRAMES, "clwn.za", 0xad161652);
	load_path(&clown_path, CLOWN_PATH_LEN, "clwn.pth", 0x5d6c998d);
	free(submesh_info_clown[18].anim);
	free(submesh_info_clown[19].anim);
	free(submesh_info_clown[20].anim);
	free(submesh_info_clown[16].anim);
	free(submesh_info_clown[17].anim);
	submesh_info_clown[18].anim = submesh_info_clown[5].anim;
	submesh_info_clown[19].anim = submesh_info_clown[13].anim;
	submesh_info_clown[20].anim = submesh_info_clown[0].anim;
	submesh_info_clown[16].anim = submesh_info_clown[10].anim;
	submesh_info_clown[17].anim = submesh_info_clown[1].anim;
}

/* 0x8013522c: the assets go when the level is unloaded (the shared animations are let go first) */
void remove_clown_assets(void)
{
	submesh_info_clown[18].anim = 0;
	submesh_info_clown[19].anim = 0;
	submesh_info_clown[20].anim = 0;
	submesh_info_clown[16].anim = 0;
	submesh_info_clown[17].anim = 0;
	remove_surface(clown_texmap, 0x32);
	remove_path(&clown_path, CLOWN_PATH_LEN, "clwn.pth");
	remove_animation(submesh_info_clown, CLOWN_GROUPS, CLOWN_FRAMES, "clwn.za");
	remove_model((void **)&clown_verts, CLOWN_NVERTS, (void **)&clown_faces, CLOWN_NFACES, (void **)&clown_normals, CLOWN_NNORMALS, "CLWN.ZM");
}

/* 0x801352f4: the init callback (type 0x2063): the record, 7.5 health, the 21 groups with the
   damaged parts hidden */
int func_801352f4(game_object_t *obj)
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
	obj->hit = clown_hit;
	obj->die = clown_die;
	obj->remove = clown_remove;
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
	e->x = 16.0f;
	e->unkcc = 1.0f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 7.5f);
	e->groups = malloc(CLOWN_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < CLOWN_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[18].flags = 0;
	e->groups[19].flags = 0;
	e->groups[20].flags = 0;
	e->groups[16].flags = 0;
	e->groups[17].flags = 0;
	e->unka8 = 0;
	init_groups(e, submesh_info_clown, CLOWN_GROUPS);
	g = e->groups;
	e->hit_group = &g[CLOWN_HIT_GROUP];
	g[CLOWN_HIT_GROUP].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	func_80135f98(e, 0);
	return(0);
}

/* 0x801354a4: the remove callback */
void clown_remove(game_object_t *obj)
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

/* a hit reaction plays to its last frame, then blends back to where it was */
#define HIT_REACTION(last) \
	frame = e->frame; \
	e->frame = frame + 1; \
	if(e->frame < last) \
	{ \
		break; \
	} \
	e->frame = frame; \
	blend_to_frame(e, submesh_info_clown, 4, e->resume_frame, 2); \
	e->ai_state = e->resume_state; \
	break;

/* a blow: the hand within reach on the frame it lands */
#define BLOW(at, hand, damage) \
	if(e->frame == at && group_near_camera(&e->groups[hand], CLOWN_REACH)) \
	{ \
		hurt_player(e->target, damage, 3, 0.0f); \
	}

/* 0x801354fc: the think callback (type 0x2063) */
void func_801354fc(game_object_t *obj)
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
	animate_loose_groups(e, CLOWN_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1003:
		HIT_REACTION(0x197)
	case 0x1004:
		HIT_REACTION(0x197)
	case 0x1001:
		HIT_REACTION(0xc4)
	case 0x1002:
		HIT_REACTION(0xc4)
	case 0x1005:
		HIT_REACTION(0x175)
	case 0x1006:
		HIT_REACTION(0x189)
	case 0x1008:
		HIT_REACTION(0x1a6)
	case 0x1009:
		HIT_REACTION(0x1b7)
	case 0x1107:
		HIT_REACTION(0x9d)
	case CLOWN_PATH:
		path_follow_turned(e, clown_path, e->path, 1.0f);
		if(e->path >= e->unk92)
		{
			clown_end_path(e);
		}
		if(++e->frame >= 0x10)
		{
			e->frame = 0;
		}
		break;
	case CLOWN_PATH_ONCE:
		path_follow_turned(e, clown_path, e->path, 1.0f);
		if(e->path >= e->unk92)
		{
			e->path = e->unk92;
		}
		if(++e->frame < 0x64)
		{
			break;
		}
		clown_end_path(e);
		break;
	case CLOWN_PATH_WAIT:
		if(++e->frame >= 0x1f8)
		{
			e->frame = 0x1d5;
		}
		if(++e->counter < 0x15)
		{
			break;
		}
		clown_end_path(e);
		break;
	case CLOWN_TAUNT:
		if(++e->counter >= 10)
		{
			e->frame++;
		}
		if(e->frame < 0x10a)
		{
			break;
		}
		clown_end_path(e);
		break;
	case CLOWN_TAUNT2:
		if(++e->frame < 0x12c)
		{
			break;
		}
		clown_end_path(e);
		break;
	case CLOWN_DESCEND:
		if(e->y <= 0.0f)
		{
			e->frame++;
		}
		else
		{
			e->y -= 1.0f;
		}
		if(e->frame < 0x211)
		{
			break;
		}
		clown_end_path(e);
		break;
	case CLOWN_STAND:
		if(++e->frame >= 0x10)
		{
			e->frame = 0;
		}
		face_camera(e, 0.2f);
		clown_approach(e);
		break;
	case CLOWN_HOP_STAND:
		if(++e->frame >= 0x42)
		{
			e->frame = 0x10;
		}
		face_camera(e, 0.2f);
		clown_approach(e);
		break;
	case CLOWN_BLOW_A:
		face_camera(e, 0.2f);
		++e->frame;
		BLOW(0x13a, CLOWN_HAND, CLOWN_BLOW)
		if(e->frame < 0x140)
		{
			break;
		}
		clown_approach(e);
		break;
	case CLOWN_BLOW_B:
		face_camera(e, 0.2f);
		++e->frame;
		BLOW(0x13a, CLOWN_HAND, CLOWN_BLOW)
		if(e->frame < 0x140)
		{
			break;
		}
		clown_approach(e);
		break;
	case CLOWN_BLOW_C:
		face_camera(e, 0.2f);
		++e->frame;
		BLOW(0x13a, CLOWN_HAND, CLOWN_BLOW)
		if(e->frame < 0x140)
		{
			break;
		}
		clown_approach(e);
		break;
	case CLOWN_COMBO_A:
		face_camera(e, 0.2f);
		++e->frame;
		BLOW(0x24c, CLOWN_HAND, CLOWN_BLOW2)
		if(e->frame < 0x258)
		{
			break;
		}
		clown_approach(e);
		break;
	case CLOWN_COMBO_B:
		face_camera(e, 0.2f);
		++e->frame;
		BLOW(0x268, CLOWN_HAND, CLOWN_BLOW2)
		if(e->frame < 0x270)
		{
			break;
		}
		clown_approach(e);
		break;
	case CLOWN_COMBO_C:
		face_camera(e, 0.2f);
		++e->frame;
		BLOW(0x276, CLOWN_HAND2, CLOWN_BLOW2)
		BLOW(0x27f, CLOWN_HAND, CLOWN_BLOW2)
		if(e->frame < 0x282)
		{
			break;
		}
		clown_approach(e);
		break;
	case CLOWN_COMBO_D:
		face_camera(e, 0.2f);
		++e->frame;
		BLOW(0x291, CLOWN_HAND2, CLOWN_BLOW2)
		BLOW(0x299, CLOWN_HAND, CLOWN_BLOW2)
		BLOW(0x29e, CLOWN_HAND2, CLOWN_BLOW2)
		if(e->frame < 0x2a6)
		{
			break;
		}
		clown_approach(e);
		break;
	case CLOWN_LUNGE:
		face_camera(e, 0.2f);
		++e->frame;
		BLOW(0xa9, CLOWN_HAND, CLOWN_BLOW)
		if(e->frame < 0xb8)
		{
			break;
		}
		clown_approach(e);
		break;
	case CLOWN_THROW:
		face_camera(e, 0.2f);
		if(++e->frame == 0x21e && (e->groups[CLOWN_HAND].flags & 0x2000))
		{
			throw_at_camera(e->groups[CLOWN_MALLET].pos, WEAPON_UNK33, e->scale);
			e->groups[CLOWN_MALLET].flags = 0;
			e->flags2 |= CLOWN_ALT;
		}
		if(e->frame < 0x22e)
		{
			break;
		}
		clown_end_path(e);
		break;
	case CLOWN_WALK:
		if(++e->frame < 0x1f8)
		{
			break;
		}
		clown_approach(e);
		break;
	case CLOWN_HOP:
		if(++e->frame < 0x7d)
		{
			break;
		}
		clown_approach(e);
		break;
	case CLOWN_HOLD:
		if(++e->frame >= 0x1f8)
		{
			e->frame = 0x1d5;
		}
		break;
	case 0x2001:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame >= 0x1d5)
		{
			e->frame = frame;
			e->ai_state = 0x4000;
		}
		break;
	case 0x2002:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame >= 0x15f)
		{
			e->frame = frame;
			e->ai_state = 0x4000;
		}
		break;
	case 0:
		if(++e->frame >= CLOWN_FRAMES)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x80135f98: the set-up the spawn table asks for (type 0x2063); the 0x100 bit makes the hopping
   variant, the 0x80 bit the combination variant with its mallet hidden */
void func_80135f98(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	if(state & CLOWN_HOPPER)
	{
		e->flags2 |= CLOWN_HOPPER;
	}
	if(e->ai_state & CLOWN_ALT)
	{
		e->ai_state &= 0xff7f;
		e->flags2 |= CLOWN_ALT;
		e->groups[CLOWN_MALLET].flags = 0;
	}
	switch(e->ai_state)
	{
	case CLOWN_BLOW_A:
		e->frame = 0x12c;
		break;
	case CLOWN_HOP_STAND:
		e->frame = 0x10;
		break;
	case CLOWN_TAUNT:
		e->counter = 0;
		e->frame = 0xc4;
		break;
	case CLOWN_TAUNT2:
		e->frame = 0x10a;
		break;
	case CLOWN_PATH:
		e->frame = 0;
		path_follow_turned(e, clown_path, e->path, 1.0f);
		break;
	case CLOWN_DESCEND:
		e->frame = 0x1f8;
		break;
	case CLOWN_THROW:
		e->frame = 0x211;
		break;
	case CLOWN_PATH_ONCE:
		e->frame = 0x42;
		path_follow_turned(e, clown_path, e->path, 1.0f);
		break;
	case CLOWN_PATH_WAIT:
		e->frame = 0x1d5;
		e->counter = 0;
		path_follow_turned(e, clown_path, e->path, 1.0f);
		break;
	case CLOWN_COMBO_D:
		e->frame = 0x282;
		break;
	case 0:
	case CLOWN_STAND:
		e->frame = 0;
		break;
	default:
		e->frame = 0;
		break;
	}
}

/* 0x8013614c: go for the player: walk to 28 units in front of the camera and pick the state by
   how close it got (0 cannot get there: stand, 1 still walking or hopping, 2 in reach: a blow) */
void clown_approach(enemy_t *e)
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
	if(e->flags2 & CLOWN_HOPPER)
	{
		set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0.0f, -28.0f);
		r = step_toward_goal_xz(e, 0.8f);
		switch(r)
		{
		case 1:
			if(e->ai_state == CLOWN_HOP)
			{
				e->frame = 0x64;
				break;
			}
			if(e->frame > 0)
			{
				e->frame--;
			}
			blend_to_frame(e, submesh_info_clown, 4, 0x64, 2);
			e->ai_state = CLOWN_HOP;
			break;
		case 2:
			play_enemy_sound(e, clown_taunt_sounds[randrng(12)], "smeek", 0);
			if(e->frame > 0)
			{
				e->frame--;
			}
			blend_to_frame(e, submesh_info_clown, 4, 0x9d, 2);
			e->ai_state = CLOWN_LUNGE;
			break;
		case 0:
			if(e->ai_state == CLOWN_HOP_STAND)
			{
				break;
			}
			if(e->frame > 0)
			{
				e->frame--;
			}
			blend_to_frame(e, submesh_info_clown, 4, 0x10, 2);
			e->ai_state = CLOWN_HOP_STAND;
			break;
		}
	}
	else if(e->flags2 & CLOWN_ALT)
	{
		set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0.0f, -28.0f);
		r = step_toward_goal_xz(e, 0.8f);
		switch(r)
		{
		case 1:
			if(e->ai_state == CLOWN_WALK)
			{
				e->frame = 0x1d5;
				break;
			}
			if(e->frame > 0)
			{
				e->frame--;
			}
			blend_to_frame(e, submesh_info_clown, 4, 0x1d5, 2);
			e->ai_state = CLOWN_WALK;
			break;
		case 2:
			if(e->frame > 0)
			{
				e->frame--;
			}
			n = randrng(100);
			if(n < 35)
			{
				blend_to_frame(e, submesh_info_clown, 4, 0x22e, 2);
				e->ai_state = CLOWN_COMBO_A;
			}
			else if(n < 65)
			{
				blend_to_frame(e, submesh_info_clown, 4, 0x258, 2);
				e->ai_state = CLOWN_COMBO_B;
			}
			else
			{
				blend_to_frame(e, submesh_info_clown, 4, 0x270, 2);
				e->ai_state = CLOWN_COMBO_C;
			}
			break;
		case 0:
			if(e->ai_state == CLOWN_STAND)
			{
				break;
			}
			if(e->frame > 0)
			{
				e->frame--;
			}
			blend_to_frame(e, submesh_info_clown, 4, 0, 2);
			e->ai_state = CLOWN_STAND;
			break;
		}
	}
	else
	{
		set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0.0f, -28.0f);
		r = step_toward_goal_xz(e, 0.8f);
		switch(r)
		{
		case 1:
			if(e->ai_state == CLOWN_WALK)
			{
				e->frame = 0x1d5;
				break;
			}
			if(e->frame > 0)
			{
				e->frame--;
			}
			blend_to_frame(e, submesh_info_clown, 4, 0x1d5, 2);
			e->ai_state = CLOWN_WALK;
			break;
		case 2:
			if(e->frame > 0)
			{
				e->frame--;
			}
			n = randrng(100);
			if(n < 35)
			{
				blend_to_frame(e, submesh_info_clown, 4, 0x12c, 2);
				e->ai_state = CLOWN_BLOW_A;
			}
			else if(n < 65)
			{
				blend_to_frame(e, submesh_info_clown, 4, 0x12c, 2);
				e->ai_state = CLOWN_BLOW_B;
			}
			else
			{
				blend_to_frame(e, submesh_info_clown, 4, 0x12c, 2);
				e->ai_state = CLOWN_BLOW_C;
			}
			break;
		case 0:
			if(e->ai_state == CLOWN_STAND)
			{
				break;
			}
			if(e->frame > 0)
			{
				e->frame--;
			}
			blend_to_frame(e, submesh_info_clown, 4, 0, 2);
			e->ai_state = CLOWN_STAND;
			break;
		}
	}
}

/* 0x8013662c: at its path's end or a taunt's: a call, then what the spawn entry asks for: loop the
   path (0x8801), throw the mallet (0x205), hold (2), or go for the player */
void clown_end_path(enemy_t *e)
{
	play_enemy_sound(e, clown_taunt_sounds[randrng(12)], "smeek", 0);
	switch((unsigned short)e->unk6)
	{
	case CLOWN_PATH:
		if(e->frame > 0)
		{
			e->frame--;
		}
		e->unk6 = 0;
		blend_to_frame(e, submesh_info_clown, 4, 0, 2);
		e->ai_state = CLOWN_PATH;
		break;
	case CLOWN_THROW:
		if(e->frame > 0)
		{
			e->frame--;
		}
		e->unk6 = 0;
		blend_to_frame(e, submesh_info_clown, 4, 0x211, 2);
		e->ai_state = CLOWN_THROW;
		break;
	case CLOWN_HOLD:
		if(e->frame > 0)
		{
			e->frame--;
		}
		e->unk6 = 0;
		blend_to_frame(e, submesh_info_clown, 4, 0x1d5, 2);
		e->ai_state = CLOWN_HOLD;
		break;
	default:
		e->unk6 = 0;
		clown_approach(e);
		break;
	}
}

/* 0x80136780: parts 8 and 7 knocked loose from part 7 (nothing in the image calls it) */
void func_80136780(enemy_t *e)
{
	detach_group(&e->groups[8], e->groups[7].pos, e->y, e->scale);
	detach_group(&e->groups[7], e->groups[7].pos, e->y, e->scale);
}

/* 0x801367e0: parts 15 and 14 knocked loose from part 14 (nothing in the image calls it) */
void func_801367e0(enemy_t *e)
{
	detach_group(&e->groups[15], e->groups[14].pos, e->y, e->scale);
	detach_group(&e->groups[14], e->groups[14].pos, e->y, e->scale);
}
