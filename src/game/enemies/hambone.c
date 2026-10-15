/*
 * hambone.c - Hambone, the Haunted House's boss (object type 0x2016, assets MIKE.ZM / mike.za,
 * the mike*.WMS texmaps, sounds in the "hambone" bank; the audits HAMBONE 1/2 START and END),
 * and the bullets he fires (object type 0x201a, MBLT.ZM, texmap mikebult.WMS). One original file,
 * 0x80109688-0x8010bf2c, its tables at 0x80192ff0-0x80193ad0 (hambone_data.c). His model has
 * 925 vertices, 1386 faces and 917 normals in 21 submeshes, the animation 586 frames; he walks a
 * path built into the image (hambone_path) rather than loaded from a file.
 *
 * He is fought twice: the spawn table starts the first fight in state 0x80 (the audit HAMBONE 1
 * START, five fires on his body, every part's hit count cleared) and the second in 0x81 (HAMBONE 2
 * START: the counts carry over, capped at 5, and the parts already lost stay lost). He walks his
 * path back and forth (0x100 forwards, 0x101 back, a taunt on the way), stops to fire (0x801: a
 * burst at the player every other frame while the movie is not in the two stretches of frames
 * 0x240-0x254 and 0x263-0x277; 0x802: a volley of bullets at frame 0x1fa), staggers when half his
 * health goes (0x103) and reacts to parts shot off (0x201-0x206). A shotgun blast knocks him back
 * (0x206). At 0 health he dies (0x400 in the first fight, 0x401 in the second) and lies in 0x410;
 * a shot then makes him twitch (0x420). 220 health by difficulty (set_enemy_health2), 2000
 * points for the kill, 50 a hit.
 *
 * His bullets fly at a point near the player at a random speed, spinning, for at most 120
 * frames, and hurt for 30 when they reach the camera; shooting one is worth 500 points.
 */
#include <stdlib.h>
#include <glide/glide.h>
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "texmap.h"
#include "credits.h"			/* debug_mode, player_stats_t */
#include "levelsnd.h"
#include "movie.h"

extern player_stats_t player_stats[];
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);	/* text at a world position */
extern void	func_800c8938(float *, float, unsigned int);		/* debug sphere */
extern void	func_800d14a4(float, float);				/* debug map marker */
extern void	func_800c99e4(enemy_t *, int);				/* debug: draw the groups' frames */
extern void	func_800d326c(void *, void *, void *);			/* a textured triangle with the depth gradients */
extern void	screen_to_ray(float *, float, float);			/* vecmat.c */
extern float	D_801faca4, D_801facb4;					/* the debug aim point (objects.c) */
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health2(enemy_t *, float);			/* health by difficulty */
extern player_stats_t *get_target_player(void);
extern void	path_place(enemy_t *, void *, short, float);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	func_800fc16c(enemy_t *, float);			/* enemy.c's file: a step towards the goal */
extern void	angles_to(float *, float *, float *);
extern int	near_camera(enemy_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);
extern void	add_explosion(float *);
extern float	shot_damage_of(player_stats_t *);
extern float	shot_damage_shared(player_stats_t *);			/* player.c */
extern void	award_points(int, int);
extern int	increment_audit(int);
extern int	mark_die();
extern float	func_800fe8dc(float);					/* a random float in +-range */
extern int	randrng(int);
extern void	point_toward_camera(float *, float, float *);
extern int	add_dynamic_light(int, float *, float *, float, int);
extern int	remove_light(int);
extern void	draw_life_bar(float, float, float, float, float, unsigned int);	/* draw2d.c */
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	animate_loose_groups(enemy_t *, int);
extern void	group_point(group_t *, float *);
extern void	draw_effects(enemy_t *), think_effects(enemy_t *), free_effects(enemy_t *);
extern void	attach_effect(enemy_t *, void *);
extern void	*attach_flame(float, float, group_t *);
extern void	*attach_spray(float *, float *, group_t *);
extern void	replace_group(enemy_t *, group_t *, group_t *);		/* a group takes over another's place */
extern void	stream_set_loop(int, int);
extern void	rot_matrix_zyx(float *, float *);
extern void	mat_mul(float *, float *, float *), mat_vec_mul(float *, float *, float *);
extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern int	load_model(void **, int, void **, int, void **, int, char *, int);
extern void	remove_model(void **, int, void **, int, void **, int, char *);
extern void	remove_animation(submesh_t *, int, int, char *);
extern int	violence_level;
extern char	text_buffer[];
extern movie_t	movie;
extern float	camera_x;						/* camera_pos[0] (camera.c) */

/* hambone_data.c */
extern texmap_t	hambone_texmap[57];
extern submesh_t submesh_info_hambone[21];
extern texmap_t	mblt_texmap[1];
extern submesh_t submesh_info_mblt[1];
extern int	hambone_taunts[5], hambone_pains[5];
extern float	hambone_path[178 * 6];				/* 0x80181888 */
extern float	hambone_flash_colour[3];
extern float	hambone_spray6_dir[3], hambone_spray6_base[3], hambone_spray3_dir[3], hambone_spray3_base[3];
extern float	hambone_spray4_base[3], hambone_spray4_dir[3], hambone_spray13_base[3], hambone_spray13_dir[3];
extern float	hambone_spray14_base[3], hambone_spray14_dir[3];
extern int	hambone_fight, hambone_first_fight;
extern int	hambone_hits_3_12, hambone_hits_1_13, hambone_hits_2_4, hambone_hits_5_6;
extern int	hambone_hits_10_14_15, hambone_hits_19, hambone_hits_0, hambone_hits_18;

float	*hambone_verts;			/* 0x801e588c */
face_t	*hambone_faces;			/* 0x80212e00 */
float	*hambone_normals;		/* 0x801e1768 */
float	*mblt_verts;			/* 0x801e5b2c */
face_t	*mblt_faces;			/* 0x8021eecc */
float	*mblt_normals;			/* 0x801e57e8 */
float	hambone_max_health;		/* 0x801e0f2c: his health at the start, for the bar */
int	hambone_walk_dir;		/* 0x801e5cc4: 1 walking his path forwards, -1 back */
unsigned short	hambone_cue_prio;	/* 0x801e7500: the priority of his gun's cue */

#define HAMBONE_GROUPS		0x15
#define HAMBONE_NVERTS		0x39d
#define HAMBONE_NFACES		0x56a
#define HAMBONE_NNORMALS	0x395
#define HAMBONE_FRAMES		0x24a
#define HAMBONE_HIT_GROUP	18		/* the group his sphere is centred on */
#define HAMBONE_BLEND_GROUPS	0x12		/* the groups a blend runs over */

#define MBLT_TYPE		0x201a
#define MBLT_NVERTS		0x36
#define MBLT_NFACES		0x6a
#define MBLT_NNORMALS		0x39
#define MBLT_LIFE		120		/* frames a bullet flies */
#define MBLT_HURT		30
#define MBLT_SPEED(e)		(*(float *)&(e)->unka4)	/* a bullet's speed (an enemy record word) */

/* the states */
#define HAMBONE_FIGHT1		0x80		/* the first fight starts */
#define HAMBONE_FIGHT2		0x81		/* the second */
#define HAMBONE_ENTER		0x82		/* walks on */
#define HAMBONE_FORWARD		0x100		/* walks his path forwards */
#define HAMBONE_BACK		0x101		/* and back */
#define HAMBONE_STAGGER		0x103		/* half his health gone */
#define HAMBONE_LOSE_13		0x201		/* the reactions to a part shot off */
#define HAMBONE_LOSE_3		0x202
#define HAMBONE_HIT_BODY	0x203		/* every second hit on group 0 */
#define HAMBONE_LOSE_LEG	0x204
#define HAMBONE_LOSE_18		0x205
#define HAMBONE_KNOCKED		0x206		/* a shotgun blast */
#define HAMBONE_DIE1		0x400		/* dies, first fight */
#define HAMBONE_DIE2		0x401		/* second fight */
#define HAMBONE_DEAD		0x410
#define HAMBONE_TWITCH		0x420		/* the body shot */
#define HAMBONE_BURST		0x801		/* fires a burst */
#define HAMBONE_VOLLEY		0x802		/* fires a volley of bullets */

#define GENERIC_EXPLODE		1		/* generic1 cues */
#define GENERIC_HAMBONE_GUN	0x3d
#define GENERIC_HAMBONE_HIT	0x95

int	func_80109a54(game_object_t *, player_stats_t *);
void	func_80109c0c(game_object_t *);
int	func_8010a064(game_object_t *, player_stats_t *);
int	func_8010a280(game_object_t *, player_stats_t *);
void	func_8010ac6c(game_object_t *);
void	func_8010b384(enemy_t *, unsigned short);
void	func_8010b658(enemy_t *);
void	func_8010b71c(enemy_t *);
void	func_8010b7a8(enemy_t *);
void	func_8010bd0c(float, float);
void	func_8010be1c(float, float);
void	func_801097f8(game_object_t *);
int	func_80109ad4(game_object_t *);
void	func_80109c44(game_object_t *);

/* 0x80109688: a volley of bullets from his gun (group 0) at points near the player, six when
   both players are in, else three */
void func_80109688(enemy_t *e)
{
	enemy_t	*b;
	short	id;
	int	n;
	int	i;
	float	dx;

	n = 3;
	if((player_stats[0].state & 4) && (player_stats[1].state & 4))
	{
		n = 6;
	}
	for(i = 0; i < n; i++)
	{
		id = add_object1_wrapper(MBLT_TYPE, func_801097f8, func_80109ad4, func_80109c44);
		if(!id)
		{
			return;
		}
		b = get_object_data(id);
		b->x = e->groups[0].pos[0];
		b->y = e->groups[0].pos[1];
		b->z = e->groups[0].pos[2];
		b->counter = MBLT_LIFE;
		b->target = get_target_player();
		b->vel[0] = 0;
		b->vel[1] = 0;
		b->vel[2] = 0;
		b->unk9c = 0;
		b->unka0 = func_800fe8dc(0.3f);
		MBLT_SPEED(b) = func_800fe8dc(0.5f) + 2.0f;
		dx = func_800fe8dc(5.0f);
		set_goal_near_camera(b, &camera_x, b->goal, dx, func_800fe8dc(5.0f), 0.0f);
	}
}

/* 0x801097f8: a bullet's draw callback: drawn spinning about its own axis (unk9c, turning by
   unka0 a frame) at its position plus its velocity, which falls by 0.1 a frame; the drawn
   position is kept for the hit test (unkb4) */
void func_801097f8(game_object_t *obj)
{
	enemy_t	*e;
	float	at[3];
	float	off[3];
	float	spin[3];
	float	mrot[9], mspin[9], m[9];

	e = (enemy_t *)obj->data;
	spin[0] = 0;
	spin[1] = 0;
	spin[2] = e->unk9c;
	rot_matrix_zyx(mspin, spin);
	at[0] = e->x;
	at[1] = e->y;
	at[2] = e->z;
	rot_matrix_zyx(mrot, &e->rx);
	mat_mul(mrot, mspin, m);
	mat_vec_mul(e->vel, m, off);
	e->x += off[0];
	e->y += off[1];
	e->z += off[2];
	draw_model(e, submesh_info_mblt, 1, mblt_verts, MBLT_NVERTS, mblt_faces, MBLT_NFACES, mblt_normals, MBLT_NNORMALS);
	e->unkb4 = e->x;
	e->unkb8 = e->y;
	e->unkbc = e->z;
	e->x = at[0];
	e->y = at[1];
	e->z = at[2];
	e->unk9c += e->unka0;
	e->vel[1] += 0.1f;
	if(debug_mode & 0x100)
	{
		func_800c8938(&e->unkb4, e->radius, 0x600000);
	}
}

/* 0x80109988: the points for a bullet on their own (nothing calls it) */
void func_80109988(game_object_t *obj, player_stats_t *p, float damage, enemy_t *e)
{
	award_points(p->index, 500);
	e->killer = p;
}

/* 0x801099c8: a bullet's hit test: its sphere where it was drawn */
int func_801099c8(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(&e->unkb4, e->radius * e->radius, p))
	{
		p->hit = 1;
		p->hit_id = obj->id;
		p->hit_group = 0;
		p->die = (void *)func_80109a54;
		p->hit_point[0] = p->aim[0];
		p->hit_point[1] = p->aim[1];
		p->hit_point[2] = p->aim[2];
	}
	return(0);
}

/* 0x80109a54: a bullet shot down: 500 points, an explosion */
int func_80109a54(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	e->ai_state = 0;
	shot_damage_of(p);
	award_points(p->index, 500);
	e->killer = p;
	remove_game_object(obj->id);
	add_explosion(p->hit_point);
	play_rotating_cue(GENERIC_EXPLODE, "generic1");
	return(0);
}

/* 0x80109ad4: a bullet's init callback */
int func_80109ad4(game_object_t *obj)
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
	obj->hit = func_801099c8;
	obj->die = func_80109a54;
	obj->remove = func_80109c0c;
	e = (enemy_t *)obj->data;
	e->counter = MBLT_LIFE;
	e->unk4 = 0x14;
	e->ai_state = 0;
	e->num = 0;
	e->flags = 0xc;
	e->rx = 0;
	e->ry = 1.570796f;
	e->rz = 0;
	e->x = 0;
	e->y = 0;
	e->z = 0;
	e->unkb4 = 0;
	e->unkb8 = 0;
	e->unkbc = 0;
	e->unkcc = 0.1129f;
	set_enemy_scale(e, 1.0f);
	e->groups = malloc(1 * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < 1; i++)
	{
		e->groups[i].flags = 0x8000;
	}
	init_groups(e, submesh_info_mblt, 1);
	g = e->groups;
	e->hit_group = g;
	g->floor = 0.0f;
	return(0);
}

/* 0x80109c0c: a bullet's remove callback */
void func_80109c0c(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	free(e->groups);
	free(e);
}

/* 0x80109c44: a bullet's think callback: on towards its goal, turned to face away from the
   camera; within 6 of the camera it hurts the player and goes, else it goes when its time is up */
void func_80109c44(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	func_800fc16c(e, MBLT_SPEED(e));
	angles_to(&camera_x, &e->x, &e->rx);
	if(near_camera(e, 6.0f))
	{
		remove_game_object(obj->id);
		hurt_player(e->target, MBLT_HURT, 0, 0.0f);
	}
	else if(--e->counter == 0)
	{
		remove_game_object(obj->id);
	}
}

/* 0x80109cf4: Hambone's draw callback: a muzzle light while he fires (frames 0x8d-0xbe on odd
   counts, frame 0x1fa), the model, his effects, the name plate and life bar while he lives */
void func_80109cf4(game_object_t *obj)
{
	enemy_t	*e;
	float	pos[3];
	int	light;
	int	lit;

	e = (enemy_t *)obj->data;
	lit = 0;
	light = 0;
	if(e->frame >= 0x8d && e->frame <= 0xbe)
	{
		if(e->counter & 1)
		{
			pos[0] = e->x;
			pos[1] = e->y + 10.0f;
			pos[2] = e->z;
			light = add_dynamic_light(0x11, pos, hambone_flash_colour, 4.0f, 15);
			lit = 1;
		}
	}
	else
	{
		lit = 0;
		if(e->frame == 0x1fa)
		{
			point_toward_camera(e->hit_group->pos, 1.0f, pos);
			light = add_dynamic_light(0x11, pos, hambone_flash_colour, 10.0f, 15);
			lit = 1;
		}
	}
	draw_model(e, submesh_info_hambone, HAMBONE_GROUPS, hambone_verts, HAMBONE_NVERTS, hambone_faces, HAMBONE_NFACES, hambone_normals, HAMBONE_NNORMALS);
	if(lit == 1)
	{
		remove_light(light);
	}
	draw_effects(e);
	if(!(e->ai_state & 0x400))
	{
		if(hambone_fight == 1)
		{
			func_8010bd0c(200.0f, 233.0f);
		}
		else
		{
			func_8010bd0c(164.0f, 233.0f);
			func_8010be1c(236.0f, 233.0f);
		}
		draw_life_bar(e->health, hambone_max_health, 150.0f, 250.0f, 248.0f, 0x904000);
	}
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 2)
	{
		screen_to_ray(player_stats[0].aim_dir, D_801faca4, D_801facb4);
		player_stats[0].aim_dir[2] = 1.0f / player_stats[0].aim_dir[2];
		func_8010a064(obj, &player_stats[0]);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[HAMBONE_HIT_GROUP].pos, e->radius, 0x600000);
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
	if(debug_mode & 0x1000)
	{
		func_800c99e4(e, 0xff);
	}
}

/* 0x8010a064: the hit test: the bounding sphere, then the submesh */
int func_8010a064(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_hambone, HAMBONE_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)func_8010a280;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x8010a120: the damage half of the die callback on its own (nothing calls it) */
void func_8010a120(game_object_t *obj, player_stats_t *p, float damage, enemy_t *e)
{
	float	old;

	old = e->health;
	e->health = old - damage;
	if(hambone_max_health * 0.5f <= old && e->health < hambone_max_health * 0.5f)
	{
		e->ai_state = HAMBONE_STAGGER;
		e->frame = 0x19f;
		e->flags &= ~2;
		play_enemy_sound(e, 8, "hambone", 0);
	}
	if(e->health < 0.0f)
	{
		award_points(p->index, 2000);
		if(hambone_fight == 1)
		{
			increment_audit(0x35);
			play_enemy_sound(e, 15, "hambone", 8);
			e->ai_state = HAMBONE_DIE1;
			e->health = 0.0f;
			e->frame = 0x202;
			e->flags &= ~2;
		}
		else
		{
			increment_audit(0x37);
			play_enemy_sound(e, 15, "hambone", 8);
			e->ai_state = HAMBONE_DIE2;
			e->health = 0.0f;
			e->frame = 0x103;
			e->flags &= ~2;
		}
	}
	else
	{
		award_points(p->index, 50);
	}
}

/* the damage, as func_8010a120 has it */
#define HAMBONE_DAMAGE(damage, half)							\
	{										\
		float	dmg, old;							\
											\
		dmg = (damage);								\
		old = e->health;							\
		e->health = old - dmg;							\
		if(hambone_max_health * (half) <= old && e->health < hambone_max_health * (half))	\
		{									\
			e->ai_state = HAMBONE_STAGGER;					\
			e->frame = 0x19f;						\
			e->flags &= ~2;							\
			play_enemy_sound(e, 8, "hambone", 0);				\
		}									\
		if(e->health < 0.0f)							\
		{									\
			award_points(p->index, 2000);					\
			if(hambone_fight == 1)						\
			{								\
				increment_audit(0x35);					\
				play_enemy_sound(e, 15, "hambone", 8);			\
				e->ai_state = HAMBONE_DIE1;				\
				e->health = 0.0f;					\
				e->frame = 0x202;					\
				e->flags &= ~2;						\
			}								\
			else								\
			{								\
				increment_audit(0x37);					\
				play_enemy_sound(e, 15, "hambone", 8);			\
				e->ai_state = HAMBONE_DIE2;				\
				e->health = 0.0f;					\
				e->frame = 0x103;					\
				e->flags &= ~2;						\
			}								\
		}									\
		else									\
		{									\
			award_points(p->index, 50);					\
		}									\
	}

/* 0x8010a280: the shot lands: a spark on his gun (group 0), else flesh and blood; on the body
   (dead) a twitch; else the damage (double on groups 10 and 14, none on the gun); then a shotgun
   blast knocks him back, or the hit counts towards the part (a part comes off at 5) */
int func_8010a280(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	float	damage;

	e = (enemy_t *)obj->data;
	if(p->hit_group == 0)
	{
		add_world_sprite(p->hit_point, 6, 0);
	}
	else
	{
		add_chunks(p->hit_point, e->unk98);
		add_world_sprite(p->hit_point, 0, 0);
		burst_blood(p->hit_point, e->unk98, 0x800000);
	}
	if(e->ai_state & 0x400)
	{
		if(e->ai_state == HAMBONE_DEAD || e->ai_state == HAMBONE_TWITCH)
		{
			e->ai_state = HAMBONE_TWITCH;
			e->frame = 0x141;
			e->flags &= ~2;
		}
		return(0);
	}
	switch(p->hit_group)
	{
	case 10:
	case 14:
		damage = shot_damage_shared(p);
		HAMBONE_DAMAGE(damage + damage, 0.5f)
		break;
	case 0:
		break;
	default:
		HAMBONE_DAMAGE(shot_damage_shared(p), 0.5f)
		break;
	}
	if(e->ai_state & 0x400)
	{
		return(0);
	}
	if(!(e->ai_state & 0x80) && p->shot_weapon == 2)
	{
		e->ai_state = HAMBONE_KNOCKED;
		e->frame = 0x240;
		e->flags &= ~2;
		return(0);
	}
	if(e->ai_state & 0x200)
	{
		return(0);
	}
	if(e->ai_state & 0x80)
	{
		return(0);
	}
	switch(p->hit_group)
	{
	case 10:
	case 14:
	case 15:
		if(hambone_fight == 1)
		{
			if(hambone_hits_10_14_15 < 4)
			{
				hambone_hits_10_14_15++;
			}
		}
		else
		{
			hambone_hits_10_14_15++;
		}
		func_8010b7a8(e);
		break;
	case 1:
	case 13:
		hambone_hits_1_13++;
		func_8010b7a8(e);
		break;
	case 2:
	case 4:
		hambone_hits_2_4++;
		func_8010b7a8(e);
		break;
	case 3:
	case 12:
		hambone_hits_3_12++;
		func_8010b7a8(e);
		break;
	case 5:
	case 6:
		hambone_hits_5_6++;
		func_8010b7a8(e);
		break;
	case 19:
		hambone_hits_19++;
		func_8010b7a8(e);
		break;
	case 18:
		hambone_hits_18++;
		func_8010b7a8(e);
		break;
	case 0:
		if(!(++hambone_hits_0 & 1))
		{
			play_rotating_cue(GENERIC_HAMBONE_HIT, "generic1");
			e->ai_state = HAMBONE_HIT_BODY;
			e->frame = 0x1c0;
			e->flags &= ~2;
		}
		break;
	case 20:		/* the table runs to group 20, which counts nothing */
		break;
	}
	return(0);
}

/* 0x8010a794 */
void load_mike_texmap(void)
{
	load_texmap(hambone_texmap, 0x39, "mike");
	load_texmap(mblt_texmap, 1, "mblt");
}

/* 0x8010a7dc: his model, its animation (the parts that come off share their originals'
   animations: 13 moves as 1, 3 as 12, 4 as 2, 6 as 5, 14 and 15 as 10) and the bullet's model */
void load_mike(void)
{
	load_model((void **)&hambone_verts, HAMBONE_NVERTS, (void **)&hambone_faces, HAMBONE_NFACES, (void **)&hambone_normals, HAMBONE_NNORMALS, "MIKE.ZM", 0xaf2798cc);
	load_anim(submesh_info_hambone, HAMBONE_GROUPS, HAMBONE_FRAMES, "mike.za", 0x9aa47bf0);
	load_model((void **)&mblt_verts, MBLT_NVERTS, (void **)&mblt_faces, MBLT_NFACES, (void **)&mblt_normals, MBLT_NNORMALS, "MBLT.ZM", 0x8d0d2cad);
	free(submesh_info_hambone[13].anim);
	free(submesh_info_hambone[3].anim);
	free(submesh_info_hambone[4].anim);
	free(submesh_info_hambone[6].anim);
	free(submesh_info_hambone[14].anim);
	free(submesh_info_hambone[15].anim);
	submesh_info_hambone[13].anim = submesh_info_hambone[1].anim;
	submesh_info_hambone[3].anim = submesh_info_hambone[12].anim;
	submesh_info_hambone[4].anim = submesh_info_hambone[2].anim;
	submesh_info_hambone[6].anim = submesh_info_hambone[5].anim;
	submesh_info_hambone[14].anim = submesh_info_hambone[10].anim;
	submesh_info_hambone[15].anim = submesh_info_hambone[10].anim;
}

/* 0x8010a964: the borrowed animations are dropped first so they are not freed twice */
void remove_mike_assets(void)
{
	submesh_info_hambone[13].anim = 0;
	submesh_info_hambone[3].anim = 0;
	submesh_info_hambone[4].anim = 0;
	submesh_info_hambone[6].anim = 0;
	submesh_info_hambone[14].anim = 0;
	submesh_info_hambone[15].anim = 0;
	remove_model((void **)&mblt_verts, MBLT_NVERTS, (void **)&mblt_faces, MBLT_NFACES, (void **)&mblt_normals, MBLT_NNORMALS, "MBLT.ZM");
	remove_surface(mblt_texmap, 1);
	remove_surface(hambone_texmap, 0x39);
	remove_animation(submesh_info_hambone, HAMBONE_GROUPS, HAMBONE_FRAMES, "mike.za");
	remove_model((void **)&hambone_verts, HAMBONE_NVERTS, (void **)&hambone_faces, HAMBONE_NFACES, (void **)&hambone_normals, HAMBONE_NNORMALS, "MIKE.ZM");
}

/* 0x8010aa68: the init callback: the record (scale 1.2, 220 health by difficulty), the groups
   (the six spare parts hidden), his voice's priorities and his greeting */
int func_8010aa68(game_object_t *obj)
{
	enemy_t	*e;
	void	*data;
	group_t	*g;
	group_t	*gun;			/* group 0: his gun (its hits spark, his shots leave from it) */
	int	i;

	data = malloc(sizeof(enemy_t));
	obj->data = data;
	if(!data)
	{
		return(1);
	}
	obj->hit = func_8010a064;
	obj->die = func_8010a280;
	obj->remove = func_8010ac6c;
	e = (enemy_t *)data;
	e->unk4 = 0x14;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0xc;
	e->target = get_target_player();
	e->rx = 0;
	e->rz = 0;
	e->x = 0;
	e->y = 0;
	e->unk98 = 0;
	e->ry = 3.1415927f;
	e->z = 10.0f;
	e->unkcc = 1.0f;
	set_enemy_scale(e, 1.2f);
	set_enemy_health2(e, 220.0f);
	hambone_max_health = e->health;
	e->groups = malloc(HAMBONE_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < HAMBONE_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[13].flags = 0;
	e->groups[3].flags = 0;
	e->groups[4].flags = 0;
	e->groups[6].flags = 0;
	e->groups[14].flags = 0;
	e->groups[10].flags = 0;
	init_groups(e, submesh_info_hambone, HAMBONE_GROUPS);
	g = e->groups;
	hambone_walk_dir = 1;
	e->hit_group = &g[HAMBONE_HIT_GROUP];
	g[HAMBONE_HIT_GROUP].floor = 0.0f;
	gun = e->groups;
	gun->flags = 0x8001;
	gun->rot[0] = 0;
	gun->rot[1] = 0;
	gun->rot[2] = 0;
	e->unk92 = 0x28;
	e->effects = 0;
	e->path = 0;
	e->unk90 = 0;
	e->prio_base = 0x11;
	e->prio_flags = next_cue_priority();
	hambone_cue_prio = next_cue_priority();
	play_enemy_sound(e, 6, "hambone", 0);
	func_8010b384(e, 0);
	return(0);
}

/* 0x8010ac6c: the remove callback */
void func_8010ac6c(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	free_effects(e);
	free(e->groups);
	free(e);
}

/* 0x8010acac: the think callback */
void func_8010acac(game_object_t *obj)
{
	enemy_t	*e;
	float	p[3];
	int	f;

	if(debug_mode & 0x10000)
	{
		return;
	}
	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, HAMBONE_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case HAMBONE_VOLLEY:
		if(++e->frame == 0x1fa)
		{
			p[0] = 0;
			p[1] = 0.4f;
			p[2] = 0;
			group_point(e->groups, p);
			add_world_sprite(p, 4, 0);
			play_rotating_cue(GENERIC_EXPLODE, "generic1");
			func_80109688(e);
		}
		if(e->frame >= 0x202)
		{
			func_8010b71c(e);
		}
		break;
	case HAMBONE_LOSE_13:
		if(++e->frame >= 0x19f)
		{
			func_8010b71c(e);
		}
		path_place(e, hambone_path, e->path, 1.0f);
		break;
	case HAMBONE_LOSE_3:
		if(++e->frame >= 0x17c)
		{
			func_8010b71c(e);
		}
		path_place(e, hambone_path, e->path, 1.0f);
		break;
	case HAMBONE_LOSE_LEG:
		if(++e->frame >= 0x76)
		{
			func_8010b71c(e);
		}
		path_place(e, hambone_path, e->path, 1.0f);
		break;
	case HAMBONE_HIT_BODY:
		if(++e->frame >= 0x1cf)
		{
			func_8010b71c(e);
		}
		path_place(e, hambone_path, e->path, 1.0f);
		break;
	case HAMBONE_LOSE_18:
		if(++e->frame >= 0x1f1)
		{
			func_8010b71c(e);
		}
		path_place(e, hambone_path, e->path, 1.0f);
		break;
	case HAMBONE_KNOCKED:
		if(++e->frame >= HAMBONE_FRAMES)
		{
			func_8010b71c(e);
		}
		path_place(e, hambone_path, e->path, 1.0f);
		break;
	case HAMBONE_DIE1:
		if(++e->frame >= 0x240)
		{
			mark_die(obj->id, e->killer);
			e->ai_state = HAMBONE_DEAD;
			e->frame--;
			stream_set_loop(2, 2);
		}
		break;
	case HAMBONE_DIE2:
		if(++e->frame >= 0x141)
		{
			mark_die(obj->id, e->killer);
			e->ai_state = HAMBONE_DEAD;
			e->frame--;
		}
		break;
	case HAMBONE_DEAD:
		break;
	case HAMBONE_TWITCH:
		if(++e->frame >= 0x150)
		{
			e->frame--;
			e->ai_state = HAMBONE_DEAD;
		}
		break;
	case HAMBONE_FIGHT1:
		if(++e->frame >= 0x65)
		{
			e->frame--;
			e->target = get_target_player();
			blend_to_frame(e, submesh_info_hambone, HAMBONE_BLEND_GROUPS, 0x76, 2);
			e->ai_state = HAMBONE_ENTER;
			play_enemy_sound(e, 7, "hambone", 0);
		}
		break;
	case HAMBONE_STAGGER:
		if(++e->frame >= 0x1c0)
		{
			func_8010b658(e);
		}
		path_place(e, hambone_path, e->path, 1.0f);
		break;
	case HAMBONE_ENTER:
		if(++e->frame >= 0x8d)
		{
			func_8010b658(e);
		}
		path_place(e, hambone_path, e->path, 1.0f);
		if(++e->path > e->unk92)
		{
			e->path = e->unk92;
		}
		break;
	case HAMBONE_BURST:
		f = movie.disp->frame;
		if(!(f >= 0x240 && f <= 0x254) && !(f >= 0x263 && f <= 0x277))
		{
			if(++e->counter & 1)
			{
				play_cue_flags_noint(GENERIC_HAMBONE_GUN, "generic1", hambone_cue_prio);
				p[0] = 0;
				p[1] = 0.7f;
				p[2] = 0;
				group_point(e->groups, p);
				add_world_sprite(p, 4, 0);
				hurt_player(e->target, 8, 6, e->unk9c);
				e->unk9c += 6.5f;
			}
			e->groups[0].rot[1] -= 0.5f;
		}
		if(++e->frame >= 0xbf)
		{
			func_8010b71c(e);
		}
		break;
	case HAMBONE_FORWARD:
		path_place(e, hambone_path, e->path, 1.0f);
		if(++e->counter >= 4)
		{
			e->counter = 0;
			e->path++;
		}
		if(e->path > e->unk92)
		{
			e->path = e->unk92;
			hambone_walk_dir = -1;
		}
		if(++e->frame == 0xf9)
		{
			play_enemy_sound(e, hambone_taunts[randrng(5)], "hambone", 0);
		}
		if(e->frame >= 0x103)
		{
			func_8010b658(e);
		}
		break;
	case HAMBONE_BACK:
		path_place(e, hambone_path, e->path, 1.0f);
		if(++e->counter >= 4)
		{
			e->counter = 0;
			e->path--;
		}
		if(e->path < e->unk90)
		{
			e->path = e->unk90;
			hambone_walk_dir = 1;
		}
		if(++e->frame >= 0xe6)
		{
			func_8010b658(e);
		}
		break;
	}
}

/* 0x8010b384: the spawn table's state: the start of the first fight (fires on his body, the hit
   counts cleared) or of the second (the counts carried over and capped at 5, the parts already
   lost taken off again, the gun's group flagged) */
void func_8010b384(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	switch(state)
	{
	case HAMBONE_FIGHT1:
		increment_audit(0x34);
		hambone_first_fight = 1;
		hambone_fight = 1;
		e->frame = 5;
		hambone_hits_3_12 = 0;
		hambone_hits_1_13 = 0;
		hambone_hits_2_4 = 0;
		hambone_hits_5_6 = 0;
		hambone_hits_10_14_15 = 0;
		hambone_hits_19 = 0;
		hambone_hits_0 = 0;
		hambone_hits_18 = 0;
		attach_effect(e, attach_flame(0.0f, (submesh_info_hambone[8].radius + submesh_info_hambone[8].radius) * e->scale[0], &e->groups[8]));
		attach_effect(e, attach_flame(0.0f, (submesh_info_hambone[9].radius + submesh_info_hambone[9].radius) * e->scale[0], &e->groups[9]));
		attach_effect(e, attach_flame(0.0f, (submesh_info_hambone[2].radius + submesh_info_hambone[2].radius) * e->scale[0], &e->groups[2]));
		attach_effect(e, attach_flame(0.0f, (submesh_info_hambone[5].radius + submesh_info_hambone[5].radius) * e->scale[0], &e->groups[5]));
		attach_effect(e, attach_flame(0.0f, (submesh_info_hambone[15].radius + submesh_info_hambone[15].radius) * e->scale[0], &e->groups[15]));
		path_place(e, hambone_path, e->path, 1.0f);
		break;
	case HAMBONE_FIGHT2:
		increment_audit(0x36);
		hambone_first_fight = 0;
		hambone_fight = 2;
		if(hambone_hits_3_12 >= 5)
		{
			hambone_hits_3_12 = 5;
		}
		if(hambone_hits_1_13 >= 5)
		{
			hambone_hits_1_13 = 5;
		}
		if(hambone_hits_2_4 >= 5)
		{
			hambone_hits_2_4 = 5;
		}
		if(hambone_hits_5_6 >= 5)
		{
			hambone_hits_5_6 = 5;
		}
		if(hambone_hits_10_14_15 >= 5)
		{
			hambone_hits_10_14_15 = 5;
		}
		if(hambone_hits_19 >= 5)
		{
			hambone_hits_19 = 5;
		}
		func_8010b7a8(e);
		e->frame = 0x76;
		path_place(e, hambone_path, e->path, 1.0f);
		e->ai_state = HAMBONE_ENTER;
		e->groups[0].flags |= 0x1000000;
		break;
	}
}

/* 0x8010b658: stop and fire: a burst in the first fight, a volley in the second */
void func_8010b658(enemy_t *e)
{
	if(hambone_fight == 1)
	{
		e->frame--;
		e->target = get_target_player();
		blend_to_frame(e, submesh_info_hambone, HAMBONE_BLEND_GROUPS, 0x8d, 2);
		e->ai_state = HAMBONE_BURST;
		e->counter = 0;
		e->unk9c = -90.0f;
	}
	else
	{
		e->frame--;
		e->target = get_target_player();
		blend_to_frame(e, submesh_info_hambone, HAMBONE_BLEND_GROUPS, 0x1f1, 2);
		e->ai_state = HAMBONE_VOLLEY;
		e->counter = 0;
		e->unk9c = -90.0f;
	}
}

/* 0x8010b71c: walk on, the way he was going */
void func_8010b71c(enemy_t *e)
{
	e->frame--;
	if(hambone_walk_dir == 1)
	{
		blend_to_frame(e, submesh_info_hambone, HAMBONE_BLEND_GROUPS, 0xe6, 2);
		e->ai_state = HAMBONE_FORWARD;
	}
	else
	{
		blend_to_frame(e, submesh_info_hambone, HAMBONE_BLEND_GROUPS, 0xbf, 2);
		e->ai_state = HAMBONE_BACK;
	}
}

/* 0x8010b7a8: the parts whose hit counts have just reached their mark come off (at the top
   violence level the stump bleeds), each with a cry and most with a reaction */
void func_8010b7a8(enemy_t *e)
{
	if(hambone_hits_10_14_15 == 10)
	{
		play_enemy_sound(e, hambone_pains[randrng(5)], "hambone", 2);
		hambone_hits_10_14_15++;
		if(violence_level >= 3)
		{
			replace_group(e, &e->groups[10], &e->groups[14]);
			attach_effect(e, attach_spray(hambone_spray14_base, hambone_spray14_dir, &e->groups[14]));
		}
	}
	if(hambone_hits_10_14_15 == 5)
	{
		play_enemy_sound(e, hambone_pains[randrng(5)], "hambone", 2);
		hambone_hits_10_14_15++;
		replace_group(e, &e->groups[15], &e->groups[10]);
	}
	if(hambone_hits_1_13 == 5)
	{
		play_enemy_sound(e, hambone_pains[randrng(5)], "hambone", 2);
		hambone_hits_1_13++;
		if(violence_level >= 3)
		{
			replace_group(e, &e->groups[1], &e->groups[13]);
			attach_effect(e, attach_spray(hambone_spray13_base, hambone_spray13_dir, &e->groups[13]));
			e->groups[17].flags |= 0xf0000000;
		}
		e->ai_state = HAMBONE_LOSE_13;
		e->frame = 0x17c;
		e->flags &= ~2;
	}
	if(hambone_hits_3_12 == 5)
	{
		play_enemy_sound(e, hambone_pains[randrng(5)], "hambone", 2);
		hambone_hits_3_12++;
		if(violence_level >= 3)
		{
			replace_group(e, &e->groups[12], &e->groups[3]);
			attach_effect(e, attach_spray(hambone_spray3_base, hambone_spray3_dir, &e->groups[3]));
			e->groups[16].flags |= 0xf0000000;
		}
		e->ai_state = HAMBONE_LOSE_3;
		e->frame = 0x150;
		e->flags &= ~2;
	}
	if(hambone_hits_2_4 == 5)
	{
		play_enemy_sound(e, hambone_pains[randrng(5)], "hambone", 2);
		hambone_hits_2_4++;
		if(violence_level >= 3)
		{
			replace_group(e, &e->groups[2], &e->groups[4]);
			attach_effect(e, attach_spray(hambone_spray4_base, hambone_spray4_dir, &e->groups[4]));
		}
		e->ai_state = HAMBONE_LOSE_LEG;
		e->frame = 0x65;
		e->flags &= ~2;
	}
	if(hambone_hits_5_6 == 5)
	{
		play_enemy_sound(e, hambone_pains[randrng(5)], "hambone", 2);
		hambone_hits_5_6++;
		if(violence_level >= 3)
		{
			replace_group(e, &e->groups[5], &e->groups[6]);
			attach_effect(e, attach_spray(hambone_spray6_base, hambone_spray6_dir, &e->groups[6]));
		}
		e->ai_state = HAMBONE_LOSE_LEG;
		e->frame = 0x65;
		e->flags &= ~2;
	}
	if(hambone_hits_19 == 5)
	{
		play_enemy_sound(e, hambone_pains[randrng(5)], "hambone", 2);
		hambone_hits_19++;
		if(violence_level >= 3)
		{
			e->groups[19].flags |= 0xf0000000;
		}
	}
	if(hambone_hits_18 == 5)
	{
		play_enemy_sound(e, hambone_pains[randrng(5)], "hambone", 2);
		hambone_hits_18++;
		if(violence_level >= 3)
		{
			e->groups[18].flags |= 0xf0000000;
		}
		e->ai_state = HAMBONE_LOSE_18;
		e->frame = 0x1cf;
		e->flags &= ~2;
	}
}

/* 0x8010bd0c: his name plate for the first fight: texture 0x7080, 64 by 32 at (x, y) */
void func_8010bd0c(float x, float y)
{
	MidVertex	a, b, c, d;

	guTexSource(find_surface(0x7080)->handle);
	guColorCombineFunction(4);
	guAlphaSource(2);
	grAlphaTestReferenceValue(4);
	a.x = x - 32.0f;
	a.y = y - 16.0f;
	a.oow = 0.90909088f;
	a.sow = 0.0f;
	a.tow = 116.36363f;
	b.x = x + 32.0f;
	b.y = y - 16.0f;
	b.oow = 0.90909088f;
	b.sow = 231.81818f;
	b.tow = 116.36363f;
	c.x = x - 32.0f;
	c.y = y + 16.0f;
	c.oow = 0.90909088f;
	c.sow = 0.0f;
	c.tow = 0.0f;
	d.x = x + 32.0f;
	d.y = y + 16.0f;
	d.oow = 0.90909088f;
	d.sow = 231.81818f;
	d.tow = 0.0f;
	func_800d326c(&a, &d, &b);
	func_800d326c(&a, &c, &d);
}

/* 0x8010be1c: the second plate of the second fight: texture 0x7081 */
void func_8010be1c(float x, float y)
{
	MidVertex	a, b, c, d;

	guTexSource(find_surface(0x7081)->handle);
	guColorCombineFunction(4);
	guAlphaSource(2);
	grAlphaTestReferenceValue(4);
	a.x = x - 32.0f;
	a.y = y - 16.0f;
	a.oow = 0.90909088f;
	a.sow = 0.0f;
	a.tow = 116.36363f;
	b.x = x + 32.0f;
	b.y = y - 16.0f;
	b.oow = 0.90909088f;
	b.sow = 231.81818f;
	b.tow = 116.36363f;
	c.x = x - 32.0f;
	c.y = y + 16.0f;
	c.oow = 0.90909088f;
	c.sow = 0.0f;
	c.tow = 0.0f;
	d.x = x + 32.0f;
	d.y = y + 16.0f;
	d.oow = 0.90909088f;
	d.sow = 231.81818f;
	d.tow = 0.0f;
	func_800d326c(&a, &d, &b);
	func_800d326c(&a, &c, &d);
}
