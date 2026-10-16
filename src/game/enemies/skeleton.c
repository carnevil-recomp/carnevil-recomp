/*
 * skeleton.c - the Big Top's skeleton (object type 0x2068; assets SKEL.ZM / skel.za / skel.pth
 * and the skel*.WMS texmaps: skull, chest, pelvis, bones, hands; its voice is the "skeleton"
 * bank). One original file, 0x8013cfb0-0x8013ea04 (Doctor Klot's file comes before it, binn's
 * after; the word at 0x8013ea04 is padding); its tables are in skeleton_data.c
 * (0x801a1bc0-0x801a20b8). The model has 388 vertices, 726 faces and 528 normals in 16
 * submeshes, the animation 487 frames, the path 317 points.
 *
 * Unlike its neighbours this file was compiled at -O (match it with the game1 profile, not
 * game2): only that reproduces its code.
 *
 * Its eyes glow (two markers on its skull, submesh 0) while its head is on. It walks its path
 * (0x8801), stands (2), idles (0, 4, 5) or waits (6, 0x209); then it closes on a point in front of
 * the camera, taunting when it gets there, and attacks: 0x201, 0x204, 0x206, 0x208 and 0x802
 * strike with its hands (parts 8 and 11, within 14 units), 0x207 bites with its skull, and 0x203 and
 * 0x205 throw its skull at the camera (which then goes from its body). A shot at the top
 * violence level knocks the part hit off; 0.75 health, 500 points for the kill and a death
 * call, 50 and a hurt call for a hit; it falls three ways (0x2001-0x2003), fading out as it does,
 * and is gone (0x4000).
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
#include "weapons.h"			/* WEAPON_SKELETON */

extern void	draw_effects(enemy_t *);
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	detach_group(group_t *, float *, float, float *);
extern void	animate_loose_groups(enemy_t *, int);
extern void	group_point(group_t *, float *);
extern void	draw_marker(float *, float, unsigned int);
extern float	func_800fe8dc(float);			/* a random float in +-range */
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
extern void	throw_at_camera(float *, int, float *);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal_xz(enemy_t *, float);
extern void	path_follow_turned(enemy_t *, float *, int, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern int	remove_die();
extern int	load_model(void **, int, void **, int, void **, int, char *, int);
extern int	load_anim(submesh_t *, int, int, char *, int);
extern int	load_path(void **, int, char *, int);
extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern void	play_skeleton_sound(int, int);
extern enemy_t	*debug_enemy;
extern char	text_buffer[];
extern int	violence_level;
extern player_stats_t player_stats[2];

extern texmap_t	skeleton_texmap[20];		/* skeleton_data.c */
extern submesh_t skeleton_anim_blob[16];
extern int	skeleton_taunts[8], skeleton_hurt_calls[8], skeleton_death_calls[8];
extern float	skeleton_eye1[3], skeleton_eye2[3];

/* the model's arrays keep their address names: weapons.c draws the thrown skull with them */
float	*D_801e5b28;			/* the vertices */
face_t	*D_8021eec8;			/* the faces */
float	*D_801e57e4;			/* the normals */
float	*skeleton_path;			/* 0x802372cc */

/* the fade while it falls (the record's word at 0x44) */
#define SKELETON_FADE(e)	(*(float *)&(e)->unk44)

#define SKELETON_GROUPS		0x10
#define SKELETON_NVERTS		0x184
#define SKELETON_NFACES		0x2d6
#define SKELETON_NNORMALS	0x210
#define SKELETON_FRAMES		0x1e7
#define SKELETON_PATH_LEN	0x13d
#define SKELETON_TEXMAPS	0x14
#define SKELETON_SKULL		0	/* the skull: its eyes glow, it is what it throws */
#define SKELETON_HIT_GROUP	5	/* the submesh its record is measured from */
#define SKELETON_HAND		8	/* the hands its blows land with */
#define SKELETON_HAND2		11
#define SKELETON_REACH		14.0f
#define SKELETON_BLOW		180	/* the damage of a blow */
#define HURT_STAB		3	/* hurt_player kinds: the plain mark ... */
#define HURT_SLASH		7	/* ... the slash, mirrored when param is set ... */
#define HURT_CLAW		10	/* ... and the claw */

#define SKELETON_IDLE		0
#define SKELETON_READY		1	/* in front of the camera, between blows */
#define SKELETON_STAND		2
#define SKELETON_CLOSE		3	/* at its goal, turning to it */
#define SKELETON_WAIT		6
#define SKELETON_PATH		0x8801
#define SKELETON_GONE		0x4000

/* 0x8013cfb0: the draw callback: the model, its glowing eyes while its skull is on and it is
   not dying, and the debug displays */
void func_8013cfb0(game_object_t *obj)
{
	enemy_t	*e;
	float	size;
	float	pt[3];

	e = (enemy_t *)obj->data;
	draw_model(e, skeleton_anim_blob, SKELETON_GROUPS, D_801e5b28, SKELETON_NVERTS, D_8021eec8, SKELETON_NFACES, D_801e57e4, SKELETON_NNORMALS);
	draw_effects(e);
	if((e->groups[SKELETON_SKULL].flags & 0x2000) && !(e->ai_state & 0x6000))
	{
		grAlphaTestReferenceValue(0x20);
		size = func_800fe8dc(0.1f) + 0.2f;
		pt[0] = skeleton_eye1[0];
		pt[1] = skeleton_eye1[1];
		pt[2] = skeleton_eye1[2];
		group_point(e->groups, pt);
		draw_marker(pt, size, 0xffff00);
		pt[0] = skeleton_eye2[0];
		pt[1] = skeleton_eye2[1];
		pt[2] = skeleton_eye2[2];
		group_point(e->groups, pt);
		draw_marker(pt, size, 0xffff00);
	}
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

int	skeleton_die(game_object_t *, player_stats_t *);

/* 0x8013d28c: the hit test: the bounding sphere, then the submesh */
int skeleton_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, skeleton_anim_blob, SKELETON_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)skeleton_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x8013d348: the shot lands: at the top violence level the part hit is knocked off; then the
   damage, points and a call, a fall (it starts to fade) or a hit reaction by the part hit */
int skeleton_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;
	int	group;
	int	r;

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return(0);
	}
	group = p->hit_group;
	g = &e->groups[group];
	if(violence_level >= 3)
	{
		detach_group(g, g->pos, e->y, e->scale);
	}
	if(!(e->ai_state & 0x6000))
	{
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			e->alpha = 255.0f;
			SKELETON_FADE(e) = -15.0f;
			award_points(p->index, 500);
			play_skeleton_sound(skeleton_death_calls[randrng(8)], 8);
			e->killer = p;
			r = randrng(100);
			if(r < 33)
			{
				e->ai_state = 0x2001;
				e->flags &= 0xfffd;
				e->frame = 0x153;
			}
			else if(r < 66)
			{
				e->ai_state = 0x2002;
				e->flags &= 0xfffd;
				e->frame = 0x1ab;
			}
			else
			{
				e->ai_state = 0x2003;
				e->flags &= 0xfffd;
				e->frame = 0x1c9;
			}
		}
		else
		{
			award_points(p->index, 50);
			play_skeleton_sound(skeleton_hurt_calls[randrng(8)], 2);
		}
	}
	if(e->ai_state & 0xe000)
	{
		return(0);
	}
	switch(group)
	{
	case 12:
	case 13:
	case 14:
		start_hit_reaction(e, 0x1a, 0x1005, skeleton_anim_blob, 5);
		break;
	case 1:
	case 2:
	case 3:
		start_hit_reaction(e, 0x1a, 0x1006, skeleton_anim_blob, 5);
		break;
	case 9:
	case 10:
	case 11:
		start_hit_reaction(e, 0x1a, 0x1007, skeleton_anim_blob, 5);
		break;
	case 6:
	case 7:
	case 8:
		start_hit_reaction(e, 0x1a, 0x1008, skeleton_anim_blob, 5);
		break;
	case 0:
		if(randrng(100) < 50)
		{
			start_hit_reaction(e, 0x1a, 0x1003, skeleton_anim_blob, 5);
		}
		else
		{
			start_hit_reaction(e, 0x1a, 0x1004, skeleton_anim_blob, 5);
		}
		break;
	case 5:
	case 15:
		if(randrng(100) < 50)
		{
			start_hit_reaction(e, 0x1a, 0x1001, skeleton_anim_blob, 5);
		}
		else
		{
			start_hit_reaction(e, 0x1a, 0x1002, skeleton_anim_blob, 5);
		}
		break;
	}
	return(0);
}

/* 0x8013d624 */
void load_skeleton_texmap(void)
{
	load_texmap(skeleton_texmap, SKELETON_TEXMAPS, "skel");
}

/* 0x8013d654 */
void load_skeleton(void)
{
	load_model((void **)&D_801e5b28, SKELETON_NVERTS, (void **)&D_8021eec8, SKELETON_NFACES, (void **)&D_801e57e4, SKELETON_NNORMALS, "SKEL.ZM", 0x76b0b7b0);
	load_anim(skeleton_anim_blob, SKELETON_GROUPS, SKELETON_FRAMES, "skel.za", 0x9faf1308);
	load_path((void **)&skeleton_path, SKELETON_PATH_LEN, "skel.pth", 0x63fcdfa5);
}

/* 0x8013d6fc */
void remove_skeleton_assets(void)
{
	remove_surface(skeleton_texmap, SKELETON_TEXMAPS);
	remove_path((void **)&skeleton_path, SKELETON_PATH_LEN, "skel.pth");
	remove_animation(skeleton_anim_blob, SKELETON_GROUPS, SKELETON_FRAMES, "skel.za");
	remove_model((void **)&D_801e5b28, SKELETON_NVERTS, (void **)&D_8021eec8, SKELETON_NFACES, (void **)&D_801e57e4, SKELETON_NNORMALS, "SKEL.ZM");
}

void	skeleton_remove(game_object_t *);
void	func_8013e518(enemy_t *, unsigned short);

/* 0x8013d794: the init callback: the record, health 0.75, the 16 groups (part 15 8), the set-up */
int func_8013d794(game_object_t *obj)
{
	enemy_t	*e;
	group_t	*g;
	int	i;

	obj->data = malloc(sizeof(enemy_t));
	if(!obj->data)
	{
		return(1);
	}
	e = (enemy_t *)obj->data;
	obj->hit = skeleton_hit;
	obj->die = skeleton_die;
	obj->remove = skeleton_remove;
	e->counter = 0;
	e->unk4 = 0x14;
	e->frame = 0;
	e->num = 0;
	e->flags = 0x2c;
	e->target = get_target_player();
	e->unk54 = 0;
	e->rx = 0;
	e->ry = 3.1415927f;
	e->rz = 0;
	e->x = -8.0f;
	e->y = 0;
	e->z = 0;
	e->unkcc = 0.94f;
	set_enemy_scale(e, 0.68f);
	set_enemy_health(e, 0.75f);
	e->groups = malloc(SKELETON_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < SKELETON_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[15].flags |= 8;
	init_groups(e, skeleton_anim_blob, SKELETON_GROUPS);
	g = e->groups;
	e->hit_group = &g[SKELETON_HIT_GROUP];
	g[SKELETON_HIT_GROUP].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	func_8013e518(e, 0);
	return(0);
}

/* 0x8013d938: the remove callback */
void skeleton_remove(game_object_t *obj)
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
	blend_to_frame(e, skeleton_anim_blob, 5, e->resume_frame, 1); \
	e->ai_state = e->resume_state; \
	break;

/* a fall: while the fade lasts every part is drawn translucent (part 15 by its own mode), and
   at its last frame it leaves the die list and is gone */
#define FALL(last) \
	e->alpha += SKELETON_FADE(e); \
	if(e->alpha > 0.0f) \
	{ \
		for(i = 0; i < SKELETON_GROUPS; i++) \
		{ \
			e->groups[i].flags &= ~0x40; \
			e->groups[i].flags |= 0x80; \
			e->groups[i].alpha = e->alpha; \
		} \
		e->groups[15].flags &= ~0x80; \
		e->groups[15].flags |= 0x100; \
	} \
	frame = e->frame; \
	e->frame = frame + 1; \
	if(e->frame < last) \
	{ \
		break; \
	} \
	e->frame = frame; \
	remove_die(obj->id, e->killer); \
	e->ai_state = SKELETON_GONE; \
	break;

void	skeleton_approach(enemy_t *);
void	skeleton_end_path(enemy_t *);

/* 0x8013d990: the think callback */
void func_8013d990(game_object_t *obj)
{
	enemy_t	*e;
	short	frame;
	int	i;

	if(debug_mode & 0x10000)
	{
		return;
	}
	if(debug_mode & 0x80000)
	{
		return;
	}
	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, SKELETON_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1001:
		HIT_REACTION(0x24)
	case 0x1002:
		HIT_REACTION(0x24)
	case 0x1003:
		HIT_REACTION(0x24)
	case 0x1004:
		HIT_REACTION(0x24)
	case 0x1005:
		HIT_REACTION(0x24)
	case 0x1006:
		HIT_REACTION(0x24)
	case 0x1007:
		HIT_REACTION(0x24)
	case 0x1008:
		HIT_REACTION(0x24)
	case SKELETON_PATH:
		path_follow_turned(e, skeleton_path, e->path, 1.0f);
		if(e->path >= e->unk92)
		{
			skeleton_end_path(e);
		}
		if(++e->frame >= 0x2f)
		{
			e->frame = 0x24;
		}
		break;
	case 0x802:
		if(++e->frame == 0x74)
		{
			if(group_near_camera(&e->groups[SKELETON_HAND], SKELETON_REACH))
			{
				hurt_player(&player_stats[0], SKELETON_BLOW, HURT_CLAW, 0.0f);
			}
			if(group_near_camera(&e->groups[SKELETON_HAND2], SKELETON_REACH))
			{
				hurt_player(&player_stats[1], SKELETON_BLOW, HURT_CLAW, 0.0f);
			}
		}
		if(e->frame < 0x81)
		{
			break;
		}
		skeleton_end_path(e);
		break;
	case SKELETON_READY:
		if(++e->frame >= 0x2f)
		{
			e->frame = 0x24;
		}
		face_camera(e, 0.2f);
		skeleton_approach(e);
		break;
	case 0x201:
		face_camera(e, 0.2f);
		if(++e->frame == 0x36 && group_near_camera(&e->groups[SKELETON_HAND], SKELETON_REACH))
		{
			hurt_player(e->target, SKELETON_BLOW, HURT_CLAW, 0.0f);
		}
		if(e->frame == 0x3a && group_near_camera(&e->groups[SKELETON_HAND], SKELETON_REACH))
		{
			hurt_player(e->target, SKELETON_BLOW, HURT_SLASH, 0.0f);
		}
		if(e->frame < 0x40)
		{
			break;
		}
		skeleton_approach(e);
		break;
	case 0x206:
		face_camera(e, 0.2f);
		if(++e->frame == 0x165 && group_near_camera(&e->groups[SKELETON_HAND], SKELETON_REACH))
		{
			hurt_player(e->target, SKELETON_BLOW, HURT_CLAW, 0.0f);
		}
		if(e->frame == 0x16c && group_near_camera(&e->groups[SKELETON_HAND], SKELETON_REACH))
		{
			hurt_player(e->target, SKELETON_BLOW, HURT_SLASH, 0.0f);
		}
		if(e->frame < 0x16e)
		{
			break;
		}
		skeleton_approach(e);
		break;
	case 0x207:
		face_camera(e, 0.2f);
		if(++e->frame == 0x1a5 && group_near_camera(&e->groups[SKELETON_SKULL], SKELETON_REACH))
		{
			hurt_player(e->target, SKELETON_BLOW, HURT_STAB, 0.0f);
		}
		if(e->frame < 0x1ab)
		{
			break;
		}
		skeleton_approach(e);
		break;
	case 0x202:
		face_camera(e, 0.2f);
		switch(++e->frame)
		{
		case 0x96:
		case 0xb7:
			if(group_near_camera(&e->groups[SKELETON_HAND2], SKELETON_REACH))
			{
				hurt_player(e->target, SKELETON_BLOW, HURT_SLASH, 0.0f);
			}
			break;
		case 0xac:
		case 0xd3:
			if(group_near_camera(&e->groups[SKELETON_HAND2], SKELETON_REACH))
			{
				hurt_player(e->target, SKELETON_BLOW, HURT_CLAW, 0.0f);
			}
			break;
		}
		if(e->frame >= 0xe0)
		{
			e->frame = 0xcb;
		}
		break;
	case 0x209:
		if(++e->frame >= 0xe0)
		{
			e->frame = 0xcb;
		}
		break;
	case 0x203:
		face_camera(e, 0.2f);
		if(++e->frame == 0x10c && (e->groups[SKELETON_HAND].flags & 0x2000))
		{
			throw_at_camera(e->groups[SKELETON_SKULL].pos, WEAPON_SKELETON, e->scale);
			e->groups[SKELETON_SKULL].flags = 0;
		}
		if(e->frame < 0x12d)
		{
			break;
		}
		skeleton_approach(e);
		break;
	case 0x205:
		face_camera(e, 0.2f);
		if(++e->frame == 0x145 && (e->groups[SKELETON_HAND].flags & 0x2000))
		{
			throw_at_camera(e->groups[SKELETON_SKULL].pos, WEAPON_SKELETON, e->scale);
			e->groups[SKELETON_SKULL].flags = 0;
		}
		if(e->frame < 0x153)
		{
			break;
		}
		skeleton_approach(e);
		break;
	case 0x204:
		face_camera(e, 0.2f);
		if(++e->frame == 0x4c && group_near_camera(&e->groups[SKELETON_HAND], SKELETON_REACH))
		{
			hurt_player(e->target, SKELETON_BLOW, HURT_CLAW, 0.0f);
		}
		if(e->frame == 0x55 && group_near_camera(&e->groups[SKELETON_HAND2], SKELETON_REACH))
		{
			hurt_player(e->target, SKELETON_BLOW, HURT_SLASH, 0.0f);
		}
		if(e->frame == 0x5e && group_near_camera(&e->groups[SKELETON_HAND], SKELETON_REACH))
		{
			hurt_player(e->target, SKELETON_BLOW, HURT_SLASH, 3.1415927f);
		}
		if(e->frame < 0x6b)
		{
			break;
		}
		skeleton_end_path(e);
		break;
	case 0x208:
		face_camera(e, 0.2f);
		switch(++e->frame)
		{
		case 0x17a:
			if(group_near_camera(&e->groups[SKELETON_HAND2], SKELETON_REACH))
			{
				hurt_player(e->target, SKELETON_BLOW, HURT_CLAW, 0.0f);
			}
			break;
		case 0x185:
		case 0x196:
			if(group_near_camera(&e->groups[SKELETON_HAND], SKELETON_REACH))
			{
				hurt_player(e->target, SKELETON_BLOW, HURT_SLASH, 0.0f);
			}
			break;
		case 0x18e:
			if(group_near_camera(&e->groups[SKELETON_HAND2], SKELETON_REACH))
			{
				hurt_player(e->target, SKELETON_BLOW, HURT_SLASH, 3.1415927f);
			}
			break;
		}
		if(e->frame < 0x19c)
		{
			break;
		}
		skeleton_end_path(e);
		break;
	case SKELETON_CLOSE:
		if(++e->frame < 0x24)
		{
			break;
		}
		skeleton_approach(e);
		break;
	case SKELETON_STAND:
		if(++e->frame >= 0x24)
		{
			e->frame = 0x1a;
		}
		break;
	case SKELETON_WAIT:
		if(++e->frame < 0x100)
		{
			break;
		}
		skeleton_end_path(e);
		break;
	case 4:
		if(++e->frame >= 0xd)
		{
			e->frame = 0;
		}
		break;
	case 5:
		if(++e->frame >= 0x1a)
		{
			e->frame = 0xd;
		}
		break;
	case 0x2001:
		FALL(0x161)
	case 0x2002:
		FALL(0x1c9)
	case 0x2003:
		FALL(0x1e7)
	case SKELETON_IDLE:
		if(++e->frame >= 0x1e7)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x8013e518: the set-up: the state's first frame (on its path, placed at its point) */
void func_8013e518(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	switch(e->ai_state)
	{
	case 5:
		e->frame = 0xd;
		break;
	case SKELETON_STAND:
		e->frame = 0x1a;
		break;
	case 0x201:
		e->frame = 0x2f;
		break;
	case 0x206:
		e->frame = 0x161;
		break;
	case 0x207:
		e->frame = 0x19c;
		break;
	case 0x202:
		e->frame = 0x81;
		break;
	case 0x209:
		e->frame = 0xcb;
		break;
	case 0x203:
		e->frame = 0x100;
		break;
	case 0x205:
		e->frame = 0x12d;
		break;
	case SKELETON_READY:
		e->frame = 0x24;
		break;
	case SKELETON_WAIT:
		e->frame = 0xe0;
		break;
	case 0x204:
		e->frame = 0x40;
		break;
	case 0x208:
		e->frame = 0x16e;
		break;
	case SKELETON_PATH:
		e->frame = 0x24;
		path_follow_turned(e, skeleton_path, e->path, 1.0f);
		break;
	case 0x802:
		e->frame = 0x6b;
		break;
	case 4:
		e->frame = 0;
		break;
	case SKELETON_IDLE:
	default:
		e->frame = 0;
		break;
	}
}

/* 0x8013e6cc: go for the player: keep a playing target, steer to a point in front of the camera
   and pick the state by how close it got (0 still far: walk, 1 there: turn to it, 2 in reach: a
   taunt and one of three attacks, or a turn) */
void skeleton_approach(enemy_t *e)
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
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0.0f, -14.0f);
	r = step_toward_goal_xz(e, 1.6f);
	switch(r)
	{
	case 1:
		if(e->ai_state == SKELETON_CLOSE)
		{
			e->frame = 0x1a;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, skeleton_anim_blob, 5, 0x1a, 1);
		e->ai_state = SKELETON_CLOSE;
		break;
	case 2:
		if(e->frame > 0)
		{
			e->frame--;
		}
		play_skeleton_sound(skeleton_taunts[randrng(8)], 0);
		r = randrng(100);
		if(r < 25)
		{
			blend_to_frame(e, skeleton_anim_blob, 5, 0x2f, 1);
			e->ai_state = 0x201;
		}
		else if(r < 50)
		{
			blend_to_frame(e, skeleton_anim_blob, 5, 0x161, 1);
			e->ai_state = 0x206;
		}
		else if(r < 75)
		{
			blend_to_frame(e, skeleton_anim_blob, 5, 0x19c, 1);
			e->ai_state = 0x207;
		}
		else
		{
			blend_to_frame(e, skeleton_anim_blob, 5, 0x1a, 1);
			e->ai_state = SKELETON_CLOSE;
		}
		break;
	case 0:
		if(e->ai_state == SKELETON_READY)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, skeleton_anim_blob, 5, 0x24, 1);
		e->ai_state = SKELETON_READY;
		break;
	}
}

/* 0x8013e924: at the end of its path or of some attacks: a taunt; the spawn entry's 2 makes it
   stand, 0x202 starts that attack, else it goes for the player */
void skeleton_end_path(enemy_t *e)
{
	play_skeleton_sound(skeleton_taunts[randrng(8)], 0);
	switch((unsigned short)e->unk6)
	{
	case 2:
		e->frame--;
		e->unk6 = 0;
		blend_to_frame(e, skeleton_anim_blob, 5, 0x1a, 1);
		e->ai_state = SKELETON_STAND;
		break;
	case 0x202:
		e->frame--;
		e->unk6 = 0;
		blend_to_frame(e, skeleton_anim_blob, 5, 0x81, 1);
		e->ai_state = 0x202;
		break;
	default:
		e->unk6 = 0;
		skeleton_approach(e);
		break;
	}
}
