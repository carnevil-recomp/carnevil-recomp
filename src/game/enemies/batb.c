/*
 * batb.c - the Big Top's BATB (object type 0x2061, assets BATB.ZM / batb.za / batb.pth and the
 * batb*.WMS texmaps: legs, arms, chest, pelvis, head, ears, nose, four wings, feet, a bottle, a
 * staff, hands; its calls are in the "mortito" bank). One original file, 0x80133580-0x801348dc
 * (a padding word follows), its tables at 0x8019ee88 (batb_data.c), rodata 0x801745b0-0x8017468c.
 * The model has 601 vertices, 980 faces and 606 normals in 21 submeshes, the animation 138
 * frames, the path 649 points.
 *
 * A winged figure on foot: it walks its path (0x8801 turned along it, 0x8803 placed on it,
 * 0x8802 holding for 21 frames first), then taunts and goes for the player: it walks to a point
 * by the camera (3), swings at it in reach (0x201, or 0x202 for the spawn entry's 0x80 variant,
 * which shows part 14 instead of 16), 190 damage when its hand part (20, or 15 for 0x203) is
 * within 20 units, and stands (1) when it cannot get there. 0x8404 is a taunt loop. It has 3.5
 * health: a hit plays a hurt call and a reaction by part, the flame thrower sets the part alight,
 * the highest violence makes parts gory; dead (500 points) it falls (0x2001 one time in four,
 * else 0x2002).
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
extern void	path_place(enemy_t *, void *, short, float);
extern void	path_follow_turned(enemy_t *, void *, int, float);
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern int	group_near_camera(group_t *, float);
extern int	face_camera(enemy_t *, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal(enemy_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern void	attach_effect(enemy_t *, void *);
extern void	*attach_flame(float, float, group_t *);
extern void	detach_group(group_t *, float *, float, float *);
extern int	mark_die();
extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern int	violence_level;
extern char	text_buffer[];
extern enemy_t	*debug_enemy;

extern texmap_t	batb_texmap[30];		/* batb_data.c */
extern submesh_t submesh_info_batb[21];
extern int	batb_taunt_sounds[11], batb_hurt_sounds[9], batb_death_sounds[3];

float	*batb_verts;			/* 0x801e57d4 */
face_t	*batb_faces;			/* 0x80212db4 */
float	*batb_normals;			/* 0x801e0f18 */
void	*batb_path;			/* 0x8021f0e4 */

#define BATB_GROUPS	0x15
#define BATB_NVERTS	0x259
#define BATB_NFACES	0x3d4
#define BATB_NNORMALS	0x25e
#define BATB_FRAMES	0x8a
#define BATB_PATH_LEN	0x289
#define BATB_HIT_GROUP	4		/* the submesh it is hit through */
#define BATB_HAND	20		/* the part a swing lands with */
#define BATB_HAND2	15		/* ... and that of 0x203 */
#define BATB_REACH	20.0f
#define BATB_BLOW	0xbe		/* 190 damage */
#define BATB_ALT	0x80		/* the spawn entry's variant: part 14 shown, part 16 hidden */

/* the states */
#define BATB_STAND	1
#define BATB_HOLD	2		/* stands looping 0x51-0x5d */
#define BATB_WALK	3		/* walks to the point by the camera */
#define BATB_SWING	0x201
#define BATB_SWING2	0x202		/* the 0x80 variant's swing */
#define BATB_SWING3	0x203
#define BATB_TAUNT	0x8404
#define BATB_PATH	0x8801		/* turned along its path */
#define BATB_PATH_WAIT	0x8802		/* 21 frames, then 0x8801 */
#define BATB_PATH_PLACE	0x8803		/* placed on its path */

int	batb_die(game_object_t *, player_stats_t *);
void	batb_remove(game_object_t *);
void	func_80134420(enemy_t *, unsigned short);
void	batb_approach(enemy_t *);
void	batb_end_path(enemy_t *);

/* 0x80133580: the draw callback (type 0x2061) */
void func_80133580(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_batb, BATB_GROUPS, batb_verts, BATB_NVERTS, batb_faces, BATB_NFACES, batb_normals, BATB_NNORMALS);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[3].pos, e->radius, 0x600000);
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

/* 0x8013377c: the hit test: the bounding sphere, then the submesh */
int batb_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_batb, BATB_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)batb_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x80133838: the shot lands: blood and chunks; the flame thrower sets the part alight; at the
   highest violence parts turn gory; then, alive, the damage (dead: 500 points, a death call, the
   fall; else 50 points and a hurt call) and a hit reaction by part */
int batb_die(game_object_t *obj, player_stats_t *p)
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
			attach_effect(e, attach_flame(0.0f, (submesh_info_batb[group].radius + submesh_info_batb[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 5:
			e->groups[5].flags |= 0xf0000000;
			break;
		case 3:
		case 4:
			e->groups[3].flags |= 0xf0000000;
			break;
		case 1:
		case 2:
		case 20:
			e->groups[1].flags |= 0xf0000000;
			e->groups[2].flags |= 0xf0000000;
			break;
		case 17:
		case 18:
		case 19:
			e->groups[18].flags |= 0xf0000000;
			e->groups[17].flags |= 0xf0000000;
			break;
		}
	}
	if(!(*(int *)&e->ai_state & 0x6400))	/* read as a word with the flags beside it (0x80133ac0 reads the halfword) */
	{
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			award_points(p->index, 500);
			play_enemy_sound(e, batb_death_sounds[randrng(3)], "mortito", 8);
			e->killer = p;
			mark_die(obj->id, p);
			if(randrng(100) < 25)
			{
				e->ai_state = 0x2001;
			}
			else
			{
				e->ai_state = 0x2002;
			}
			e->frame = 0x3d;
			e->flags &= 0xfffd;
		}
		else
		{
			award_points(p->index, 50);
			play_enemy_sound(e, batb_hurt_sounds[randrng(9)], "mortito", 2);
		}
	}
	if(e->ai_state & 0xe000)
	{
		return(0);
	}
	switch(group)
	{
	case 0:
	case 10:
	case 15:
		start_hit_reaction(e, 0x5e, 0x1005, submesh_info_batb, 4);
		break;
	case 11:
	case 12:
	case 13:
		start_hit_reaction(e, 0x5e, 0x1006, submesh_info_batb, 4);
		break;
	case 5:
		if(randrng(100) < 50)
		{
			start_hit_reaction(e, 0x5e, 0x1003, submesh_info_batb, 4);
		}
		else
		{
			start_hit_reaction(e, 0x5e, 0x1004, submesh_info_batb, 4);
		}
		break;
	case 3:
	case 4:
		if(randrng(100) < 50)
		{
			start_hit_reaction(e, 0x5e, 0x1001, submesh_info_batb, 4);
		}
		else
		{
			start_hit_reaction(e, 0x5e, 0x1002, submesh_info_batb, 4);
		}
		break;
	}
	return(0);
}

/* 0x80133bc8 */
void load_batb_texmap(void)
{
	load_texmap(batb_texmap, 0x1e, "batb");
}

/* 0x80133bf8: the model, the animation and the path */
void load_batb(void)
{
	load_model((void **)&batb_verts, BATB_NVERTS, (void **)&batb_faces, BATB_NFACES, (void **)&batb_normals, BATB_NNORMALS, "BATB.ZM", 0x72d2c58e);
	load_anim(submesh_info_batb, BATB_GROUPS, BATB_FRAMES, "batb.za", 0xc1ba5a59);
	load_path(&batb_path, BATB_PATH_LEN, "batb.pth", 0x38f3581c);
}

/* 0x80133ca0: all its assets go when the level is unloaded (Binary Ninja's name says texmap) */
void remove_batb_texmap(void)
{
	remove_surface(batb_texmap, 0x1e);
	remove_path(&batb_path, BATB_PATH_LEN, "batb.pth");
	remove_animation(submesh_info_batb, BATB_GROUPS, BATB_FRAMES, "batb.za");
	remove_model((void **)&batb_verts, BATB_NVERTS, (void **)&batb_faces, BATB_NFACES, (void **)&batb_normals, BATB_NNORMALS, "BATB.ZM");
}

/* 0x80133d38: the init callback (type 0x2061): the record, 3.5 health, the 21 groups (part 14
   drawn the other way, 8), standing */
int func_80133d38(game_object_t *obj)
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
	obj->hit = batb_hit;
	obj->die = batb_die;
	obj->remove = batb_remove;
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
	e->x = 8.0f;
	e->unkcc = 0.5f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 3.5f);
	e->groups = malloc(BATB_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < BATB_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[14].flags |= 8;
	e->unka8 = 0;
	init_groups(e, submesh_info_batb, BATB_GROUPS);
	g = e->groups;
	e->hit_group = &g[BATB_HIT_GROUP];
	g[BATB_HIT_GROUP].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	func_80134420(e, 0);
	return(0);
}

/* 0x80133ed0: the remove callback */
void batb_remove(game_object_t *obj)
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
	blend_to_frame(e, submesh_info_batb, 4, e->resume_frame, 2); \
	e->ai_state = e->resume_state; \
	break;

/* 0x80133f28: the think callback (type 0x2061) */
void func_80133f28(game_object_t *obj)
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
	animate_loose_groups(e, BATB_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1001:
		HIT_REACTION(0x73)
	case 0x1002:
		HIT_REACTION(0x73)
	case 0x1003:
		HIT_REACTION(0x73)
	case 0x1004:
		HIT_REACTION(0x73)
	case 0x1005:
		HIT_REACTION(0x73)
	case 0x1006:
		HIT_REACTION(0x73)
	case BATB_PATH:
		path_follow_turned(e, batb_path, e->path, 1.0f);
		if(e->path >= e->unk92)
		{
			batb_end_path(e);
		}
		if(++e->frame >= 0x5e)
		{
			e->frame = 0x51;
		}
		break;
	case BATB_PATH_PLACE:
		path_place(e, batb_path, e->path, 1.0f);
		if(++e->path >= e->unk92)
		{
			batb_end_path(e);
		}
		if(++e->frame >= 0x5e)
		{
			e->frame = 0x51;
		}
		break;
	case BATB_PATH_WAIT:
		if(++e->counter > 0x14)
		{
			e->ai_state = BATB_PATH;
		}
		if(++e->frame >= 0x5e)
		{
			e->frame = 0x51;
		}
		break;
	case BATB_STAND:
		if(++e->frame >= 0x3d)
		{
			e->frame = 0x37;
		}
		face_camera(e, 0.2f);
		batb_approach(e);
		break;
	case BATB_SWING:
		face_camera(e, 0.2f);
		if(++e->frame == 0x2b && group_near_camera(&e->groups[BATB_HAND], BATB_REACH))
		{
			hurt_player(e->target, BATB_BLOW, 3, 0.0f);
		}
		if(e->frame < 0x37)
		{
			break;
		}
		batb_approach(e);
		break;
	case BATB_SWING2:
		face_camera(e, 0.2f);
		if(++e->frame == 0x79 && group_near_camera(&e->groups[BATB_HAND], BATB_REACH))
		{
			hurt_player(e->target, BATB_BLOW, 3, 0.0f);
		}
		if(e->frame == 0x81 && group_near_camera(&e->groups[BATB_HAND], BATB_REACH))
		{
			hurt_player(e->target, BATB_BLOW, 3, 0.0f);
		}
		if(e->frame < 0x8a)
		{
			break;
		}
		batb_approach(e);
		break;
	case BATB_SWING3:
		face_camera(e, 0.2f);
		if(++e->frame == 0x2e && group_near_camera(&e->groups[BATB_HAND2], BATB_REACH))
		{
			hurt_player(e->target, BATB_BLOW, 3, 0.0f);
		}
		if(e->frame < 0x37)
		{
			break;
		}
		batb_approach(e);
		break;
	case BATB_WALK:
		if(++e->frame < 0x5e)
		{
			break;
		}
		batb_approach(e);
		break;
	case BATB_HOLD:
		if(++e->frame >= 0x5e)
		{
			e->frame = 0x51;
		}
		break;
	case BATB_TAUNT:
		if(++e->frame == 0x16)
		{
			play_enemy_sound(e, batb_taunt_sounds[randrng(11)], "mortito", 0);
		}
		if(e->frame >= 0x22)
		{
			e->frame = 0xc;
		}
		break;
	case 0x2001:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame >= 0x51)
		{
			e->frame = frame;
			e->ai_state = 0x4000;
		}
		break;
	case 0x2002:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame >= 0x51)
		{
			e->frame = frame;
			e->ai_state = 0x4000;
		}
		break;
	case 0:
		if(++e->frame >= BATB_FRAMES)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x80134420: the set-up the spawn table asks for (type 0x2061); the 0x80 bit picks the variant */
void func_80134420(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	if(state & BATB_ALT)
	{
		e->ai_state = state & 0xff7f;
		e->flags2 |= BATB_ALT;
		e->groups[16].flags = 0;
		e->groups[14].flags = 0xa008;
	}
	else
	{
		e->groups[16].flags = 0xa000;
		e->groups[14].flags = 0;
	}
	switch(e->ai_state)
	{
	case BATB_SWING:
		e->frame = 0x22;
		break;
	case BATB_STAND:
		e->frame = 0x37;
		break;
	case BATB_TAUNT:
		e->frame = 0xc;
		e->groups[16].flags = 0;
		e->groups[14].flags = 0;
		break;
	case BATB_PATH:
		e->frame = 0x51;
		path_follow_turned(e, batb_path, e->path, 1.0f);
		break;
	case BATB_PATH_PLACE:
		e->frame = 0x51;
		path_place(e, batb_path, e->path, 1.0f);
		break;
	case BATB_PATH_WAIT:
		e->counter = 0;
		e->frame = 0x51;
		path_follow_turned(e, batb_path, e->path, 1.0f);
		break;
	case 0:
		e->frame = 0;
		break;
	default:
		e->frame = 0;
		break;
	}
}

/* 0x80134580: go for the player: walk to a point by the camera and pick the state by how close
   it got (0 cannot get there: stand, 1 still walking, 2 in reach: a taunt call and a swing) */
void batb_approach(enemy_t *e)
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
	r = step_toward_goal(e, 1.3f);
	switch(r)
	{
	case 1:
		if(e->ai_state == BATB_WALK)
		{
			e->frame = 0x51;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_batb, 4, 0x51, 2);
		e->ai_state = BATB_WALK;
		break;
	case 2:
		play_enemy_sound(e, batb_taunt_sounds[randrng(11)], "mortito", 0);
		if(e->frame > 0)
		{
			e->frame--;
		}
		if(e->flags2 & BATB_ALT)
		{
			blend_to_frame(e, submesh_info_batb, 4, 0x73, 2);
			e->ai_state = BATB_SWING2;
		}
		else
		{
			blend_to_frame(e, submesh_info_batb, 4, 0x22, 2);
			e->ai_state = BATB_SWING;
		}
		break;
	case 0:
		if(e->ai_state == BATB_STAND)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_batb, 4, 0x37, 2);
		e->ai_state = BATB_STAND;
		break;
	}
}

/* 0x80134774: at its path's end: a taunt call; the spawn entry's 2 makes it hold its place
   (0x51 on), else it goes for the player */
void batb_end_path(enemy_t *e)
{
	play_enemy_sound(e, batb_taunt_sounds[randrng(11)], "mortito", 0);
	if((unsigned short)e->unk6 == 2)
	{
		if(e->frame > 0)
		{
			e->frame--;
		}
		e->unk6 = 0;
		blend_to_frame(e, submesh_info_batb, 4, 0x51, 2);
		e->ai_state = BATB_HOLD;
	}
	else
	{
		e->unk6 = 0;
		batb_approach(e);
	}
}

/* 0x8013481c: parts 20 and 2 knocked loose from part 2 (nothing in the image calls it) */
void func_8013481c(enemy_t *e)
{
	detach_group(&e->groups[20], e->groups[2].pos, e->y, e->scale);
	detach_group(&e->groups[2], e->groups[2].pos, e->y, e->scale);
}

/* 0x8013487c: parts 19 and 17 knocked loose from part 17 (nothing in the image calls it) */
void func_8013487c(enemy_t *e)
{
	detach_group(&e->groups[19], e->groups[17].pos, e->y, e->scale);
	detach_group(&e->groups[17], e->groups[17].pos, e->y, e->scale);
}
