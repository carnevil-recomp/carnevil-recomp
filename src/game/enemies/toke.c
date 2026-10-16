/*
 * toke.c - the Big Top's TOKE (object type 0x2066; TOKE.ZM / toke.za and the toke*.WMS texmaps:
 * a skull face, beard, moustache, ears and hair, a frilled collar, tie and coin, balloons,
 * hands; its sounds are in the banks "tokken", "bigtop" and "bigtop2"), with the pieces it
 * throws (object type 0x1901: one coloured part of the shared MISL.ZM model, tokeg10-17.WMS) and
 * its movie-script callbacks. One original file, 0x80138320-0x8013b9c8 (LITL's file ends before
 * it with a padding word at 0x8013831c; Doctor Klot's draw starts at 0x8013b9c8), its tables in
 * toke_data.c.
 *
 * The exported names are the address names the shared files (spawn.c, wave.c, the movie
 * scripts) use: func_80138ac4 is the draw callback, func_801394f4 the init, func_801397f4 the
 * think, func_8013aa34 the state set-up, the sub_8013af24 ... sub_8013b850 are script callbacks.
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

extern player_stats_t	player_stats[2];
extern submesh_t	missile_submesh[7];		/* weapons_data.c: MISL.ZM's parts */
extern float		*missile_verts, *missile_normals;	/* weapons.c */
extern face_t		*missile_faces;

extern player_stats_t *get_target_player(void);
extern player_stats_t *attack_target(enemy_t *);
extern int	randrng(int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	near_camera(enemy_t *, float);
extern void	path_place_by_camera(enemy_t *, float *, int, float);	/* enemy.c */
extern void	hurt_player(player_stats_t *, int, int, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern void	award_points(int, int);
extern void	burst_flecks(float *, int, int, int);
extern void	fleck_enemy(enemy_t *, int, int, int);
extern void	set_enemy_scale(enemy_t *, float);

void	*toke_misl_path;		/* 0x802373cc: the path the thrown pieces fly along */

extern char	text_buffer[];
extern int	violence_level;
extern enemy_t	*debug_enemy;
extern int	D_801de94c;			/* jester.c: the jester cannot be hurt while set; the init clears it */
extern texmap_t	toke_texmap[59];		/* toke_data.c */
extern submesh_t submesh_info_toke[22];

extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);			/* a debug sphere */
extern void	func_800d14a4(float, float);					/* a debug map dot */
extern void	func_800c99e4(enemy_t *, int);
extern void	draw_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	draw_life_bar(float, float, float, float, float, unsigned int);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_shared(player_stats_t *);
extern void	replace_group(enemy_t *, group_t *, group_t *);
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);
extern void	increment_audit(int);
extern void	disable_guns(void);
extern int	mark_die();
extern void	set_enemy_health2(enemy_t *, float);
extern void	init_groups(enemy_t *, submesh_t *, int);
extern unsigned int next_cue_priority(void);
extern void	load_texmap(texmap_t *, int, char *), remove_surface(texmap_t *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);

float	*toke_verts;			/* 0x801e5b64 */
face_t	*toke_faces;			/* 0x8021f078 */
float	*toke_normals;			/* 0x801e5814 */
enemy_t	*toke_enemy;			/* 0x801e0ec0: the one TOKE, for the script callbacks */
int	toke_shown;			/* 0x801e5dcc: he is drawn and can be hit */
int	toke_damage_scaled;		/* 0x801e6760: shots do half below 20% of his health, none below 8% */
float	toke_full_health;		/* 0x801e1778 */
int	toke_gore;			/* 0x801e0ee0: parts may be damaged (with violence 3) */
int	toke_hits_head;			/* 0x801e0e98: shots on groups 6 and 20 */
int	toke_hits_body;			/* 0x801e5da0: shots on groups 9, 19 and 21 */
int	toke_hits_14;			/* 0x801e0efc: shots on group 14 */
int	toke_hits_1;			/* 0x801e0ed4: shots on group 1 */
int	toke_reactions;			/* 0x801e0a04: alternates the reaction to body shots */
int	D_801fad1c, D_801e5dc4, D_801de8b4;
int	D_801e0e80, D_801e0e84, D_801e0e88, D_801e0e8c, D_801e0e90, D_801e0e94, D_801e0e9c, D_801e0ea8;
int	D_801e5830, D_801e57dc;
int	toke_spray_r, toke_spray_g, toke_spray_b;	/* 0x801e6b70, 0x801e5dc0, 0x801e5db8: the colour of the 0x8209 flecks, cycling */

#define TOKE_GROUPS	0x16
#define TOKE_NVERTS	0x4aa
#define TOKE_NFACES	0x80b
#define TOKE_NNORMALS	0x534
#define TOKE_FRAMES	0x368
#define MISL_PATH_LEN	0x30e

int	toke_hit(game_object_t *, player_stats_t *);
int	toke_die(game_object_t *, player_stats_t *);
void	toke_remove(game_object_t *);
void	func_8013aa34(enemy_t *, unsigned short);
void	toke_draw_name(float, float);
void	func_8013b94c(enemy_t *);
void	func_8013b8d8(enemy_t *);
void	sub_8013afbc(void);

extern void	detach_effect(enemy_t *, group_t *);
extern void	free_blood_drops(void), free_chunks(void), free_world_sprites(void);
extern int	toke_calls1[6], toke_call1;		/* toke_data.c: 0x801a1168, 0x801a1180 */
extern int	toke_calls2[10], toke_call2;		/* toke_data.c: 0x801a1184, 0x801a11ac */

extern void	pick_random_action(enemy_t *, void *, submesh_t *, int, int);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	detach_group(group_t *, float *, float, float *);
extern surface_t *find_surface(int);
extern void	func_800d326c(void *, void *, void *);		/* a screen-space textured triangle */
extern int	toke_actions[];				/* toke_data.c: 0x801a111c */
extern int	toke_action_picks;			/* toke_data.c: 0x801a1164 */
extern float	D_801a1110[3];				/* toke_data.c: the colour of the light he dies in (255, 0, 255) */
extern void	think_effects(enemy_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern int	face_camera(enemy_t *, float);
extern void	group_point(group_t *, float *);
extern int	group_near_camera(group_t *, float);
extern float	func_800fe8dc(float);			/* a random float in +-range */
extern void	add_smoke(float *, unsigned int);
extern int	add_dynamic_light(int, float *, float *, float, int);
extern void	movie_script_skip(void);
extern void	add_chunks_n(float *, float, int);
extern void	burst_blood(float *, float, int);	/* the original passes a third word the routine ignores */
extern void	attach_effect(enemy_t *, void *);
extern void	*attach_held_object(group_t *, int, float, float, float, float, float, float, float *);
extern void	lob_at_camera(float *, int, float *, int);
extern void	spray_flecks(float *, int, int, int);
void	toke_pick_action(enemy_t *);
void	func_8013b018(void);
void	sub_8013b078(void);

#define MISL_TYPE	0x1901
#define WEAPON_MISSILE	0x34		/* weapons.h (not included: its effects.h has the two-argument burst_blood): MISL.ZM's parts 0x34 and 0x35 */
#define MISL_NVERTS	0x14e
#define MISL_NFACES	0x1a6
#define MISL_NNORMALS	0x158
#define MISL_BLOW	0xaa		/* a piece that reaches the player */

void	toke_misl_draw(game_object_t *);
int	toke_misl_init(game_object_t *);
void	toke_misl_think(game_object_t *);
int	toke_misl_hit(game_object_t *, player_stats_t *);
int	toke_misl_pop(game_object_t *, player_stats_t *);
void	toke_misl_remove(game_object_t *);

/* 0x80138320: a thrown piece of the given kind (its MISL part and colour) along the path
   points path..end, aimed by the camera, at the target player (the thrower passes its
   rotation, which is not used) */
void toke_throw_misl(float *rot, int kind, int path, int end)
{
	enemy_t	*m;
	short	id;

	id = add_object1_wrapper(MISL_TYPE, toke_misl_draw, toke_misl_init, toke_misl_think);
	if(id)
	{
		play_bigtop2_sound2(randrng(6) + 0x21, 0);
		m = get_object_data(id);
		m->target = get_target_player();
		set_goal_near_camera(m, camera_pos, m->vel, 2.0f, 0.0f, -9.0f);
		m->path = path;
		m->unk90 = path;
		m->unk92 = end;
		m->unka8 = kind;
		switch(kind)
		{
		case 4:
			m->unk20 = 0xff0000;
			break;
		case 3:
			m->unk20 = 0xffff00;
			break;
		case 2:
			m->unk20 = 0xff00ff;
			break;
		case 1:
			m->unk20 = 0xff00;
			break;
		case 0:
		default:
			m->unk20 = 0xff;
			break;
		}
		m->counter = 0x50;
	}
}

/* 0x8013844c: a thrown piece of the given kind along path..end, aimed at a point by the camera
   that depends on the kind (x 4, 2, -4, -2 or 0), at player 1, then at the target player */
void toke_throw_misl_spread(float *rot, int kind, int path, int end)
{
	enemy_t	*m;
	short	id;

	id = add_object1_wrapper(MISL_TYPE, toke_misl_draw, toke_misl_init, toke_misl_think);
	if(id)
	{
		play_bigtop2_sound2(randrng(6) + 0x21, 0);
		m = get_object_data(id);
		m->target = &player_stats[0];
		m->path = path;
		m->unk90 = path;
		m->unk92 = end;
		m->unka8 = kind;
		switch(kind)
		{
		case 4:
			set_goal_near_camera(m, camera_pos, m->vel, 0.0f, 0.0f, 0.0f);
			m->unk20 = 0xff0000;
			break;
		case 3:
			set_goal_near_camera(m, camera_pos, m->vel, -2.0f, 0.0f, 0.0f);
			m->unk20 = 0xffff00;
			break;
		case 2:
			set_goal_near_camera(m, camera_pos, m->vel, -4.0f, 0.0f, 0.0f);
			m->unk20 = 0xff00ff;
			break;
		case 1:
			set_goal_near_camera(m, camera_pos, m->vel, 2.0f, 0.0f, 0.0f);
			m->unk20 = 0xff00;
			break;
		case 0:
		default:
			set_goal_near_camera(m, camera_pos, m->vel, 4.0f, 0.0f, 0.0f);
			m->unk20 = 0xff;
			break;
		}
		m->target = get_target_player();
		m->counter = 0x50;
	}
}

/* 0x801385fc: a kind 2 piece (0xff00ff) along path..end with the given velocity, for 120 frames */
void toke_throw_misl_at(float *rot, float *vel, int path, int end)
{
	enemy_t	*m;
	short	id;

	id = add_object1_wrapper(MISL_TYPE, toke_misl_draw, toke_misl_init, toke_misl_think);
	if(id)
	{
		m = get_object_data(id);
		m->target = &player_stats[0];
		m->path = path;
		m->unk90 = path;
		m->unk92 = end;
		m->unka8 = 2;
		m->vel[0] = vel[0];
		m->vel[1] = vel[1];
		m->vel[2] = vel[2];
		m->unk20 = 0xff00ff;
		m->target = get_target_player();
		m->counter = 0x78;
	}
}

/* 0x801386d0: a piece's draw callback: its kind's MISL part */
void toke_misl_draw(game_object_t *obj)
{
	enemy_t	*m;

	m = (enemy_t *)obj->data;
	draw_model(m, &missile_submesh[m->unka8], 1, missile_verts, MISL_NVERTS, missile_faces, MISL_NFACES, missile_normals, MISL_NNORMALS);
}

/* 0x80138740: a piece's hit test: its sphere about its group's pivot */
int toke_misl_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*m;

	m = (enemy_t *)obj->data;
	if(shot_hits_sphere(m->hit_group->pivot, m->radius * m->radius, p))
	{
		p->hit = 1;
		p->hit_id = obj->id;
		p->die = (void *)toke_misl_pop;
		p->hit_point[0] = p->aim[0];
		p->hit_point[1] = p->aim[1];
		p->hit_point[2] = p->aim[2];
	}
	return(0);
}

/* 0x801387cc: a piece shot down: a sound, flecks in its colour, 500 points, gone */
int toke_misl_pop(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*m;
	float	unused[2];		/* 8 bytes the original's frame reserves and never touches */

	m = (enemy_t *)obj->data;
	play_bigtop2_sound(randrng(3) + 0x1e, 0);
	burst_flecks(&m->x, ((unsigned char *)&m->unk20)[2], ((unsigned char *)&m->unk20)[1], ((unsigned char *)&m->unk20)[0]);
	award_points(p->index, 500);
	remove_game_object(obj->id);
	return(0);
}

/* 0x8013884c: a piece's init: one group (visible, placed by its own angles), spun 4.71 about y,
   at x 4 */
int toke_misl_init(game_object_t *obj)
{
	enemy_t	*m;
	void	*data;

	data = malloc(sizeof(enemy_t));
	obj->data = data;
	if(!data)
	{
		return(1);
	}
	obj->hit = toke_misl_hit;
	obj->die = toke_misl_pop;
	obj->remove = toke_misl_remove;
	m = (enemy_t *)obj->data;
	m->counter = 0;
	m->frame = 0;
	m->ai_state = 0;
	m->flags = 0xc;
	m->rx = 0;
	m->ry = 0;
	m->rz = 0;
	m->x = 4.0f;
	m->y = 0;
	m->z = 0;
	m->groups = malloc(sizeof(group_t));
	if(!m->groups)
	{
		free(m);
		return(1);
	}
	m->groups->flags = 0x9000;
	m->groups->pos[0] = 0;
	m->groups->pos[1] = 0;
	m->groups->pos[2] = 0;
	m->groups->pivot[0] = 0;
	m->groups->pivot[1] = 0;
	m->groups->pivot[2] = 0;
	m->groups->rot[0] = 0;
	m->groups->rot[1] = 4.71f;
	m->groups->rot[2] = 0;
	m->unkcc = 0.2f;
	set_enemy_scale(m, 1.0f);
	m->hit_group = m->groups;
	m->unk98 = 0;
	m->ai_state = 0;
	return(0);
}

/* 0x80138984: a piece's think: it spins; at the player it hurts (170) and goes; else it flies
   its path leaving flecks of its colour, and goes at the path's end or when its time is up */
void toke_misl_think(game_object_t *obj)
{
	enemy_t	*m;
	float	unused[2];		/* 8 bytes the original's frame reserves and never touches */

	m = (enemy_t *)obj->data;
	m->groups->rot[0] += 0.3f;
	if(near_camera(m, 15.0f))
	{
		hurt_player(attack_target(m), MISL_BLOW, 3, 0.0f);
		remove_game_object(obj->id);
		return;
	}
	path_place_by_camera(m, toke_misl_path, m->path, 1.0f);
	fleck_enemy(m, ((unsigned char *)&m->unk20)[2], ((unsigned char *)&m->unk20)[1], ((unsigned char *)&m->unk20)[0]);
	if(++m->path > m->unk92)
	{
		m->path = m->unk90;
		remove_game_object(obj->id);
	}
	m->counter--;
	if(!m->counter)
	{
		remove_game_object(obj->id);
	}
}

/* 0x80138a8c: a piece's remove callback */
void toke_misl_remove(game_object_t *obj)
{
	enemy_t	*m;

	m = (enemy_t *)obj->data;
	free(m->groups);
	free(m);
}

/* 0x80138ac4: the draw callback (type 0x2066): while he is shown, his model, the attached
   effects, and while alive his life bar and name plate */
void func_80138ac4(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(!toke_shown)
	{
		return;
	}
	draw_model(e, submesh_info_toke, TOKE_GROUPS, toke_verts, TOKE_NVERTS, toke_faces, TOKE_NFACES, toke_normals, TOKE_NNORMALS);
	draw_effects(e);
	if(!(e->ai_state & 0x6000))
	{
		draw_life_bar(e->health, toke_full_health, 100.0f, 300.0f, 248.0f, 0x1010a0);
		toke_draw_name(200.0f, 239.0f);
	}
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[9].pos, e->radius, 0x600000);
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

/* 0x80138d34: the hit test, while he is shown: the bounding sphere, then the submesh */
int toke_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(toke_shown)
	{
		if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
		{
			group = find_submesh_hit(e, submesh_info_toke, TOKE_GROUPS, p);
			if(group != -1)
			{
				p->hit = 1;
				p->hit_id = obj->id;
				p->hit_group = group;
				p->die = (void *)toke_die;
				p->hit_point[0] = p->aim[0];
				p->hit_point[1] = p->aim[1];
				p->hit_point[2] = p->aim[2];
			}
		}
	}
	return(0);
}

/* 0x80138dfc: the shot lands. At the highest violence (once toke_gore is set) parts give way
   after 8 and 16 shots (groups 6/20 and 9/19/21 swapped for damaged ones, 14 and 1 gory, 8
   gory at once), each with a cry. Damage (halved, then none, near the end once
   toke_damage_scaled is set) and 100 points; at 0 the level is won: audit 0x4d, both players'
   guns off and their states ended, 4000 points and one of two deaths. Else a hit reaction */
int toke_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;
	float	damage;

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return(0);
	}
	add_world_sprite(p->hit_point, 0, 0);
	group = p->hit_group;
	if(violence_level >= 3 && toke_gore)
	{
		switch(group)
		{
		case 6:
		case 20:
			switch(++toke_hits_head)
			{
			case 8:
				play_tokken2_sound(0x4c, 0);
				e->groups[6].flags |= 0xf0000000;
				break;
			case 16:
				play_tokken2_sound(0x52, 0);
				replace_group(e, &e->groups[6], &e->groups[20]);
				e->groups[5].flags = 0;
				break;
			}
			break;
		case 9:
		case 19:
		case 21:
			switch(++toke_hits_body)
			{
			case 8:
				play_tokken2_sound(0x56, 0);
				replace_group(e, &e->groups[9], &e->groups[21]);
				break;
			case 16:
				play_tokken2_sound(0x4d, 0);
				replace_group(e, &e->groups[21], &e->groups[19]);
				break;
			}
			break;
		case 8:
			e->groups[8].flags |= 0xf0000000;
			break;
		case 14:
			if(++toke_hits_14 == 8)
			{
				play_tokken2_sound(0x4b, 0);
				e->groups[14].flags |= 0x10000000;
			}
			break;
		case 1:
			if(++toke_hits_1 == 8)
			{
				play_tokken2_sound(0x4f, 0);
				e->groups[1].flags |= 0x20000000;
			}
			break;
		}
	}
	if(!(e->ai_state & 0x6000))
	{
		damage = shot_damage_shared(p);
		if(toke_damage_scaled)
		{
			if(e->health / toke_full_health < 0.08f)
			{
				damage = 0;
			}
			else if(e->health / toke_full_health < 0.2f)
			{
				damage *= 0.5f;
			}
		}
		e->health -= damage;
		if(e->health <= 0.0f)
		{
			increment_audit(0x4d);
			player_stats[0].unkd4 = 100;
			player_stats[1].unkd4 = 100;
			play_tokken2_sound(0x60, 8);
			disable_guns();
			player_stats[0].clicks = 0;
			player_stats[1].clicks = 0;
			if(player_stats[0].state & 0xc)
			{
				player_stats[0].state = 0x40;
			}
			else
			{
				player_stats[0].state = 0x80;
			}
			if(player_stats[1].state & 0xc)
			{
				player_stats[1].state = 0x40;
			}
			else
			{
				player_stats[1].state = 0x80;
			}
			e->ry = 1.570796f;
			award_points(p->index, 4000);
			e->killer = p;
			mark_die(obj->id, p);
			if(violence_level >= 3)
			{
				e->ai_state = 0x2001;
				e->flags &= 0xfffd;
				e->frame = 0xc4;
			}
			else
			{
				e->ai_state = 0x2002;
				e->flags &= 0xfffd;
				e->frame = 0xf6;
			}
		}
		else
		{
			award_points(p->index, 100);
		}
	}
	if(!(e->ai_state & 0xe000))
	{
		switch(group)
		{
		case 0:
		case 1:
		case 15:
			start_hit_reaction(e, 0x156, 0x1005, submesh_info_toke, 8);
			break;
		case 12:
		case 13:
		case 14:
			start_hit_reaction(e, 0x140, 0x1006, submesh_info_toke, 8);
			break;
		case 6:
		case 20:
			if(randrng(100) < 50)
			{
				start_hit_reaction(e, 0xa7, 0x1003, submesh_info_toke, 8);
			}
			else
			{
				start_hit_reaction(e, 0xa7, 0x1004, submesh_info_toke, 8);
			}
			break;
		case 8:
		case 9:
		case 19:
		case 21:
			if(++toke_reactions & 1)
			{
				start_hit_reaction(e, 0x140, 0x1001, submesh_info_toke, 8);
			}
			else
			{
				start_hit_reaction(e, 0x156, 0x1002, submesh_info_toke, 8);
			}
			break;
		}
	}
	return(0);
}

/* 0x801392fc, 0x8013932c */
void load_toke_texmap(void)
{
	load_texmap(toke_texmap, 0x3b, "toke");
}

void remove_toke_texmap(void)
{
	remove_surface(toke_texmap, 0x3b);
}

/* 0x80139354: the model, the animation and the thrown pieces' path; groups 19 and 21 take
   group 9's animation, group 20 group 6's */
void load_toke(void)
{
	load_model((void **)&toke_verts, TOKE_NVERTS, (void **)&toke_faces, TOKE_NFACES, (void **)&toke_normals, TOKE_NNORMALS, "TOKE.ZM", 0x6b476043);
	load_anim(submesh_info_toke, TOKE_GROUPS, TOKE_FRAMES, "toke.za", 0x1d31369f);
	load_path(&toke_misl_path, MISL_PATH_LEN, "misl.pth", 0xf41fe989);
	free(submesh_info_toke[19].anim);
	free(submesh_info_toke[21].anim);
	free(submesh_info_toke[20].anim);
	submesh_info_toke[19].anim = submesh_info_toke[9].anim;
	submesh_info_toke[21].anim = submesh_info_toke[9].anim;
	submesh_info_toke[20].anim = submesh_info_toke[6].anim;
}

/* 0x80139444 (wave.c calls it by this name; proposed remove_toke_assets): the assets freed,
   the borrowed animations dropped first so they are not freed twice */
void func_80139444(void)
{
	submesh_info_toke[19].anim = 0;
	submesh_info_toke[21].anim = 0;
	submesh_info_toke[20].anim = 0;
	remove_surface(toke_texmap, 0x3b);
	remove_path(&toke_misl_path, MISL_PATH_LEN, "misl.pth");
	remove_animation(submesh_info_toke, TOKE_GROUPS, TOKE_FRAMES, "toke.za");
	remove_model((void **)&toke_verts, TOKE_NVERTS, (void **)&toke_faces, TOKE_NFACES, (void **)&toke_normals, TOKE_NNORMALS, "TOKE.ZM");
}

/* 0x801394f4: the init callback: 540 health (by the adjustment), scale 0.68, the hit part
   group 8; the level's TOKE state is reset; audit 0x4c */
int func_801394f4(game_object_t *obj)
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
	toke_enemy = e;
	obj->hit = toke_hit;
	obj->die = toke_die;
	obj->remove = toke_remove;
	e->counter = 0;
	e->unk4 = 0x14;
	e->frame = 0;
	e->num = 0;
	e->flags = 0xc;
	e->target = get_target_player();
	e->unk54 = 0;
	e->rx = 0;
	e->ry = 3.1415927f;
	e->rz = 0;
	e->x = 0;
	e->y = 0;
	e->z = 0;
	e->unkcc = 1.0f;
	set_enemy_scale(e, 0.68f);
	set_enemy_health2(e, 540.0f);
	toke_full_health = e->health;
	e->groups = malloc(TOKE_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < TOKE_GROUPS; i++)
	{
		e->groups[i].flags = 0xa400;
	}
	e->alpha = 255.0f;
	e->groups[19].flags = 0;
	e->groups[21].flags = 0;
	e->groups[20].flags = 0;
	e->groups[16].flags = 0;
	init_groups(e, submesh_info_toke, TOKE_GROUPS);
	g = e->groups;
	e->hit_group = &g[8];
	g[8].floor = 0;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0xf;
	e->prio_flags = next_cue_priority();
	toke_reactions = 0;
	toke_hits_head = 0;
	toke_hits_body = 0;
	D_801fad1c = 0;
	toke_hits_1 = 0;
	toke_hits_14 = 0;
	D_801e5dc4 = 0;
	toke_gore = 0;
	D_801de94c = 0;
	toke_shown = 1;
	D_801de8b4 = 0;
	toke_damage_scaled = 1;
	D_801e0e80 = 2;
	D_801e0e84 = 0;
	D_801e0e88 = 0;
	D_801e0e8c = 0;
	D_801e0e90 = 0;
	D_801e0e94 = 0;
	D_801e0e9c = 0;
	D_801e0ea8 = 0;
	D_801e5830 = 0;
	D_801e57dc = 0;
	func_8013aa34(e, 0);
	increment_audit(0x4c);
	toke_spray_r = 0xff;
	toke_spray_g = 0xff;
	toke_spray_b = 0xff;
	return(0);
}

/* 0x8013979c: the remove callback */
void toke_remove(game_object_t *obj)
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

/* The think loads the debug word afresh for its second test, which the compiler does only for
   a different declaration of it: the original reached the one word both as the array element
   (debug_mode, credits.h) and as a plain int at its address (the symbol debug_mode,
   config/symbols_abs.ld). The port has only the array. */
#ifdef PORT_HOST
#define debug_word	game_state[3]
#else
extern int	debug_word __asm__("debug_mode");
#endif

/* 0x801397f4: the think callback. The states: 0 and 3 loop (0x368 and 0x52 frames), 2 holds
   frame 0x51; 0x1001-0x1006 hit reactions blend back to resume_frame/resume_state; 0x201,
   0x202 and 0x820a face the camera (not 0x820a) and shoot from group 16 (a flash, bigtop2 call
   0x1b, the target hurt 12 the next frame); 0x8203 calls (func_8013b94c) and hurts the target
   3 if group 17 is near the camera; 0x8005 sets group 6's texture bit 0x1000000 for frames
   10-0x47 (with call 7), then cry 0x2f and the vanish (func_8013b018); 0x8006 fades, bursts
   in coloured smoke and hides him, then state 3 (sub_8013b078); 0x8204 loops 0x16f-0x1c3;
   0x2001/0x2002 end him (a magenta light, flecks and a sprite on the hit group, the movie
   script skipped on, 0x2001 also starts bigtop2 music 0x1c and drops chunks, blood from his
   attached groups, then state 0x4000); 0x8206, 0x820b,
   0x820c and 0x820d lob MISL parts from his hands (groups 18, 17); 0x8207, 0x8208, 0x820e
   and 0x820f throw the coloured pieces along misl.pth; 0x8209 sprays cycling-colour flecks
   from both hands. Each attack ends in toke_pick_action. */
void func_801397f4(game_object_t *obj)
{
	enemy_t	*e;
	float	pt[3];
	int	i, r, g, b;

	if(debug_mode & 0x10000)
	{
		return;
	}
	if(debug_word & 0x80000)
	{
		return;
	}
	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, TOKE_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1003:
		if(++e->frame >= 0xc4)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_toke, 8, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1004:
		if(++e->frame >= 0xc4)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_toke, 8, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1002:
		if(++e->frame >= 0x16f)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_toke, 8, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1005:
		if(++e->frame >= 0x16f)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_toke, 8, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1001:
	case 0x1006:
		if(++e->frame >= 0x156)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_toke, 8, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x201:
		face_camera(e, 0.2f);
		if(++e->frame == 0x90)
		{
			pt[0] = 0.6f;
			pt[1] = -0.1f;
			pt[2] = 0.3f;
			group_point(&e->groups[16], pt);
			add_world_sprite(pt, 4, 0);
			add_world_sprite(pt, 2, 0);
			play_bigtop2_sound(0x1b, 0);
		}
		if(e->frame == 0x91)
		{
			hurt_player(e->target, 0x23, 12, 0);
		}
		if(e->frame >= 0xa7)
		{
			e->frame = 0x7a;
			e->target = get_target_player();
		}
		break;
	case 0x202:
		face_camera(e, 0.2f);
		if(++e->frame == 0x34f)
		{
			pt[0] = 0.6f;
			pt[1] = -0.1f;
			pt[2] = 0.3f;
			group_point(&e->groups[16], pt);
			add_world_sprite(pt, 4, 0);
			add_world_sprite(pt, 2, 0);
			play_bigtop2_sound(0x1b, 0);
		}
		if(e->frame == 0x350)
		{
			hurt_player(e->target, 0x23, 12, 0);
		}
		if(e->frame >= 0x368)
		{
			e->frame = 0x34c;
			e->target = get_target_player();
		}
		break;
	case 0x820a:
		if(++e->frame == 0x283)
		{
			pt[0] = 0.6f;
			pt[1] = -0.1f;
			pt[2] = 0.3f;
			group_point(&e->groups[16], pt);
			add_world_sprite(pt, 4, 0);
			add_world_sprite(pt, 2, 0);
			play_bigtop2_sound(0x1b, 0);
		}
		if(e->frame == 0x284)
		{
			hurt_player(e->target, 0x23, 12, 0);
		}
		if(e->frame == 0x2ba)
		{
			func_8013b94c(e);
		}
		if(e->frame >= 0x2c4)
		{
			e->target = get_target_player();
			e->frame = 0x27e;
		}
		break;
	case 0x8203:
		face_camera(e, 0.2f);
		if(++e->frame == 0x120)
		{
			func_8013b94c(e);
		}
		if(e->frame == 0x133 && group_near_camera(&e->groups[17], 20.0f))
		{
			hurt_player(e->target, 0x23, 3, 0);
		}
		if(e->frame >= 0x140)
		{
			e->frame = 0x11e;
		}
		break;
	case 3:
		if(++e->frame >= 0x52)
		{
			e->frame = 0;
		}
		break;
	case 2:
		if(++e->frame >= 0x52)
		{
			e->frame = 0x51;
		}
		break;
	case 0x8005:
		if(++e->frame == 10)
		{
			e->groups[6].flags |= 0x1000000;
			play_tokken2_sound(7, 0);
		}
		if(e->frame == 0x48)
		{
			e->groups[6].flags &= ~0x1000000;
		}
		if(e->frame >= 0x52)
		{
			func_8013b018();
		}
		break;
	case 0x8006:
		if(++e->frame >= 0x5e)
		{
			e->alpha *= 0.9f;
		}
		if(e->frame == 0x60)
		{
			pt[0] = e->x;
			pt[1] = e->y + 2.3f;
			pt[2] = e->z;
			add_world_sprite(pt, 10, 0);
			play_bigtop2_sound(0x1d, 0);
		}
		if(e->frame >= 0x5e && e->frame <= 0x62)
		{
			for(i = 0; i < 5; i++)
			{
				pt[0] = func_800fe8dc(3.0f) + e->x;
				pt[1] = e->y;
				pt[2] = func_800fe8dc(3.0f) + e->z;
				r = randrng(0xff);
				g = randrng(0xff);
				b = randrng(0xff);
				add_smoke(pt, 0xff000000 | (r & 0xff) << 16 | (g & 0xff) << 8 | (b & 0xff));
			}
		}
		if(e->frame >= 0x7a)
		{
			toke_shown = 0;
			sub_8013b078();
		}
		break;
	case 0x8204:
		if(++e->frame >= 0x1c4)
		{
			e->frame = 0x16f;
		}
		break;
	case 0x2001:
		if(++e->frame == 0xc5)
		{
			add_dynamic_light(0x11, e->hit_group->pivot, D_801a1110, 12.0f, 15);
			burst_flecks(e->hit_group->pivot, 0xff, 0, 0xff);
			burst_flecks(e->hit_group->pivot, 0xff, 0, 0xff);
			burst_flecks(e->hit_group->pivot, 0xff, 0, 0xff);
			add_world_sprite(e->hit_group->pivot, 11, 0);
		}
		if(e->frame == 0xcc)
		{
			movie_script_skip();
		}
		if(e->frame == 0xd4)
		{
			play_level_music(0x1c, "bigtop2");
		}
		if(e->frame >= 0xd4 && e->frame <= 0xdc)
		{
			for(i = 0; i < TOKE_GROUPS; i++)
			{
				if(e->groups[i].flags & GROUP_ROOT)
				{
					add_chunks_n(e->groups[i].pivot, e->y - 100.0f, 1);
					burst_blood(e->groups[i].pivot, e->y - 100.0f, 0x800000);
				}
			}
		}
		if(e->frame >= 0xf6)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		break;
	case 0x2002:
		if(++e->frame == 0xf7)
		{
			add_dynamic_light(0x11, e->hit_group->pivot, D_801a1110, 12.0f, 15);
			burst_flecks(e->hit_group->pivot, 0xff, 0, 0xff);
			burst_flecks(e->hit_group->pivot, 0xff, 0, 0xff);
			burst_flecks(e->hit_group->pivot, 0xff, 0, 0xff);
			add_world_sprite(e->hit_group->pivot, 11, 0);
		}
		if(e->frame == 0xfe)
		{
			movie_script_skip();
		}
		if(e->frame >= 0x106 && e->frame <= 0x10e)
		{
			for(i = 0; i < TOKE_GROUPS; i++)
			{
				if(e->groups[i].flags & GROUP_ROOT)
				{
					burst_blood(e->groups[i].pivot, e->y - 100.0f, 0x800000);
				}
			}
		}
		if(e->frame >= 0x11e)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		break;
	case 0:
		if(++e->frame >= TOKE_FRAMES)
		{
			e->frame = 0;
		}
		break;
	case 0x8206:
		if(++e->frame == 0x1ce)
		{
			attach_effect(e, attach_held_object(&e->groups[18], WEAPON_MISSILE + 1, 0.19f, 0.07f, 0.0f, 0.28f, 0.56f, -0.66f, e->scale));
		}
		if(e->frame == 0x1df)
		{
			lob_at_camera(e->groups[18].pivot, WEAPON_MISSILE + 1, e->scale, (int)&e->rx);
			detach_effect(e, &e->groups[18]);
		}
		if(e->frame == 0x1d3)
		{
			attach_effect(e, attach_held_object(&e->groups[17], WEAPON_MISSILE + 1, 0.15f, -0.15f, 0.0f, -1.6f, -0.72f, 0.0f, e->scale));
		}
		if(e->frame == 0x1e3)
		{
			lob_at_camera(e->groups[17].pivot, WEAPON_MISSILE + 1, e->scale, (int)&e->rx);
			detach_effect(e, &e->groups[17]);
		}
		if(e->frame >= 0x1ea)
		{
			toke_pick_action(e);
		}
		break;
	case 0x820b:
		if(++e->frame == 0x33b)
		{
			attach_effect(e, attach_held_object(&e->groups[18], WEAPON_MISSILE + 1, 0.19f, 0.07f, 0.0f, 0.28f, 0.56f, -0.66f, e->scale));
		}
		if(e->frame == 0x340)
		{
			lob_at_camera(e->groups[18].pivot, WEAPON_MISSILE + 1, e->scale, (int)&e->rx);
			detach_effect(e, &e->groups[18]);
		}
		if(e->frame == 0x348)
		{
			attach_effect(e, attach_held_object(&e->groups[17], WEAPON_MISSILE + 1, 0.15f, -0.15f, 0.0f, -1.6f, -0.72f, 0.0f, e->scale));
		}
		if(e->frame == 0x34c)
		{
			lob_at_camera(e->groups[17].pivot, WEAPON_MISSILE + 1, e->scale, (int)&e->rx);
			detach_effect(e, &e->groups[17]);
		}
		if(e->frame >= 0x34c)
		{
			toke_pick_action(e);
		}
		break;
	case 0x820c:
		if(++e->frame == 0x2d3)
		{
			attach_effect(e, attach_held_object(&e->groups[17], WEAPON_MISSILE, 0.15f, -0.15f, 0.0f, -1.6f, -0.72f, 0.0f, e->scale));
		}
		if(e->frame == 0x2e1)
		{
			lob_at_camera(e->groups[17].pivot, WEAPON_MISSILE, e->scale, (int)&e->rx);
			detach_effect(e, &e->groups[17]);
		}
		if(e->frame >= 0x2fa)
		{
			toke_pick_action(e);
		}
		break;
	case 0x820d:
		if(++e->frame == 0x30a)
		{
			attach_effect(e, attach_held_object(&e->groups[18], WEAPON_MISSILE, 0.19f, 0.07f, 0.0f, 0.28f, 0.56f, -0.66f, e->scale));
		}
		if(e->frame == 0x319)
		{
			lob_at_camera(e->groups[18].pivot, WEAPON_MISSILE, e->scale, (int)&e->rx);
			detach_effect(e, &e->groups[18]);
		}
		if(e->frame >= 0x331)
		{
			toke_pick_action(e);
		}
		break;
	case 0x8207:
		switch(++e->frame)
		{
		case 0x1ef:
			toke_throw_misl(&e->rx, 4, 0, 0x31);
			break;
		case 0x1f4:
			toke_throw_misl(&e->rx, 3, 0x32, 0x63);
			break;
		case 0x1f9:
			toke_throw_misl(&e->rx, 2, 0x64, 0x95);
			break;
		case 0x1fe:
			toke_throw_misl(&e->rx, 1, 0x96, 0xc7);
			break;
		}
		if(e->frame >= 0x21a)
		{
			toke_pick_action(e);
		}
		break;
	case 0x8208:
		switch(++e->frame)
		{
		case 0x1ef:
			toke_throw_misl_spread(&e->rx, 2, 0x12c, 0x15d);
			toke_throw_misl_spread(&e->rx, 0, 0x190, 0x1c1);
			break;
		case 0x1f4:
			toke_throw_misl_spread(&e->rx, 3, 0xfa, 0x12b);
			toke_throw_misl_spread(&e->rx, 1, 0x15e, 0x18f);
			break;
		case 0x1f9:
			toke_throw_misl_spread(&e->rx, 4, 0xc8, 0xf9);
			break;
		}
		if(e->frame >= 0x21a)
		{
			toke_pick_action(e);
		}
		break;
	case 0x820e:
		switch(++e->frame)
		{
		case 0x254:
			toke_throw_misl(&e->rx, 2, 0x1c2, 0x1f3);
			break;
		case 0x259:
			toke_throw_misl(&e->rx, 0, 0x1f4, 0x225);
			break;
		case 0x25e:
			toke_throw_misl(&e->rx, 4, 0x226, 0x257);
			break;
		}
		if(e->frame >= 0x27e)
		{
			toke_pick_action(e);
		}
		break;
	case 0x820f:
		switch(++e->frame)
		{
		case 0x21f:
			toke_throw_misl(&e->rx, 3, 0x258, 0x289);
			break;
		case 0x224:
			toke_throw_misl(&e->rx, 0, 0x28a, 0x2bb);
			break;
		case 0x229:
			toke_throw_misl(&e->rx, 1, 0x2bc, 0x2ed);
			break;
		}
		if(e->frame >= 0x24f)
		{
			toke_pick_action(e);
		}
		break;
	case 0x8209:
		e->frame++;
		spray_flecks(e->groups[18].pivot, (unsigned char)toke_spray_r, (unsigned char)toke_spray_g, (unsigned char)toke_spray_b);
		spray_flecks(e->groups[17].pivot, (unsigned char)toke_spray_r, (unsigned char)toke_spray_g, (unsigned char)toke_spray_b);
		toke_spray_r += 1;
		toke_spray_g += 2;
		toke_spray_b += 3;
		if(e->frame >= 0x52)
		{
			toke_pick_action(e);
		}
		break;
	}
}

/* 0x8013aa34: the state set-up: each state's first frame */
void func_8013aa34(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	switch(e->ai_state)
	{
	case 0x8006:
		e->frame = 0x52;
		break;
	case 0x201:
		e->frame = 0x7a;
		break;
	case 0x8005:
		e->frame = 0;
		break;
	case 0:
		e->frame = 0;
		break;
	default:
		e->frame = 0;
		break;
	}
}

/* 0x8013aa88: a playing player to aim at; once D_801de8b4 is set, a random action from his
   table, and after three of them (or the 0x8209 one) a call from func_8013b94c's list */
void toke_pick_action(enemy_t *e)
{
	player_stats_t	*p;

	if(e->target->state != 4)
	{
		p = get_target_player();
		if(p->state == 4)
		{
			e->target = p;
		}
	}
	if(D_801de8b4)
	{
		toke_action_picks++;
		pick_random_action(e, toke_actions, submesh_info_toke, 8, 2);
		if(toke_action_picks >= 3 || e->ai_state == 0x8209)
		{
			toke_action_picks = 0;
			func_8013b94c(e);
		}
	}
}

/* 0x8013ab54: back to standing (state 2) */
void toke_stand(enemy_t *e)
{
	e->frame--;
	e->unk6 = 0;
	blend_to_frame(e, submesh_info_toke, 8, 0, 2);
	e->ai_state = 2;
}

/* 0x8013abac, 0x8013ac0c: groups 18 and 10 knocked loose from group 10, groups 17 and 3 from
   group 3 (no caller in the image) */
void toke_drop_right_arm(enemy_t *e)
{
	detach_group(&e->groups[18], e->groups[10].pos, e->y, e->scale);
	detach_group(&e->groups[10], e->groups[10].pos, e->y, e->scale);
}

void toke_drop_left_arm(enemy_t *e)
{
	detach_group(&e->groups[17], e->groups[3].pos, e->y, e->scale);
	detach_group(&e->groups[3], e->groups[3].pos, e->y, e->scale);
}

/* 0x8013ac6c: the name beside his life bar in two halves (textures 0x7086 and 0x7087), each
   80 by 16, 41.6 left and right of x */
void toke_draw_name(float x, float y)
{
	struct { float x, y, oow, sow, tow, pad; } v[4];
	float	x0;

	guTexSource(find_surface(0x7086)->handle);
	grConstantColorValue(0xa0a0a0);
	guColorCombineFunction(5);
	guAlphaSource(2);
	grAlphaTestReferenceValue(4);
	x0 = x;
	x = x - 41.6f;
	v[0].x = x - 40.0f;
	v[0].y = y - 8.0f;
	v[0].oow = 0.909090877f;
	v[0].sow = 0.909090877f;
	v[0].tow = 57.2727242f;
	v[1].x = x + 40.0f;
	v[1].y = y - 8.0f;
	v[1].oow = 0.909090877f;
	v[1].sow = 231.818176f;
	v[1].tow = 57.2727242f;
	v[2].x = x - 40.0f;
	v[2].y = y + 8.0f;
	v[2].oow = 0.909090877f;
	v[2].sow = 0.909090877f;
	v[2].tow = 0.909090877f;
	v[3].x = x + 40.0f;
	v[3].y = y + 8.0f;
	v[3].oow = 0.909090877f;
	v[3].sow = 231.818176f;
	v[3].tow = 0.909090877f;
	func_800d326c(&v[0], &v[3], &v[1]);
	func_800d326c(&v[0], &v[2], &v[3]);
	guTexSource(find_surface(0x7087)->handle);
	x = x0 + 41.6f;
	v[0].x = x - 40.0f;
	v[0].y = y - 8.0f;
	v[0].oow = 0.909090877f;
	v[0].sow = 0.909090877f;
	v[0].tow = 57.2727242f;
	v[1].x = x + 40.0f;
	v[1].y = y - 8.0f;
	v[1].oow = 0.909090877f;
	v[1].sow = 231.818176f;
	v[1].tow = 57.2727242f;
	v[2].x = x - 40.0f;
	v[2].y = y + 8.0f;
	v[2].oow = 0.909090877f;
	v[2].sow = 0.909090877f;
	v[2].tow = 0.909090877f;
	v[3].x = x + 40.0f;
	v[3].y = y + 8.0f;
	v[3].oow = 0.909090877f;
	v[3].sow = 231.818176f;
	v[3].tow = 0.909090877f;
	func_800d326c(&v[0], &v[3], &v[1]);
	func_800d326c(&v[0], &v[2], &v[3]);
}

/* 0x8013ae84: the script's state change for TOKE: to the state with the given first frame;
   during a blend the frame is set at once (the blend's target kept as the resume point), else
   a blend of 2 to it */
void toke_set_state(int frame, int state)
{
	enemy_t	*e;

	e = toke_enemy;
	if(state == e->ai_state)
	{
		return;
	}
	if(e->flags & 2)
	{
		e->flags &= 0xfffd;
		e->resume_state = e->ai_state;
		e->resume_frame = e->frame;
		e->frame = frame;
	}
	else
	{
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_toke, 8, frame, 2);
	}
	e->ai_state = state;
}

/* ---- the movie script's callbacks for TOKE (0x8013af24-0x8013b9c8) ---- */

/* 0x8013af24: from here his parts can be damaged */
void sub_8013af24(void)
{
	toke_gore = 1;
}

/* 0x8013af38, 0x8013af5c: shown (at full alpha), hidden */
void sub_8013af38(void)
{
	toke_shown = 1;
	toke_enemy->alpha = 255.0f;
}

void func_8013af5c(void)
{
	toke_shown = 0;
}

/* 0x8013af6c, 0x8013af84: group 16 drawn (0xa400) or hidden */
void sub_8013af6c(void)
{
	toke_enemy->groups[16].flags = 0xa400;
}

void sub_8013af84(void)
{
	toke_enemy->groups[16].flags = 0;
}

/* 0x8013af98 */
void func_8013af98(void)
{
	toke_set_state(0, 0x8005);
}

/* 0x8013afbc: a call from his first list, then state 0x8006 from frame 0x52 (he fades and
   vanishes in coloured smoke, see the think), the pieces held on groups 18 and 17 let go */
void sub_8013afbc(void)
{
	enemy_t	*e;

	e = toke_enemy;
	func_8013b8d8(e);
	toke_set_state(0x52, 0x8006);
	detach_effect(e, &e->groups[18]);
	detach_effect(e, &e->groups[17]);
}

/* 0x8013b018: the same with cry 0x2f */
void func_8013b018(void)
{
	enemy_t	*e;

	e = toke_enemy;
	play_tokken2_sound(0x2f, 2);
	toke_set_state(0x52, 0x8006);
	detach_effect(e, &e->groups[18]);
	detach_effect(e, &e->groups[17]);
}

/* 0x8013b078, 0x8013b09c */
void sub_8013b078(void)
{
	toke_set_state(0, 3);
}

void sub_8013b09c(void)
{
	toke_set_state(0x7a, 0x201);
}

/* 0x8013b0c0: his random actions start */
void sub_8013b0c0(void)
{
	D_801de8b4 = 1;
	toke_pick_action(toke_enemy);
}

/* 0x8013b0f4: state 0x8203 at frame 0x11e at once */
void sub_8013b0f4(void)
{
	enemy_t	*e;

	e = toke_enemy;
	e->ai_state = 0x8203;
	e->flags &= 0xfffd;
	e->frame = 0x11e;
}

/* 0x8013b11c: placed at (-140.52, 102.38, 1.42) facing 1.57, cry 0x44, state 0x8204 */
void sub_8013b11c(void)
{
	enemy_t	*e;

	e = toke_enemy;
	e->x = -140.520004f;
	e->y = 102.379997f;
	e->z = 1.41999996f;
	e->rx = 0;
	e->ry = 1.570796f;
	e->rz = 0;
	play_tokken2_sound(0x44, 4);
	toke_set_state(0x16f, 0x8204);
}

/* 0x8013b18c, 0x8013b1b0, 0x8013b1d4 */
void sub_8013b18c(void)
{
	toke_set_state(0x27e, 0x820a);
}

void sub_8013b1b0(void)
{
	toke_set_state(0x34c, 0x202);
}

void sub_8013b1d4(void)
{
}

/* 0x8013b1dc-0x8013b49c: the tests the Big Top script waits on: each makes him vanish (sub_8013afbc)
   once, the ones with a health fraction when his health first falls below it; 0x8013b454
   ends the damage scaling below 10%, 0x8013b49c says he is dead */
void func_8013b1dc(void)
{
	if(!D_801e0e84)
	{
		D_801e0e84 = 1;
		sub_8013afbc();
	}
}

int func_8013b214(void)
{
	if(D_801e0e88)
	{
		return(1);
	}
	if(toke_enemy->health / toke_full_health < 0.8f)
	{
		D_801e0e88 = 1;
		sub_8013afbc();
		return(1);
	}
	return(0);
}

void func_8013b288(void)
{
	if(!D_801e0e8c)
	{
		D_801e0e8c = 1;
		sub_8013afbc();
	}
}

void func_8013b2c0(void)
{
	if(!D_801e0e90)
	{
		D_801e0e90 = 1;
		sub_8013afbc();
	}
}

int func_8013b2f8(void)
{
	if(D_801e0e94)
	{
		return(1);
	}
	if(toke_enemy->health / toke_full_health < 0.6f)
	{
		D_801e0e94 = 1;
		sub_8013afbc();
		return(1);
	}
	return(0);
}

int func_8013b36c(void)
{
	if(D_801e5830)
	{
		return(1);
	}
	if(toke_enemy->health / toke_full_health < 0.5f)
	{
		D_801e5830 = 1;
		sub_8013afbc();
		return(1);
	}
	return(0);
}

int func_8013b3e0(void)
{
	if(D_801e57dc)
	{
		return(1);
	}
	if(toke_enemy->health / toke_full_health < 0.3f)
	{
		D_801e57dc = 1;
		sub_8013afbc();
		return(1);
	}
	return(0);
}

int func_8013b454(void)
{
	if(toke_enemy->health / toke_full_health < 0.1f)
	{
		toke_damage_scaled = 0;
		return(1);
	}
	return(0);
}

int func_8013b49c(void)
{
	if(toke_enemy->health / toke_full_health <= 0.0f)
	{
		return(1);
	}
	return(0);
}

/* 0x8013b4d4: TOKE placed at (-22.7999992f, -127.550003f, 1.10000002f) facing 7.6500001f */
void func_8013b4d4(void)
{
	enemy_t	*e;

	e = toke_enemy;
	e->x = -22.7999992f;
	e->y = -127.550003f;
	e->z = 1.10000002f;
	e->rx = 0;
	e->ry = 7.6500001f;
	e->rz = 0;
}

/* 0x8013b518: TOKE placed at (2.42000008f, -92.7699966f, -49.5299988f) facing 6.88000011f, with a cry */
void sub_8013b518(void)
{
	enemy_t	*e;

	e = toke_enemy;
	play_tokken2_sound(0x45, 0);
	e->x = 2.42000008f;
	e->y = -92.7699966f;
	e->z = -49.5299988f;
	e->rx = 0;
	e->ry = 6.88000011f;
	e->rz = 0;
}

/* 0x8013b584: TOKE placed at (68.8399963f, -10.5699997f, -78.3899994f) facing 7.94999981f */
void sub_8013b584(void)
{
	enemy_t	*e;

	e = toke_enemy;
	e->x = 68.8399963f;
	e->y = -10.5699997f;
	e->z = -78.3899994f;
	e->rx = 0;
	e->ry = 7.94999981f;
	e->rz = 0;
}

/* 0x8013b5c8: TOKE placed at (31.2199993f, 83.8199997f, -27.8600006f) facing 9.52999973f */
void sub_8013b5c8(void)
{
	enemy_t	*e;

	e = toke_enemy;
	e->x = 31.2199993f;
	e->y = 83.8199997f;
	e->z = -27.8600006f;
	e->rx = 0;
	e->ry = 9.52999973f;
	e->rz = 0;
}

/* 0x8013b60c: TOKE placed at (-126.900002f, 103.599998f, -1.44000006f) facing 10.8100004f, with a cry */
void sub_8013b60c(void)
{
	enemy_t	*e;

	e = toke_enemy;
	play_tokken2_sound(0x47, 0);
	e->x = -126.900002f;
	e->y = 103.599998f;
	e->z = -1.44000006f;
	e->rx = 0;
	e->ry = 10.8100004f;
	e->rz = 0;
}

/* 0x8013b678: TOKE placed at (-139.399994f, 36.0999985f, -102.339996f) facing 12.0f */
void sub_8013b678(void)
{
	enemy_t	*e;

	e = toke_enemy;
	e->x = -139.399994f;
	e->y = 36.0999985f;
	e->z = -102.339996f;
	e->rx = 0;
	e->ry = 12.0f;
	e->rz = 0;
}

/* 0x8013b6bc: TOKE placed at (-140.520004f, 102.379997f, 1.41999996f) facing 1.57079601f, with a cry */
void sub_8013b6bc(void)
{
	enemy_t	*e;

	e = toke_enemy;
	play_tokken2_sound(0x2d, 0);
	e->x = -140.520004f;
	e->y = 102.379997f;
	e->z = 1.41999996f;
	e->rx = 0;
	e->ry = 1.57079601f;
	e->rz = 0;
}

/* 0x8013b728: TOKE placed at (-217.050003f, -68.0500031f, 46.9000015f) facing 12.6499996f */
void sub_8013b728(void)
{
	enemy_t	*e;

	e = toke_enemy;
	e->x = -217.050003f;
	e->y = -68.0500031f;
	e->z = 46.9000015f;
	e->rx = 0;
	e->ry = 12.6499996f;
	e->rz = 0;
}

/* 0x8013b76c: TOKE placed at (-163.679993f, -90.3000031f, 3.69000006f) facing 13.5900002f */
void sub_8013b76c(void)
{
	enemy_t	*e;

	e = toke_enemy;
	e->x = -163.679993f;
	e->y = -90.3000031f;
	e->z = 3.69000006f;
	e->rx = 0;
	e->ry = 13.5900002f;
	e->rz = 0;
}

/* 0x8013b7b0: TOKE placed at (-4.21999979f, -125.650002f, 0) facing 7.6500001f, with a cry */
void sub_8013b7b0(void)
{
	enemy_t	*e;

	e = toke_enemy;
	play_tokken2_sound(0x12, 0);
	e->x = -4.21999979f;
	e->y = -125.650002f;
	e->z = 0;
	e->rx = 0;
	e->ry = 7.6500001f;
	e->rz = 0;
}

/* 0x8013b814: TOKE placed at (-4.21999979f, -125.650002f, 0) facing 7.6500001f */
void sub_8013b814(void)
{
	enemy_t	*e;

	e = toke_enemy;
	e->x = -4.21999979f;
	e->y = -125.650002f;
	e->z = 0;
	e->rx = 0;
	e->ry = 7.6500001f;
	e->rz = 0;
}

/* 0x8013b850: the end: the blood, chunks and world sprites cleared, one of his two deaths */
void sub_8013b850(void)
{
	enemy_t	*e;

	e = toke_enemy;
	free_blood_drops();
	free_chunks();
	free_world_sprites();
	if(violence_level >= 3)
	{
		e->ai_state = 0x2001;
		e->flags &= 0xfffd;
		e->frame = 0xce;
	}
	else
	{
		e->ai_state = 0x2002;
		e->flags &= 0xfffd;
		e->frame = 0x100;
	}
}

/* 0x8013b8d8, 0x8013b94c: the next "tokken" call from each of his two lists, in turn */
void func_8013b8d8(enemy_t *e)
{
	play_tokken2_sound(toke_calls1[toke_call1], 0);
	toke_call1 = (toke_call1 + 1) % 6;
}

void func_8013b94c(enemy_t *e)
{
	play_tokken2_sound(toke_calls2[toke_call2], 0);
	toke_call2 = (toke_call2 + 1) % 10;
}
