/*
 * taur.c - the Freak Show boss TAUR, whose name plate reads EYECLOPS (object type 0x2027; TAUR.ZM /
 * taur.za and the taur*.WMS texmaps, sound bank "taur"). One original file, 0x80118728-0x8011a170 (the word at
 * 0x80118724 before it is padding), rodata 0x8017362c-0x801736fc, tables 0x80195078-0x80195a70
 * (taur_data.c). The model has 748 vertices, 795 normals and 1282 faces in 26 submeshes, the
 * animation 483 frames. 300 health (set_enemy_health2), a life bar and a name plate.
 *
 * Seven small parts (groups 0-6) glow on and off (func_80119d58) and take the animations of the
 * groups they sit on (24, 25, 13, 7 and 23); at any violence a shot on one hides it, leaves a
 * spray and does ten times the damage. Halfway down its health it reels (0x1005).
 *
 * States: 0 the whole animation, 1 walking at the player, 2 walking to a fixed spot (taur_spot)
 * and then picking its next move, 4 its entrance (audit 0x3c, a roar), 6 a taunt, 0x201-0x205
 * five attacks (it chooses one at random and walks to its range first), 0x1001 and 0x1005 hit
 * reactions, 0x2001 dying (then 0x4000). Three hits on the player in a row and it taunts
 * instead of attacking; a shot resets the count.
 *
 * The exported names are the address names the shared files (spawn.c, wave.c) use:
 * func_80118728 is the draw callback, func_8011905c the init, func_80119280 the think,
 * func_80119894 the state set-up.
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
#include "taur.h"

extern char	text_buffer[];
extern int	violence_level;
extern enemy_t	*debug_enemy;
extern float	debug_p1x, debug_p1y, debug_p1z, debug_p2x, debug_p2y, debug_p2z;	/* scene.c */

extern texmap_t	taur_texmap[45];		/* taur_data.c */
extern submesh_t taur_submesh_info[26];
extern taur_glow_t taur_glow[7];
extern float	taur_spray1a[3], taur_spray1b[3], taur_spray2a[3], taur_spray2b[3], taur_spray3a[3], taur_spray3b[3];
extern float	taur_spray4a[3], taur_spray4b[3], taur_spray5a[3], taur_spray5b[3], taur_spray6a[3], taur_spray6b[3];
extern float	taur_spray7a[3], taur_spray7b[3];

extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);	/* text at a world position */
extern void	func_800c8938(float *, float, unsigned int);			/* a debug sphere */
extern void	func_800d14a4(float, float);					/* a debug map dot */
extern void	func_800c99e4(enemy_t *, int);
extern void	func_800d326c(void *, void *, void *);				/* a screen triangle */
extern void	draw_life_bar(float, float, float, float, float, unsigned int);
extern void	draw_effects(enemy_t *);			/* run the attached effects */
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	attach_effect(enemy_t *, void *);
extern void	*attach_spray(float *, float *, group_t *);
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health2(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern int	randrng(int);
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
extern float	shot_damage_shared(player_stats_t *);
extern void	award_points(int, int);
extern int	mark_die();							/* spawn.c: (id, killer) */
extern void	increment_audit(int);
extern void	init_groups(enemy_t *, submesh_t *, int);
extern unsigned int next_cue_priority(void);
extern void	play_enemy_sound_noint(enemy_t *, int, char *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *);

float	*taur_verts;			/* 0x801e5bc4 */
face_t	*taur_faces;			/* 0x8022b11c */
float	*taur_normals;			/* 0x801e5920 */
float	taur_full_health;		/* 0x801e57f0: for the life bar and the halfway reel */
int	taur_hits;			/* 0x801e5bf8: hits on the player since it was last shot */
int	taur_taunt;			/* 0x801de884: the next taunt (0-4) */
float	taur_spot[3];			/* 0x802374a0: where its walk (state 2) leads */

#define TAUR_GROUPS	0x1a
#define TAUR_NVERTS	0x2ec
#define TAUR_NFACES	0x502
#define TAUR_NNORMALS	0x31b
#define TAUR_FRAMES	0x1e3

int	func_80118a44(game_object_t *, player_stats_t *);
void	func_80119228(game_object_t *);
void	func_80119894(enemy_t *, unsigned short);
void	func_80119944(enemy_t *);
void	func_80119a5c(enemy_t *);
void	func_80119b18(enemy_t *);
void	func_80119b88(enemy_t *);
void	func_80119d58(enemy_t *);
void	func_80119f10(float, float);
void	func_8011a020(enemy_t *);

/* 0x80118728: the draw callback (type 0x2027): the glow, the model, its life bar and name */
void func_80118728(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	func_80119d58(e);
	draw_model(e, taur_submesh_info, TAUR_GROUPS, taur_verts, TAUR_NVERTS, taur_faces, TAUR_NFACES, taur_normals, TAUR_NNORMALS);
	draw_effects(e);
	if(!(e->ai_state & 0x6000))
	{
		func_80119f10(200.0f, 233.0f);
		draw_life_bar(e->health, taur_full_health, 150.0f, 250.0f, 248.0f, 0x908000);
	}
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[23].pos, e->radius, 0x600000);
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

/* 0x80118988: the hit test: the bounding sphere, then the submesh */
int func_80118988(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, taur_submesh_info, TAUR_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
			p->hit_group = group;
			p->die = (void *)func_80118a44;
		}
	}
	return(0);
}

/* 0x80118a44: the shot lands: blood; at any violence a small part (groups 0-6) is shot off
   (a spray, the part hidden) and takes ten times the damage; 2000 points, a cry and the death
   (audit 0x3d), else 50 points; then the reel when it falls below half its health, else a cry
   and a flinch for a small part (which also resets taur_hits) */
int func_80118a44(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;
	int	group;
	float	health;
	float	damage;

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
	if(violence_level > 0)
	{
		switch(group)
		{
		case 6:
			attach_effect(e, attach_spray(taur_spray1b, taur_spray1a, &e->groups[24]));
			g->flags = 0;
			break;
		case 5:
			attach_effect(e, attach_spray(taur_spray2b, taur_spray2a, &e->groups[25]));
			g->flags = 0;
			break;
		case 4:
			attach_effect(e, attach_spray(taur_spray3b, taur_spray3a, &e->groups[13]));
			g->flags = 0;
			break;
		case 0:
			attach_effect(e, attach_spray(taur_spray4b, taur_spray4a, &e->groups[7]));
			g->flags = 0;
			break;
		case 1:
			attach_effect(e, attach_spray(taur_spray5b, taur_spray5a, &e->groups[23]));
			g->flags = 0;
			break;
		case 2:
			attach_effect(e, attach_spray(taur_spray6b, taur_spray6a, &e->groups[23]));
			g->flags = 0;
			break;
		case 3:
			attach_effect(e, attach_spray(taur_spray7b, taur_spray7a, &e->groups[23]));
			g->flags = 0;
			break;
		}
	}
	health = e->health;
	if(!(e->ai_state & 0x6000))
	{
		damage = shot_damage_shared(p);
		if(violence_level > 0 && group < 7)
		{
			if(group >= 0)
			{
				damage *= 10.0f;
			}
		}
		e->health -= damage;
		if(e->health <= 0.0f)
		{
			award_points(p->index, 2000);
			e->killer = p;
			mark_die(obj->id, p);
			play_enemy_sound(e, 0x29, "taur", 8);
			e->ai_state = 0x2001;
			e->frame = 0x2f;
			e->flags &= 0xfffd;
			increment_audit(0x3d);
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
	if(taur_full_health * 0.5f < health && e->health <= taur_full_health * 0.5f)
	{
		play_enemy_sound(e, 0x16, "taur", 4);
		start_hit_reaction(e, 0x1cc, 0x1005, taur_submesh_info, 0x16);
	}
	else
	{
		switch(group)
		{
		case 6:
			taur_hits = 0;
			play_enemy_sound(e, 0x12, "taur", 4);
			start_hit_reaction(e, 0x61, 0x1001, taur_submesh_info, 0x16);
			break;
		case 5:
			taur_hits = 0;
			play_enemy_sound(e, 0x16, "taur", 4);
			start_hit_reaction(e, 0x61, 0x1001, taur_submesh_info, 0x16);
			break;
		case 4:
			taur_hits = 0;
			play_enemy_sound(e, 7, "taur", 4);
			start_hit_reaction(e, 0x61, 0x1001, taur_submesh_info, 0x16);
			break;
		case 0:
			taur_hits = 0;
			play_enemy_sound(e, 0xf, "taur", 4);
			start_hit_reaction(e, 0x61, 0x1001, taur_submesh_info, 0x16);
			break;
		case 1:
			taur_hits = 0;
			play_enemy_sound(e, 0x23, "taur", 4);
			start_hit_reaction(e, 0x61, 0x1001, taur_submesh_info, 0x16);
			break;
		case 2:
			taur_hits = 0;
			play_enemy_sound(e, 0x1f, "taur", 4);
			start_hit_reaction(e, 0x61, 0x1001, taur_submesh_info, 0x16);
			break;
		case 3:
			taur_hits = 0;
			play_enemy_sound(e, 0xa, "taur", 4);
			start_hit_reaction(e, 0x61, 0x1001, taur_submesh_info, 0x16);
			break;
		}
	}
	return(0);
}

/* 0x80118e14 */
void load_taur_texmap(void)
{
	load_texmap(taur_texmap, 0x2d, "taur");
}

/* 0x80118e44: the model and the animation; the seven small parts take the animations of the
   groups they sit on */
void load_taur(void)
{
	load_model((void **)&taur_verts, TAUR_NVERTS, (void **)&taur_faces, TAUR_NFACES, (void **)&taur_normals, TAUR_NNORMALS, "TAUR.ZM", 0x3dab0017);
	load_anim(taur_submesh_info, TAUR_GROUPS, TAUR_FRAMES, "taur.za", 0xa9f51b4a);
	free(taur_submesh_info[6].anim);
	free(taur_submesh_info[5].anim);
	free(taur_submesh_info[4].anim);
	free(taur_submesh_info[0].anim);
	free(taur_submesh_info[1].anim);
	free(taur_submesh_info[2].anim);
	free(taur_submesh_info[3].anim);
	taur_submesh_info[6].anim = taur_submesh_info[24].anim;
	taur_submesh_info[5].anim = taur_submesh_info[25].anim;
	taur_submesh_info[4].anim = taur_submesh_info[13].anim;
	taur_submesh_info[0].anim = taur_submesh_info[7].anim;
	taur_submesh_info[1].anim = taur_submesh_info[23].anim;
	taur_submesh_info[2].anim = taur_submesh_info[23].anim;
	taur_submesh_info[3].anim = taur_submesh_info[23].anim;
}

/* 0x80118f9c: the borrowed animations are dropped first so they are not freed twice */
void remove_taur_assets(void)
{
	taur_submesh_info[6].anim = 0;
	taur_submesh_info[5].anim = 0;
	taur_submesh_info[4].anim = 0;
	taur_submesh_info[0].anim = 0;
	taur_submesh_info[1].anim = 0;
	taur_submesh_info[2].anim = 0;
	taur_submesh_info[3].anim = 0;
	remove_surface(taur_texmap, 0x2d);
	remove_animation(taur_submesh_info, TAUR_GROUPS, TAUR_FRAMES, "taur.za");
	remove_model((void **)&taur_verts, TAUR_NVERTS, (void **)&taur_faces, TAUR_NFACES, (void **)&taur_normals, TAUR_NNORMALS, "TAUR.ZM");
}

/* 0x8011905c: the init callback (type 0x2027): the record, 26 groups, 300 health (the life
   bar's full), the first state; the debug points cleared */
int func_8011905c(game_object_t *obj)
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
	obj->hit = func_80118988;
	obj->die = func_80118a44;
	obj->remove = func_80119228;
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
	taur_taunt = 0;
	e->ry = 3.1415927f;
	e->unkcc = 1.3f;
	set_enemy_scale(e, 0.9f);
	set_enemy_health2(e, 300.0f);
	taur_full_health = e->health;
	e->groups = malloc(TAUR_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < TAUR_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	init_groups(e, taur_submesh_info, TAUR_GROUPS);
	g = e->groups;
	e->hit_group = &g[22];
	g[22].floor = 0;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	func_80119894(e, 0);
	taur_hits = 0;
	debug_p1x = debug_p1y = debug_p1z = debug_p2x = debug_p2y = debug_p2z = 0;
	return(0);
}

/* 0x80119228: the remove callback */
void func_80119228(game_object_t *obj)
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

/* 0x80119280: the think callback */
void func_80119280(game_object_t *obj)
{
	enemy_t	*e;

	if(debug_mode & 0x10000)
	{
		return;
	}
	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, TAUR_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	func_8011a020(e);
	switch(e->ai_state)
	{
	case 2:
		if(++e->frame >= 0x151)
		{
			e->frame = 0x138;
		}
		face_camera(e, 0.2f);
		e->goal[0] = taur_spot[0];
		e->goal[1] = taur_spot[1];
		e->goal[2] = taur_spot[2];
		if(step_toward_goal_xz(e, 0.6f) == 2)
		{
			func_80119944(e);
		}
		break;
	case 0x1001:
		if(++e->frame >= 0x78)
		{
			e->frame--;
			blend_to_frame(e, taur_submesh_info, 0x16, e->resume_frame, 5);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x1005:
		if(++e->frame >= 0x1e3)
		{
			e->frame--;
			blend_to_frame(e, taur_submesh_info, 0x16, e->resume_frame, 5);
			e->ai_state = e->resume_state;
		}
		break;
	case 1:
		if(++e->frame >= 0x2f)
		{
			e->frame = 0;
		}
		face_camera(e, 0.2f);
		func_80119b88(e);
		break;
	case 4:
		if(++e->frame >= 0x174)
		{
			face_camera(e, 0.2f);
			func_80119a5c(e);
		}
		break;
	case 0x201:
		face_camera(e, 0.2f);
		if(++e->frame == 0x90 && group_near_camera(&e->groups[15], 40.0f))
		{
			taur_hits++;
			hurt_player(e->target, 0x140, 7, 3.1415927f);
		}
		if(e->frame >= 0xab)
		{
			func_80119b18(e);
		}
		break;
	case 0x202:
		face_camera(e, 0.2f);
		if(++e->frame == 0x18b && group_near_camera(&e->groups[23], 40.0f))
		{
			taur_hits++;
			hurt_player(e->target, 0x140, 3, 0);
		}
		if(e->frame >= 0x1b9)
		{
			func_80119b18(e);
		}
		break;
	case 0x203:
		face_camera(e, 0.2f);
		if(++e->frame == 0xc3 && group_near_camera(&e->groups[15], 40.0f))
		{
			taur_hits++;
			hurt_player(e->target, 0x140, 7, 0);
		}
		if(e->frame >= 0xea)
		{
			func_80119b18(e);
		}
		break;
	case 0x204:
		face_camera(e, 0.2f);
		if(++e->frame == 0xf8 && group_near_camera(&e->groups[15], 40.0f))
		{
			taur_hits++;
			hurt_player(e->target, 0x140, 3, 0);
		}
		if(e->frame >= 0x112)
		{
			func_80119b18(e);
		}
		break;
	case 0x205:
		face_camera(e, 0.2f);
		switch(++e->frame)
		{
		case 0x119:
			if(group_near_camera(&e->groups[15], 40.0f))
			{
				hurt_player(e->target, 0x70, 7, 3.1415927f);
			}
			break;
		case 0x124:
			if(group_near_camera(&e->groups[15], 40.0f))
			{
				hurt_player(e->target, 0x70, 7, 0);
			}
			break;
		case 0x130:
			if(group_near_camera(&e->groups[15], 40.0f))
			{
				taur_hits++;
				hurt_player(e->target, 0x70, 0xa, 0);
			}
			break;
		}
		if(e->frame >= 0x138)
		{
			func_80119b18(e);
		}
		break;
	case 0x2001:
		if(++e->frame >= 0x61)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		break;
	case 0:
		if(++e->frame >= 0x1e3)
		{
			e->frame = 0;
		}
		break;
	case 6:
		if(++e->frame >= 0x1cc)
		{
			func_80119a5c(e);
		}
		break;
	}
}

/* 0x80119894: the state set-up: the entrance (4) counts in the audits, roars and sets the spot
   its walk leads to */
void func_80119894(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	switch(state)
	{
	case 4:
		increment_audit(0x3c);
		e->frame = 0x151;
		play_enemy_sound(e, 0x26, "taur", 8);
		taur_spot[0] = 846.799988f;
		taur_spot[1] = 30.6000004f;
		taur_spot[2] = 240.139999f;
		break;
	case 1:
		e->frame = 0;
		break;
	case 0:
	default:
		e->frame = 0;
		break;
	}
}

/* 0x80119944: at the end of a walk or a taunt: attack, or after three hits on the player in a
   row a taunt (the five in turn) */
void func_80119944(enemy_t *e)
{
	if(e->frame > 0)
	{
		e->frame--;
	}
	if(taur_hits < 3)
	{
		func_80119a5c(e);
		return;
	}
	switch(taur_taunt)
	{
	case 0:
		play_enemy_sound(e, 0x13, "taur", 0);
		break;
	case 1:
		play_enemy_sound(e, 0x2a, "taur", 0);
		break;
	case 2:
		play_enemy_sound(e, 0x2d, "taur", 0);
		break;
	case 3:
		play_enemy_sound(e, 0x2c, "taur", 0);
		break;
	case 4:
		play_enemy_sound(e, 0x14, "taur", 0);
		break;
	}
	if(++taur_taunt >= 5)
	{
		taur_taunt = 0;
	}
	blend_to_frame(e, taur_submesh_info, 0x16, 0x1b9, 5);
	e->ai_state = 6;
}

/* 0x80119a5c: pick one of the five attacks (a fifth each) and the distance it is made from */
void func_80119a5c(enemy_t *e)
{
	int	r;

	r = randrng(100);
	if(r < 20)
	{
		e->unka8 = 0x201;
		e->unk9c = -32.0f;
	}
	else if(r < 40)
	{
		e->unka8 = 0x202;
		e->unk9c = -26.0f;
	}
	else if(r < 60)
	{
		e->unka8 = 0x203;
		e->unk9c = -32.0f;
	}
	else if(r < 80)
	{
		e->unka8 = 0x204;
		e->unk9c = -16.0f;
	}
	else
	{
		e->unka8 = 0x205;
		e->unk9c = -16.0f;
	}
	func_80119b88(e);
}

/* 0x80119b18: an attack over: back to its walk (2), facing the player */
void func_80119b18(enemy_t *e)
{
	if(e->frame > 0)
	{
		e->frame--;
	}
	blend_to_frame(e, taur_submesh_info, 0x16, 0x138, 5);
	e->ai_state = 2;
	face_camera(e, 0.2f);
}

/* 0x80119b88: walk to the chosen attack's distance from the camera (1), then make it */
void func_80119b88(enemy_t *e)
{
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0.0f, e->unk9c);
	switch(step_toward_goal_xz(e, 0.7f))
	{
	case 2:
		e->target = get_target_player();
		if(e->frame > 0)
		{
			e->frame--;
		}
		switch(e->unka8)
		{
		case 0x201:
			blend_to_frame(e, taur_submesh_info, 0x16, 0x78, 5);
			e->ai_state = 0x201;
			break;
		case 0x202:
			blend_to_frame(e, taur_submesh_info, 0x16, 0x174, 5);
			e->ai_state = 0x202;
			break;
		case 0x203:
			blend_to_frame(e, taur_submesh_info, 0x16, 0xab, 5);
			e->ai_state = 0x203;
			break;
		case 0x204:
			blend_to_frame(e, taur_submesh_info, 0x16, 0xea, 5);
			e->ai_state = 0x204;
			break;
		case 0x205:
		default:
			blend_to_frame(e, taur_submesh_info, 0x16, 0x112, 5);
			e->ai_state = 0x205;
			break;
		}
		break;
	case 0:
	case 1:
		if(e->ai_state != 1)
		{
			if(e->frame > 0)
			{
				e->frame--;
			}
			blend_to_frame(e, taur_submesh_info, 0x16, 0, 5);
			e->ai_state = 1;
		}
		break;
	}
}

/* 0x80119d58: the glow of the seven small parts, a step a frame: dark, then now and again (5%
   after 21 frames) lighting up (texture variants 1, 2, then 4), lit for 6 frames, fading (2,
   1) and dark again; the variant is the group's texture key high bits */
void func_80119d58(enemy_t *e)
{
	taur_glow_t	*t;
	int	i;
	int	n;
	unsigned int	flags;

	for(i = 0; i < 7; i++)
	{
		t = &taur_glow[i];
		n = ++t->count;
		flags = (unsigned short)e->groups[t->group].flags;
		switch(t->state)
		{
		case 0:
			if(n >= 0x15 && randrng(100) < 5)
			{
				t->state = 4;
				t->count = 0;
			}
			break;
		case 4:
			if(n <= 0)
			{
				flags |= 0x1000000;
			}
			else if(n < 2)
			{
				flags |= 0x2000000;
			}
			else
			{
				flags |= 0x4000000;
				t->count = 0;
				t->state = 1;
			}
			break;
		case 1:
			flags |= 0x4000000;
			if(n >= 6)
			{
				t->state = 3;
				t->count = 0;
			}
			break;
		case 3:
			if(n < 2)
			{
				flags |= 0x2000000;
			}
			else if(n < 4)
			{
				flags |= 0x1000000;
			}
			else
			{
				t->count = 0;
				t->state = 0;
			}
			break;
		}
		e->groups[t->group].flags = flags;
	}
}

/* 0x80119f10: the name beside its life bar (texture 0x7083), 128 by 32 around x, y */
void func_80119f10(float x, float y)
{
	struct { float x, y, oow, sow, tow, pad; } v[4];

	guTexSource(find_surface(0x7083)->handle);
	guColorCombineFunction(4);
	guAlphaSource(2);
	grAlphaTestReferenceValue(4);
	v[0].x = x - 64.0f;
	v[0].y = y - 16.0f;
	v[0].oow = 0.909090877f;
	v[0].sow = 0;
	v[0].tow = 58.1818161f;
	v[1].x = x + 64.0f;
	v[1].y = y - 16.0f;
	v[1].oow = 0.909090877f;
	v[1].sow = 231.818176f;
	v[1].tow = 58.1818161f;
	v[2].x = x - 64.0f;
	v[2].y = y + 16.0f;
	v[2].oow = 0.909090877f;
	v[2].sow = 0;
	v[2].tow = 0;
	v[3].x = x + 64.0f;
	v[3].y = y + 16.0f;
	v[3].oow = 0.909090877f;
	v[3].sow = 231.818176f;
	v[3].tow = 0;
	func_800d326c(&v[0], &v[3], &v[1]);
	func_800d326c(&v[0], &v[2], &v[3]);
}

/* 0x8011a020: its footsteps, on the frames its feet land */
void func_8011a020(enemy_t *e)
{
	switch(e->frame)
	{
	case 9:
	case 0x14:
	case 0x1f:
	case 0x2b:
	case 0x13d:
	case 0x148:
	case 0x17b:
	case 0x180:
	case 0x184:
	case 0x188:
	case 0x18d:
	case 0x192:
	case 0x19b:
	case 0x1a0:
	case 0x1a7:
	case 0x1ad:
	case 0x1b1:
	case 0x1b8:
		play_enemy_sound_noint(e, 0x30, "freaksho", -1);
		break;
	}
}
