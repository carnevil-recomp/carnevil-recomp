/*
 * mari.c - Evil Marie (her life bar's name picture, texture 0x7082), the Haunted House boss with
 * the axe (object type 0x1104; MARI.ZM, mari.za and
 * the mari*.WMS texmaps; the axe AXE.ZM with mariaxe/marihndl.WMS; sound bank "chastity"), and
 * the axe she throws (object type 0x2010). One original file, 0x800ed1e0-0x800ef870 (the axe
 * first, then Mari, then the boss fight's callbacks the Haunted House movie script calls),
 * rodata 0x80171018-0x80171148, tables at 0x8018e058-0x8018ea60 (mari_data.c and below). The
 * model has 1540 vertices, 1573 normals and 2674 faces in 31 submeshes, the animation 328
 * frames; the axe 25 vertices, 30 normals, 32 faces. She walks her own path (D_80183148,
 * spawn_paths_data.c) in legs the script starts, swings at the player (states 0x201-0x204,
 * hurting on one frame each if her axe arm's group 15 is near), throws the axe on frame 0x17 of
 * her throwing walks (0x803, 0x80d), flashes when shot (a light for six frames), shows a life
 * bar, loses her parts at 65%, 50% and 25% of her health and the script ends the fight below 13%.
 *
 * States: 0 idle, 0x80b waiting (the glow of 0x80114df0, the flash file), 0x801-0x80a and
 * 0x81d/0x81e the script's walks, 0x201-0x204 the attacks, 0x1001 a flinch, 0x8101 fading at
 * 65%, 0x8102 gone, 0x2001 the script's death (blood and chunks from the neck), 0x2002 shot dead,
 * 0x4000 dead.
 *
 * The exported names are the address names the shared files (spawn.c, wave.c, the movie
 * scripts) use: something_mari_anim is the draw callback, func_800edff0 the init,
 * func_800ee228 the think, func_800eeeec the state set-up, sub_800eef68..sub_800ef7b0 the
 * script's callbacks.
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

extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	func_8011464c(enemy_t *, submesh_t *, int);	/* model.c: the groups placed at the current frame */
extern void	func_80114df0(enemy_t *);			/* the flash file: Mari's glow while she waits */
extern void	func_800fc550(float *, float *, float *);	/* enemy.c: the heading towards a point */
extern void	func_800d326c(void *, void *, void *);		/* a screen triangle */
extern void	init_groups(enemy_t *, submesh_t *, int);
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health2(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern player_stats_t *attack_target(enemy_t *);
extern int	randrng(int);
extern float	fsqrt(float);
extern void	path_place(enemy_t *, float *, int, float);
extern void	animate_loose_groups(enemy_t *, int);
extern int	face_camera(enemy_t *, float);
extern int	group_near_camera(group_t *, float);
extern int	near_camera(enemy_t *, float);
extern void	aim_at_camera(enemy_t *, float, float *);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	add_world_sprite(float *, int, int);
extern void	burst_blood(float *, float, int);		/* the original passes a third word the routine ignores */
extern void	add_chunks(float *, float);
extern float	shot_damage_shared(player_stats_t *);
extern void	award_points(int, int);
extern int	increment_audit(int);
extern short	remove_game_object(short);
extern void	point_toward_camera(float *, float, float *);
extern int	add_dynamic_light(int, float *, float *, float, int);
extern int	remove_light(int);
extern void	draw_life_bar(float, float, float, float, float, unsigned int);
extern unsigned int next_cue_priority(void);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *);
extern enemy_t	*debug_enemy;
extern int	violence_level;
extern int	D_80212db0, D_80212dbc, D_80212dc8, D_80212dd0, D_80212dd4;	/* the movie script's variables */

extern texmap_t	mari_texmap[43];		/* mari_data.c */
extern submesh_t mari_anim_blob[31];
extern texmap_t	D_8018e998[2];			/* the axe */
extern submesh_t something_mari_weap_anim[1];
extern float	D_80183148[][6];		/* spawn_paths_data.c: her path */

/* float words of the record: the fade rate while dying (0x44), the thrown axe's spin (0xa4) */
#define FADE(e)		(*(float *)&(e)->unk44)
#define SPIN(e)		(*(float *)&(e)->unka4)

float	*mari_verts;			/* 0x801e5938 */
face_t	*mari_faces;			/* 0x80212e74 */
float	*mari_normals;			/* 0x801e4394 */
float	*axe_verts;			/* 0x801e09ec */
face_t	*axe_faces;			/* 0x801e6ecc */
float	*axe_normals;			/* 0x801e0968 */
enemy_t	*D_801e0ea4;			/* Mari's record, for the script's callbacks */
float	D_801e0f38;			/* her full health */
int	D_801e0f08;
int	D_802373c8;			/* shots taken: every tenth during an attack makes her flinch */
int	D_8018e9f8[5] = { 14, 15, 16, 17, 18 };	/* her flinch cries */
float	D_8018ea0c[7][3] =		/* the flash a shot lights on her, frame by frame */
{
	{ 0.0f, 0.0f, 255.0f }, { 0.0f, 0.0f, 200.0f }, { 0.0f, 0.0f, 150.0f }, { 0.0f, 0.0f, 110.0f },
	{ 0.0f, 0.0f, 80.0f }, { 0.0f, 0.0f, 40.0f }, { 0.0f, 0.0f, 0.0f },
};

void	func_800ed358(game_object_t *);
int	func_800ed4c0(game_object_t *);
void	func_800ed654(game_object_t *);
int	func_800ed454(game_object_t *, player_stats_t *);
void	func_800ed61c(game_object_t *);
int	func_800edd30(game_object_t *, player_stats_t *);
void	func_800ee1d8(game_object_t *);
void	func_800eeeec(enemy_t *, unsigned short);
void	func_800ef110(float, float);
void	func_800ef220(enemy_t *);

/* ---- the axe (object 0x2010) ---- */

/* 0x800ed1e0: Mari throws the axe: from her hand (group 7) at the camera, spinning, with the
   time it takes to arrive */
void func_800ed1e0(enemy_t *m)
{
	short	id;
	enemy_t	*e;
	float	speed, d, t, spin;
	float	*cam;

	id = add_object1_wrapper(0x2010, func_800ed358, func_800ed4c0, func_800ed654);
	if(id)
	{
		e = get_object_data(id);
		e->x = m->groups[7].pos[0];
		speed = 3.5f;
		e->y = m->groups[7].pos[1];
		e->z = m->groups[7].pos[2];
		e->counter = 0x78;
		aim_at_camera(e, speed, e->goal);
		cam = &camera_x;
		d = fsqrt((e->x - cam[0]) * (e->x - cam[0]) + (e->y - camera_y) * (e->y - camera_y) + (e->z - camera_z) * (e->z - camera_z));
		e->target = get_target_player();
		t = d / speed;
		spin = 13.0899696f / t;
		e->unk98 = m->y - 30.0f;
		e->rx = 0;
		e->ry = e->rz = e->rx;
		e->unk9c = d;
		e->alpha = 255.0f;
		e->unka0 = t;
		SPIN(e) = spin;
		func_800fc550(cam, &e->x, &e->rx);
	}
}

/* 0x800ed358: the axe's draw callback */
void func_800ed358(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, something_mari_weap_anim, 1, axe_verts, 0x19, axe_faces, 0x20, axe_normals, 0x1e);
}

/* 0x800ed3bc */
void func_800ed3bc(game_object_t *obj, player_stats_t *p, float damage, enemy_t *e)
{
	e->counter = 0;
	e->killer = p;
}

/* 0x800ed3c8: the axe's hit test: it can be shot out of the air */
int func_800ed3c8(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(&e->x, e->radius * e->radius, p))
	{
		p->hit = 1;
		p->hit_id = obj->id;
		p->hit_group = 0;
		p->die = (void *)func_800ed454;
		p->hit_point[0] = p->aim[0];
		p->hit_point[1] = p->aim[1];
		p->hit_point[2] = p->aim[2];
	}
	return(0);
}

/* 0x800ed454: the axe shot: 500 points, a clang, a spark, gone */
int func_800ed454(game_object_t *obj, player_stats_t *p)
{
	award_points(p->index, 500);
	play_rotating_cue(0x95, "generic1");
	add_world_sprite(p->hit_point, 9, 0);
	remove_game_object(obj->id);
	return(0);
}

/* 0x800ed4c0: the axe's init callback */
int func_800ed4c0(game_object_t *obj)
{
	enemy_t	*e;
	group_t	*g;
	int	i;

	obj->data = malloc(sizeof(enemy_t));
	if(!obj->data)
	{
		return(1);
	}
	obj->hit = func_800ed3c8;
	obj->die = func_800ed454;
	obj->remove = func_800ed61c;
	e = (enemy_t *)obj->data;
	e->counter = 0x1e;
	e->unk4 = 0x14;
	e->ai_state = 1;
	e->num = 0;
	e->flags = 0xc;
	e->rx = 0;
	e->ry = 0;
	e->rz = 0;
	e->x = 0;
	e->y = 0;
	e->z = 0;
	e->unkcc = 0.13f;
	set_enemy_scale(e, 1.0f);
	e->groups = malloc(sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < 1; i++)
	{
		e->groups[i].flags = 0x9000;
		e->groups[i].rot[0] = 3.1415927f;
		e->groups[i].rot[1] = 3.6651914f;
		e->groups[i].rot[2] = 3.1415927f;
	}
	init_groups(e, something_mari_weap_anim, 1);
	g = e->groups;
	e->hit_group = g;
	g->floor = 0;
	return(0);
}

/* 0x800ed61c: the axe's remove callback */
void func_800ed61c(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	free(e->groups);
	free(e);
}

/* 0x800ed654: the axe's think callback: in flight it spins and flies until it reaches the
   camera (the player is hit) or its time runs out; state 2 (knocked away) falls, bounces off
   the floor and fades */
void func_800ed654(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	switch(e->ai_state)
	{
	case 1:
		e->hit_group->rot[0] += SPIN(e);
		e->x += e->goal[0];
		e->y += e->goal[1];
		e->z += e->goal[2];
		if(near_camera(e, 3.5f))
		{
			remove_game_object(obj->id);
			hurt_player(attack_target(e), 100, 7, 0.0f);
		}
		else if(--e->counter == 0)
		{
			remove_game_object(obj->id);
		}
		break;
	case 2:
		e->x += e->goal[0];
		e->y += e->goal[1];
		e->z += e->goal[2];
		e->goal[1] += -0.23f;
		if(e->y < e->unk98)
		{
			e->y = e->unk98;
			e->goal[1] *= -0.5f;
		}
		e->alpha -= 5.0f;
		if(e->alpha <= 0.0f)
		{
			remove_game_object(obj->id);
		}
		break;
	}
}

/* ---- Mari (object 0x1104) ---- */

/* 0x800ed7e8: the draw callback: the shot flash, the glow while she waits, the model, her
   life bar while she is alive */
void something_mari_anim(game_object_t *obj)
{
	enemy_t	*e;
	float	pt[3];
	int	light;
	int	lit;

	e = (enemy_t *)obj->data;
	light = 0;
	if((unsigned short)e->unk4 < 6)
	{
		point_toward_camera(e->hit_group->pos, 1.0f, pt);
		lit = 1;
		light = add_dynamic_light(0x11, pt, D_8018ea0c[(unsigned short)e->unk4], 3.0f, 0xf);
	}
	else
	{
		lit = 0;
	}
	if(e->ai_state == 0x80b)
	{
		func_80114df0(e);
	}
	draw_model(e, mari_anim_blob, 0x1f, mari_verts, 0x604, mari_faces, 0xa72, mari_normals, 0x625);
	if(lit == 1)
	{
		remove_light(light);
	}
	if(!(e->ai_state & 0x6000))
	{
		draw_life_bar(e->health, D_801e0f38, 100.0f, 300.0f, 248.0f, 0xff90ff);
		func_800ef110(193.0f, 233.0f);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->hit_group->pos, e->radius, 0x600000);
	}
	if(debug_mode & 0x20)
	{
		guAlphaSource(1);
		guColorCombineFunction(1);
		grConstantColorValue(0xffffffff);
		grAlphaBlendFunction(1, 5, 1, 5);
		func_800d14a4(e->x * 0.12f + 200.0f, e->z * 0.12f + 128.0f);
		grAlphaBlendFunction(4, 0, 4, 0);
	}
	if(debug_mode & 0x1000)
	{
		func_800c99e4(e, 0xff8080);
	}
}

/* 0x800eda1c: the damage: below 65% her part 18 goes, she cries and starts to fade (0x8101);
   below 50% part 19 goes and three parts are marked; below 25% two parts are shot off at the
   highest violence; at 0 she dies (2000 points), else 50 points */
void func_800eda1c(game_object_t *obj, player_stats_t *p, float damage, enemy_t *e)
{
	float	health;

	if(e->ai_state & 0x6000)
	{
		return;
	}
	health = e->health;
	e->health = health - damage;
	if(D_801e0f38 * 0.65f < health && e->health < D_801e0f38 * 0.65f)
	{
		D_801e0ea4->groups[18].flags = 0;
		play_enemy_sound(e, 8, "chastity", 4);
		blend_to_frame(D_801e0ea4, mari_anim_blob, 9, 0x3f, 2);
		e->ai_state = 0x8101;
		FADE(e) = -10.0f;
	}
	if(D_801e0f38 * 0.5f < health && e->health < D_801e0f38 * 0.5f)
	{
		D_801e0ea4->groups[19].flags = 0;
		D_801e0ea4->groups[11].flags |= 0x1000000;
		D_801e0ea4->groups[3].flags |= 0x1000000;
		D_801e0ea4->groups[29].flags |= 0x1000000;
		play_enemy_sound(e, 7, "chastity", 4);
	}
	if(D_801e0f38 * 0.25f < health && e->health < D_801e0f38 * 0.25f)
	{
		if(violence_level >= 3)
		{
			D_801e0ea4->groups[30].flags |= 0xf0000000;
			D_801e0ea4->groups[16].flags |= 0xf0000000;
		}
		play_enemy_sound(e, 6, "chastity", 4);
	}
	if(e->health <= 0.0f)
	{
		increment_audit(0x39);
		e->ai_state = 0x2002;
		award_points(p->index, 2000);
		e->counter = 0;
		e->killer = p;
	}
	else
	{
		award_points(p->index, 50);
	}
}

/* 0x800edc68: the hit test (not once she has faded away) */
int func_800edc68(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(e->ai_state == 0x8102)
	{
		return(0);
	}
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, mari_anim_blob, 0x1f, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)func_800edd30;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x800edd30: the die callback (the shot lands): the damage, then a spark (and blood once she
   is below a quarter of her health) or the flash; every tenth shot during an attack makes her
   flinch with a cry */
int func_800edd30(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return(0);
	}
	func_800eda1c(obj, p, shot_damage_shared(p), e);
	if(D_801e0f38 * 0.25f < e->health || !violence_level)
	{
		e->unk4 = 0;
		if(!violence_level)
		{
			add_world_sprite(p->hit_point, 0, 0);
		}
	}
	else
	{
		add_world_sprite(p->hit_point, 0, 0);
		burst_blood(p->hit_point, e->y, 0xa00000);
	}
	if(++D_802373c8 % 10 == 0)
	{
		switch(e->ai_state)
		{
		case 0x201:
		case 0x202:
		case 0x203:
		case 0x204:
			e->resume_frame = e->frame;
			e->resume_state = e->ai_state;
			e->ai_state = 0x1001;
			e->frame = 0x25;
			play_enemy_sound(e, D_8018e9f8[randrng(5)], "chastity", 2);
			break;
		}
	}
	return(0);
}

/* 0x800eded8, 0x800edf20: the texmaps (hers and the axe's), the models and the animation */
void load_mari_texmap(void)
{
	load_texmap(mari_texmap, 0x2b, "mari");
	load_texmap(D_8018e998, 2, "axe");
}

void load_mari(void)
{
	load_model((void **)&mari_verts, 0x604, (void **)&mari_faces, 0xa72, (void **)&mari_normals, 0x625, "MARI.ZM", 0xe88e9bb7);
	load_model((void **)&axe_verts, 0x19, (void **)&axe_faces, 0x20, (void **)&axe_normals, 0x1e, "AXE.ZM", 0x7e44762d);
	load_anim(mari_anim_blob, 0x1f, 0x148, "mari.za", 0x0c975878);
}

/* 0x800edff0: the init callback: her record (kept in D_801e0ea4 for the script), full alpha,
   the groups (three of them flagged 8), 300 health scaled by difficulty, the waiting state */
int func_800edff0(game_object_t *obj)
{
	enemy_t	*e;
	group_t	*g;
	int	i;

	obj->data = malloc(sizeof(enemy_t));
	if(!obj->data)
	{
		return(1);
	}
	obj->hit = func_800edc68;
	obj->die = func_800edd30;
	obj->remove = func_800ee1d8;
	e = (enemy_t *)obj->data;
	e->target = get_target_player();
	D_801e0f08 = 0;
	e->unk98 = 90.0199966f;
	e->counter = 0x1e;
	e->unk4 = 0x14;
	D_801e0ea4 = e;
	e->frame = 0;
	e->ai_state = 0;
	e->num = 0;
	e->flags = 0xc;
	e->rx = 0;
	e->ry = 0;
	e->rz = 0;
	e->x = 0;
	e->y = 0;
	e->z = 0;
	e->unkcc = 1.1f;
	set_enemy_scale(e, 1.0f);
	e->alpha = 255.0f;
	FADE(e) = 10.0f;
	e->groups = malloc(0x1f * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < 0x1f; i++)
	{
		e->groups[i].flags = 0xa080;
		e->groups[i].alpha = e->alpha;
	}
	e->groups[19].flags |= 8;
	e->groups[18].flags |= 8;
	e->groups[17].flags |= 8;
	func_8011464c(e, mari_anim_blob, 0x1f);
	g = e->groups;
	e->hit_group = &g[9];
	g[9].floor = 0;
	set_enemy_health2(e, 300.0f);
	e->prio_base = 0xf;
	D_801e0f38 = e->health;
	e->prio_flags = next_cue_priority();
	func_800eeeec(e, 0x80b);
	e->frame = 0x11b;
	return(0);
}

/* 0x800ee1d8: the remove callback */
void func_800ee1d8(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(debug_enemy == e)
	{
		debug_enemy = 0;
	}
	free(e->groups);
	free(e);
}

/* 0x800ee228: the think callback */
void func_800ee228(game_object_t *obj)
{
	enemy_t	*e;
	int	i;

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		return;
	}
	e->unk4++;
	animate_loose_groups(e, 0x1f);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x801:
		if(++e->frame >= 0xd4)
		{
			e->frame = 0xc4;
		}
		path_place(e, D_80183148[0], e->path, 1.0f);
		e->ry += 3.1415927f;
		if(++e->path > e->unk92)
		{
			func_800ef220(e);
			e->unk6 = 0x802;
		}
		break;
	case 0x802:
		if(++e->frame >= 0x11b)
		{
			e->frame = 0x11b;
		}
		path_place(e, D_80183148[0], e->path, 1.0f);
		e->ry += 3.1415927f;
		if(++e->path > e->unk92)
		{
			e->ai_state = 0x803;
			e->path = 0x48;
			e->unk90 = 0x48;
			e->unk92 = 0x84;
			e->frame = 0x13;
		}
		break;
	case 0x804:
		if(++e->frame >= 0xd4)
		{
			e->frame = 0xc4;
		}
		path_place(e, D_80183148[0], e->path, 1.0f);
		e->ry += 3.1415927f;
		if(++e->path > e->unk92)
		{
			func_800ef220(e);
		}
		break;
	case 0x805:
		if(++e->frame >= 0x13)
		{
			e->frame = 0;
		}
		path_place(e, D_80183148[0], e->path, 1.0f);
		e->ry += 3.1415927f;
		if(++e->path > e->unk92)
		{
			blend_to_frame(D_801e0ea4, mari_anim_blob, 9, 0x13, 2);
			e->ai_state = 0x80d;
			e->path = 0x1bf;
			e->unk90 = 0x1bf;
			e->unk92 = 0x1e6;
		}
		break;
	case 0x806:
		if(++e->frame >= 0xd4)
		{
			e->frame = 0xc4;
		}
		path_place(e, D_80183148[0], e->path, 1.0f);
		e->ry += 3.1415927f;
		if(++e->path > e->unk92)
		{
			blend_to_frame(D_801e0ea4, mari_anim_blob, 9, 0x7b, 2);
			e->ai_state = 0x80e;
			e->path = 0x1e7;
			e->unk90 = 0x1e7;
			e->unk92 = 0x233;
		}
		break;
	case 0x807:
		if(++e->frame >= 0xd4)
		{
			e->frame = 0xc4;
		}
		path_place(e, D_80183148[0], e->path, 1.0f);
		e->ry += 3.1415927f;
		if(++e->path > e->unk92)
		{
			func_800ef220(e);
		}
		break;
	case 0x808:
	case 0x809:
	case 0x80a:
		if(++e->frame >= 0x11b)
		{
			e->frame = 0x11b;
		}
		path_place(e, D_80183148[0], e->path, 1.0f);
		e->ry += 3.1415927f;
		if(++e->path > e->unk92)
		{
			play_enemy_sound(e, 0x15, "chastity", 4);
			blend_to_frame(D_801e0ea4, mari_anim_blob, 9, 0x11b, 2);
			e->ai_state = 0x80b;
			e->counter = 0;
			e->path = 0x1bc;
			e->unk90 = 0x1bc;
			e->unk92 = 0x1bc;
		}
		break;
	case 0x80b:
		e->frame++;
		e->counter++;
		if(e->frame >= 0x148)
		{
			e->frame = 0x11b;
			e->counter = 0;
		}
		break;
	case 0x2002:
		if(++e->frame >= 0x148)
		{
			e->frame = 0x11b;
		}
		break;
	case 0x803:
	case 0x80d:
		if(++e->frame >= 0x25)
		{
			e->frame = 0x13;
		}
		if(e->frame == 0x17)
		{
			func_800ed1e0(e);
		}
		goto walk_loop;
	case 0x81d:
		if(++e->frame >= 0x13)
		{
			e->frame = 0;
		}
		path_place(e, D_80183148[0], e->path, 1.0f);
		e->ry += 3.1415927f;
		if(++e->path > e->unk92)
		{
			blend_to_frame(D_801e0ea4, mari_anim_blob, 9, 0xc4, 2);
			D_801e0ea4->path = 0x134;
			D_801e0ea4->ai_state = 0x806;
			D_801e0ea4->unk90 = 0x134;
			D_801e0ea4->unk92 = 0x15b;
			path_place(D_801e0ea4, D_80183148[0], D_801e0ea4->path, 1.0f);
			D_801e0ea4->ry += 3.1415927f;
		}
		break;
	case 0x80e:
		if(++e->frame >= 0x97)
		{
			e->frame = 0x7b;
		}
	walk_loop:
		path_place(e, D_80183148[0], e->path, 1.0f);
		e->ry += 3.1415927f;
		if(++e->path > e->unk92)
		{
			e->path = e->unk90;
		}
		break;
	case 0x81e:
		if(++e->frame >= 0x13)
		{
			e->frame = 0;
		}
		path_place(e, D_80183148[0], e->path, 1.0f);
		e->ry += 3.1415927f;
		if(++e->path > e->unk92)
		{
			blend_to_frame(D_801e0ea4, mari_anim_blob, 9, 0xc4, 2);
			D_801e0ea4->path = 0x15c;
			D_801e0ea4->ai_state = 0x807;
			D_801e0ea4->unk90 = 0x15c;
			D_801e0ea4->unk92 = 0x175;
			path_place(D_801e0ea4, D_80183148[0], D_801e0ea4->path, 1.0f);
			D_801e0ea4->ry += 3.1415927f;
		}
		break;
	case 0:
		if(++e->frame >= 0x148)
		{
			e->frame = 0;
		}
		break;
	case 0x1001:
		if(++e->frame >= 0x3f)
		{
			e->frame = e->resume_frame;
			e->ai_state = e->resume_state;
		}
		break;
	case 0x2001:
		if(++e->frame == 0xff)
		{
			burst_blood(e->groups[29].pivot, e->y - 20.0f, 0xa00000);
			burst_blood(e->groups[29].pivot, e->y - 20.0f, 0xa00000);
			add_chunks(e->groups[29].pivot, e->y - 20.0f);
			add_chunks(e->groups[29].pivot, e->y - 20.0f);
		}
		if(e->frame >= 0x110)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		break;
	case 0x8101:
		if(++e->frame >= 0x67)
		{
			e->frame = 0x3f;
		}
		e->alpha += FADE(e);
		if(e->alpha < 0.0f)
		{
			e->alpha = 0.0f;
			e->ai_state = 0x8102;
			e->counter = 0;
		}
		for(i = 0; i < 0x1f; i++)
		{
			e->groups[i].alpha = e->alpha;
		}
		break;
	case 0x201:
		face_camera(e, 0.2f);
		if(++e->frame == 0x73 && group_near_camera(&e->groups[15], 30.0f))
		{
			hurt_player(e->target, 0xaf, 7, 0.0f);
		}
		if(e->frame >= 0x7b)
		{
			func_800ef220(e);
		}
		break;
	case 0x202:
		face_camera(e, 0.2f);
		if(++e->frame == 0x88 && group_near_camera(&e->groups[15], 30.0f))
		{
			hurt_player(e->target, 0xaf, 7, 0.0f);
		}
		if(e->frame >= 0x97)
		{
			func_800ef220(e);
		}
		break;
	case 0x203:
		face_camera(e, 0.2f);
		if(++e->frame == 0xa3 && group_near_camera(&e->groups[15], 30.0f))
		{
			hurt_player(e->target, 0xaf, 7, 3.1415927f);
		}
		if(e->frame >= 0xb0)
		{
			func_800ef220(e);
		}
		break;
	case 0x204:
		face_camera(e, 0.2f);
		if(++e->frame == 0xbb && group_near_camera(&e->groups[15], 30.0f))
		{
			hurt_player(e->target, 0xaf, 3, 0.0f);
		}
		if(e->frame >= 0xc4)
		{
			func_800ef220(e);
		}
		break;
	case 0x4000:
		break;
	}
}

/* 0x800eee20 */
void remove_mari(void)
{
	remove_surface(D_8018e998, 2);
	remove_surface(mari_texmap, 0x2b);
	remove_animation(mari_anim_blob, 0x1f, 0x148, "mari.za");
	remove_model((void **)&axe_verts, 0x19, (void **)&axe_faces, 0x20, (void **)&axe_normals, 0x1e, "AXE.ZM");
	remove_model((void **)&mari_verts, 0x604, (void **)&mari_faces, 0xa72, (void **)&mari_normals, 0x625, "MARI.ZM");
}

/* 0x800eeeec: the state set-up: 0x801 (the script's first walk) counts in the audits and
   starts her at the head of her path, turned round */
void func_800eeeec(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	if(state == 0x801)
	{
		increment_audit(0x38);
		e->frame = 0xc4;
		e->path = 0;
		path_place(e, D_80183148[0], 0, 1.0f);
		e->ry += 3.1415927f;
		e->counter = 0;
	}
}

/* ---- the Haunted House script's callbacks for the fight ---- */

/* 0x800eef68: 1 while she has health left */
int sub_800eef68(void)
{
	if(D_801e0ea4->health > 0.0f)
	{
		return(1);
	}
	return(0);
}

/* 0x800eef94: 1 below violence level 2 */
int sub_800eef94(void)
{
	return(violence_level < 2);
}

/* 0x800eefa4: the script's death: her last cry, on her death walk at its fixed place */
void sub_800eefa4(void)
{
	D_801e0ea4->ai_state = 0x2001;
	D_801e0ea4->frame = 0xd4;
	play_enemy_sound(D_801e0ea4, 9, "chastity", 8);
	D_801e0ea4->path = 0x1bd;
	D_801e0ea4->unk90 = 0x1bd;
	D_801e0ea4->unk92 = 0x1be;
	path_place(D_801e0ea4, D_80183148[0], D_801e0ea4->path, 1.0f);
	D_801e0ea4->ry += 3.1415927f;
	D_801e0ea4->x = -1020.83398f;
	D_801e0ea4->z = 196.945999f;
}

/* 0x800ef048, 0x800ef07c: the death pose at two other places */
void sub_800ef048(void)
{
	D_801e0ea4->ai_state = 0x2001;
	D_801e0ea4->frame = 0xf2;
	D_801e0ea4->x = -1019.73401f;
	D_801e0ea4->z = 199.845993f;
}

void sub_800ef07c(void)
{
	D_801e0ea4->ai_state = 0x2001;
	D_801e0ea4->frame = 0xf2;
	D_801e0ea4->x = -1018.83398f;
	D_801e0ea4->z = 197.345993f;
}

/* 0x800ef0b0: 1 while the death plays */
int func_800ef0b0(void)
{
	return(D_801e0ea4->ai_state == 0x2001);
}

/* 0x800ef0c8: back to full alpha */
void sub_800ef0c8(void)
{
	int	i;

	D_801e0ea4->alpha = 255.0f;
	for(i = 0; i < 0x1f; i++)
	{
		D_801e0ea4->groups[i].alpha = D_801e0ea4->alpha;
	}
}

/* 0x800ef110: the picture beside her life bar (texture 0x7082), 128 by 16 around x, y */
void func_800ef110(float x, float y)
{
	struct { float x, y, oow, sow, tow, pad; } v[4];

	guTexSource(find_surface(0x7082)->handle);
	guColorCombineFunction(4);
	guAlphaSource(2);
	grAlphaTestReferenceValue(4);
	v[0].x = x - 64.0f;
	v[0].y = y - 8.0f;
	v[0].oow = 0.909090877f;
	v[0].sow = 0;
	v[0].tow = 29.0909081f;
	v[1].x = x + 64.0f;
	v[1].y = y - 8.0f;
	v[1].oow = 0.909090877f;
	v[1].sow = 231.818176f;
	v[1].tow = 29.0909081f;
	v[2].x = x - 64.0f;
	v[2].y = y + 8.0f;
	v[2].oow = 0.909090877f;
	v[2].sow = 0;
	v[2].tow = 0;
	v[3].x = x + 64.0f;
	v[3].y = y + 8.0f;
	v[3].oow = 0.909090877f;
	v[3].sow = 231.818176f;
	v[3].tow = 0;
	func_800d326c(&v[0], &v[3], &v[1]);
	func_800d326c(&v[0], &v[2], &v[3]);
}

/* 0x800ef220: pick one of the four swings at random (a quarter each) and go for the player */
void func_800ef220(enemy_t *e)
{
	int	r;
	int	state;
	int	frame;

	r = randrng(100);
	if(r < 25)
	{
		state = 0x201;
		frame = 0x67;
	}
	else if(r < 50)
	{
		state = 0x202;
		frame = 0x7b;
	}
	else if(r < 75)
	{
		state = 0x203;
		frame = 0x97;
	}
	else
	{
		state = 0x204;
		frame = 0xb0;
	}
	e->target = get_target_player();
	if(e->frame > 0)
	{
		e->frame--;
	}
	blend_to_frame(e, mari_anim_blob, 9, frame, 2);
	e->ai_state = state;
}

/* 0x800ef2e8, 0x800ef2fc: a script timer; it stops and part 17 goes once she is below 75% */
void sub_800ef2e8(void)
{
	D_80212db0 = 0x82;
}

void sub_800ef2fc(void)
{
	if(D_801e0ea4->health < D_801e0f38 * 0.75f)
	{
		D_80212db0 = 0;
		D_801e0ea4->groups[17].flags = 0;
	}
}

/* 0x800ef344: the walk 0x802 (path points 0x1a-0x47) */
void sub_800ef344(void)
{
	blend_to_frame(D_801e0ea4, mari_anim_blob, 9, 0x110, 2);
	D_801e0ea4->path = 0x1a;
	D_801e0ea4->ai_state = 0x802;
	D_801e0ea4->unk90 = 0x1a;
	D_801e0ea4->unk92 = 0x47;
	path_place(D_801e0ea4, D_80183148[0], D_801e0ea4->path, 1.0f);
	D_801e0ea4->ry += 3.1415927f;
}

/* 0x800ef3d0, 0x800ef3e4: a script timer that stops when she has faded or died */
void sub_800ef3d0(void)
{
	D_80212dbc = 0x78;
}

void sub_800ef3e4(void)
{
	if(D_801e0ea4->ai_state == 0x8102)
	{
		D_80212dbc = 0;
	}
	if(D_801e0ea4->ai_state == 0x2002)
	{
		D_80212dbc = 0;
	}
}

/* 0x800ef420: the walk 0x804 (path points 0x85-0x10d) */
void sub_800ef420(void)
{
	D_801e0ea4->ai_state = 0x804;
	D_801e0ea4->path = 0x85;
	D_801e0ea4->unk90 = 0x85;
	D_801e0ea4->unk92 = 0x10d;
	D_801e0ea4->frame = 0xc4;
	D_801e0ea4->flags &= 0xfffd;
}

/* 0x800ef45c, 0x800ef470: a script counter */
void sub_800ef45c(void)
{
	D_80212dc8 = 0x14;
}

void sub_800ef470(void)
{
	D_80212dc8--;
}

/* 0x800ef48c: the throwing walk 0x805 (path points 0x10e-0x133) */
void sub_800ef48c(void)
{
	blend_to_frame(D_801e0ea4, mari_anim_blob, 9, 0, 2);
	D_801e0ea4->path = 0x10e;
	D_801e0ea4->ai_state = 0x805;
	D_801e0ea4->unk90 = 0x10e;
	D_801e0ea4->unk92 = 0x133;
	path_place(D_801e0ea4, D_80183148[0], D_801e0ea4->path, 1.0f);
	D_801e0ea4->ry += 3.1415927f;
}

/* 0x800ef518: below 13% of her health: the walk 0x808 (path points 0x176-0x18e); 1 when it
   started */
int sub_800ef518(void)
{
	if(D_801e0ea4->health < D_801e0f38 * 0.13f)
	{
		blend_to_frame(D_801e0ea4, mari_anim_blob, 9, 0x110, 2);
		D_801e0ea4->path = 0x176;
		D_801e0ea4->ai_state = 0x808;
		D_801e0ea4->unk90 = 0x176;
		D_801e0ea4->unk92 = 0x18e;
		path_place(D_801e0ea4, D_80183148[0], D_801e0ea4->path, 1.0f);
		D_801e0ea4->ry += 3.1415927f;
		return(1);
	}
	return(0);
}

/* 0x800ef5d8, 0x800ef5ec: a countdown; near its end her throwing walk 0x80d turns into 0x81d */
void sub_800ef5d8(void)
{
	D_80212dd0 = 10;
}

void sub_800ef5ec(void)
{
	if(--D_80212dd0 < 2 && D_801e0ea4->ai_state == 0x80d)
	{
		D_801e0ea4->ai_state = 0x81d;
		blend_to_frame(D_801e0ea4, mari_anim_blob, 9, 0, 2);
	}
}

/* 0x800ef658 */
void sub_800ef658(void)
{
}

/* 0x800ef660: as 0x800ef518 with the walk 0x809 (path points 0x18f-0x1a7) */
int sub_800ef660(void)
{
	if(D_801e0ea4->health < D_801e0f38 * 0.13f)
	{
		blend_to_frame(D_801e0ea4, mari_anim_blob, 9, 0x110, 2);
		D_801e0ea4->path = 0x18f;
		D_801e0ea4->ai_state = 0x809;
		D_801e0ea4->unk90 = 0x18f;
		D_801e0ea4->unk92 = 0x1a7;
		path_place(D_801e0ea4, D_80183148[0], D_801e0ea4->path, 1.0f);
		D_801e0ea4->ry += 3.1415927f;
		return(1);
	}
	return(0);
}

/* 0x800ef720, 0x800ef734: a countdown; near its end her walk 0x80e turns into 0x81e */
void sub_800ef720(void)
{
	D_80212dd4 = 7;
}

void sub_800ef734(void)
{
	if(--D_80212dd4 < 2 && D_801e0ea4->ai_state == 0x80e)
	{
		blend_to_frame(D_801e0ea4, mari_anim_blob, 9, 0, 2);
		D_801e0ea4->ai_state = 0x81e;
	}
}

/* 0x800ef7a8 */
void sub_800ef7a8(void)
{
}

/* 0x800ef7b0: as 0x800ef518 with the walk 0x80a (path points 0x1a8-0x1bb) */
int sub_800ef7b0(void)
{
	if(D_801e0ea4->health < D_801e0f38 * 0.13f)
	{
		blend_to_frame(D_801e0ea4, mari_anim_blob, 9, 0x110, 2);
		D_801e0ea4->path = 0x1a8;
		D_801e0ea4->ai_state = 0x80a;
		D_801e0ea4->unk90 = 0x1a8;
		D_801e0ea4->unk92 = 0x1bb;
		path_place(D_801e0ea4, D_80183148[0], D_801e0ea4->path, 1.0f);
		D_801e0ea4->ry += 3.1415927f;
		return(1);
	}
	return(0);
}
