/*
 * krampus.c - Krampus, Rickety Town's boss (object type 0x2046, assets KRAM.ZM / kram.za /
 * kram.pth and the kram*.WMS texmaps). One original file, 0x8012d4a0-0x8012fa20, tables at
 * 0x8019d1a0 (texmaps), 0x8019da40 (submeshes) and 0x8019ddc0-0x8019de4c. The model has 1462
 * vertices, 1536 normals and 2426 faces in 28 submeshes, the animation 639 frames, the path
 * 604 points. There is one Krampus: the_krampus is his record and the level's movie script
 * walks him through five sections of his path (the script callbacks at the end of this file).
 *
 * Groups 20-27 are the pieces his limbs break into: each shares a limb's animation (set up by
 * load_krampus) and is hidden until twenty-four hits on that limb shoot it off
 * (replace_group); twelve hits wound it (texture bit 0x10000000) and make him flinch.
 *
 * His states: 0 idle, 1 walk, 2 stand, 3 roar, 4 and 6 the path's two halves, 7 and 8 two
 * long taunts, 0x201/0x203/0x204 the swipes, 0x202 throwing coal, 0x802/0x803 the script's
 * path sections (0x803 breathing smoke), 0x1001-0x1005 the hit reactions, 0x2001 dying,
 * 0x4001/0x4002 dead (the level ends).
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
#include "lights.h"

extern void	draw_effects(enemy_t *);			/* run the attached effects */
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, int, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);		/* objects.c */
extern void	set_enemy_health2(enemy_t *, float);		/* health by difficulty */
extern player_stats_t *get_target_player(void);		/* objects.c */
extern int	randrng(int);
extern void	attach_effect(enemy_t *, void *);
extern void	detach_effect(enemy_t *, group_t *);
extern void	*attach_held_object(group_t *, int, float, float, float, float, float, float, float *);	/* weapons.c */
extern void	throw_volley(float *, int, float *);		/* weapons.c */
extern void	path_place(enemy_t *, void *, short, float);	/* start along a path */
extern void	path_follow(enemy_t *, void *, short, float);	/* step along a path */
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern int	face_camera(enemy_t *, float);			/* enemy.c */
extern int	group_near_camera(group_t *, float);		/* a group within reach of the player */
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);	/* blend to a frame */
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);	/* hit reaction */
extern void	pick_random_action(enemy_t *, void *, submesh_t *, int, int);	/* pick a weighted random state (objects.c's file) */
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern float	distance_between(float *, float *);		/* enemy.c */
extern int	shot_hits_sphere(float *, float, player_stats_t *);	/* the shot hits the sphere */
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);		/* effects.c (the third argument is unused) */
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);		/* sprites.c */
extern void	blow_snow(float *, float *);			/* effects.c */
extern float	shot_damage_shared(player_stats_t *);		/* player.c */
extern void	award_points(int, int);
extern int	increment_audit(int);
extern int	mark_die();					/* spawn.c; the original passes the player too */
extern void	replace_group(enemy_t *, group_t *, group_t *);	/* a limb is shot off: its piece flies */
extern void	enable_camera_follow(void), disable_accuracy_tracking(void), disable_guns(void);
extern void	fade_out_level_stream(void), fade_out_level_music(void), movie_script_abort(void);
extern void	fade(float, int, int);
extern void	level_end_stats_wrapper(game_object_t *);	/* the end-of-level statistics object */
extern void	stream_jump(int, int, int);
extern void	draw_life_bar(float, float, float, float, float, unsigned int);	/* draw2d.c */
extern void	func_800d3528(MidVertexWithTA *, MidVertexWithTA *, MidVertexWithTA *);	/* a textured alpha triangle */
extern void	draw_puff(game_object_t *), puff_think(game_object_t *);	/* particle.c: the breath puff (object 0x1000) */
extern int	init_puff(game_object_t *);
extern void	puff_setup(short, int, float *, float, float, float, float, float, float, float, unsigned int);
extern void	puff_set_size(short, float);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern enemy_t	*debug_enemy;
extern char	text_buffer[];
extern int	violence_level;

extern texmap_t	krampus_texmap[0x45];			/* krampus_data.c */
extern submesh_t submesh_info_krampus[0x1c];

float	*krampus_verts;				/* 0x801e5b08 */
face_t	*krampus_faces;				/* 0x80212e84 */
float	*krampus_normals;			/* 0x801e57b4 */
void	*krampus_path;				/* 0x802372ac */
enemy_t	*the_krampus;				/* 0x801e0eac */
float	krampus_max_health;			/* 0x801e0f44 */
float	D_801de8c4;				/* set to 512 when Krampus appears; nothing found reads it */
float	krampus_hand1_last[3];			/* 0x801e5ce8: where group 16 was last frame (the snow it blows) */
float	krampus_hand2_last[3];			/* 0x801fac70: where group 17 was */
unsigned short	krampus_cue_prio_base;			/* 0x801facb8 */
unsigned short	krampus_cue_prio;			/* 0x8023726c: the priority of his "rickity" cues */

/* the hits on each limb (12 wounds it, 24 shoots it off) */
int	krampus_hits_head;			/* 0x801e0eb8: groups 0, 1, 12 */
int	krampus_hits_13;			/* 0x801e0ed0: groups 13-15 */
int	krampus_hits_right_arm;			/* 0x801e0ed8: groups 2, 3, 16 */
int	krampus_hits_left_arm;			/* 0x801e0f04: groups 4, 5, 17 */
int	krampus_hits_body;			/* 0x801e5cdc: groups 6, 7 */
int	krampus_hits_legs;			/* 0x801e0e78: groups 8-11 */

/* the movie script's section timers */
int	krampus_timer1;				/* 0x801e0ef8 */
int	krampus_timer2;				/* 0x801e0f00 */
int	krampus_timer3;				/* 0x801e0f14 */
int	krampus_timer4;				/* 0x801e0f1c */
int	krampus_timer5;				/* 0x801e0f24 */

/* the attacks near the camera: a percentile threshold, the state and its first frame */
typedef struct attack_choice
{
	int	chance;
	short	state, frame;
} attack_choice_t;

float	krampus_light_colour[3] = { 255.0f, 0.0f, 0.0f };	/* 0x8019ddc0 */
int	krampus_hurt_sounds[7] = { 18, 19, 20, 21, 22, 23, 24 };	/* 0x8019ddcc */
int	krampus_taunts[8] = { 9, 11, 12, 13, 14, 15, 16, 17 };	/* 0x8019dde8 */
int	krampus_growls[7] = { 2, 3, 4, 5, 6, 7, 10 };		/* 0x8019de08 */
attack_choice_t	krampus_attacks[5] =			/* 0x8019de24 */
{
	{ 10, 7, 0x232 }, { 20, 8, 0x253 }, { 50, 0x201, 0xb8 }, { 80, 0x203, 0x1ea }, { 101, 0x204, 0 }
};

/* the states */
#define KRAMPUS_IDLE		0
#define KRAMPUS_WALK		1
#define KRAMPUS_STAND		2
#define KRAMPUS_ROAR		3
#define KRAMPUS_PATH_START	4
#define KRAMPUS_PATH		6
#define KRAMPUS_TAUNT1		7
#define KRAMPUS_TAUNT2		8
#define KRAMPUS_SWIPE		0x201
#define KRAMPUS_THROW_COAL	0x202
#define KRAMPUS_SWIPE_BOTH	0x203
#define KRAMPUS_KICK		0x204
#define KRAMPUS_SECTION		0x802
#define KRAMPUS_SECTION_SMOKE	0x803
#define KRAMPUS_HIT_LEFT	0x1001
#define KRAMPUS_HIT_RIGHT	0x1002
#define KRAMPUS_HIT_LEGS	0x1003
#define KRAMPUS_HIT_HEAD	0x1004
#define KRAMPUS_HIT_13		0x1005
#define KRAMPUS_DYING		0x2001
#define KRAMPUS_DEAD		0x4001
#define KRAMPUS_DEAD_FALLING	0x4002
#define KRAMPUS_STAND2		0x8005

#define LIMB_WOUNDED		0x10000000	/* the group's texture key high bit: the wounded texture */

int	krampus_die(game_object_t *, player_stats_t *);
void	krampus_remove(game_object_t *);
void	krampus_setup(enemy_t *, unsigned short);
void	krampus_next_attack(enemy_t *);
void	draw_krampus_nameplate(float, float, float);

/* 0x8012d4a0: the draw callback: a red light at his left hand while he holds the coal, the
   model, his health bar and name, and the snow his hands blow */
void krampus_draw(game_object_t *obj)
{
	enemy_t	*e;
	int	lit, light;

	e = (enemy_t *)obj->data;
	lit = 0;
	light = 0;
	if((unsigned short)(e->frame - 0x9d) < 0x15)
	{
		light = add_dynamic_light(0x11, e->groups[17].pivot, krampus_light_colour, 6.0f, 15);
		lit = 1;
	}
	draw_model(e, submesh_info_krampus, 0x1c, krampus_verts, 0x5b6, krampus_faces, 0x97a, krampus_normals, 0x600);
	draw_effects(e);
	if(lit == 1)
	{
		remove_light(light);
	}
	if(!(e->ai_state & 0x6000))
	{
		draw_life_bar(e->health, krampus_max_health, 100.0f, 300.0f, 248.0f, 0xe4bd23);
		draw_krampus_nameplate(200.0f, 233.0f, 255.0f);
	}
	blow_snow(krampus_hand1_last, e->groups[16].pivot);
	krampus_hand1_last[0] = e->groups[16].pivot[0];
	krampus_hand1_last[1] = e->groups[16].pivot[1];
	krampus_hand1_last[2] = e->groups[16].pivot[2];
	blow_snow(krampus_hand2_last, e->groups[17].pivot);
	krampus_hand2_last[0] = e->groups[17].pivot[0];
	krampus_hand2_last[1] = e->groups[17].pivot[1];
	krampus_hand2_last[2] = e->groups[17].pivot[2];
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xffffff);
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
			draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xff0000);
		}
		else
		{
			draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xffffff);
		}
	}
	if(debug_mode & 0x1000)
	{
		func_800c99e4(e, 0xff);
	}
}

/* 0x8012d7d8: the hit test: the bounding sphere, then the submesh */
int krampus_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_krampus, 0x1c, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
			p->hit_group = group;
			p->die = (void *)krampus_die;
		}
	}
	return(0);
}

/* a limb's hit count: at 12 the limb is wounded, at 24 shot off */
#define LIMB_HIT(count, wound, off) \
	count++; \
	if(count == 12) \
	{ \
		wound; \
	} \
	if(count == 24) \
	{ \
		off; \
	} \
	break;

/* the flinch for a limb's 12th and 24th hit */
#define LIMB_REACT(count, frame, state) \
	if(count == 12) \
	{ \
		play_enemy_sound(e, krampus_hurt_sounds[randrng(7)], "krampus", 2); \
		start_hit_reaction(e, frame, state, submesh_info_krampus, 7); \
	} \
	if(count == 24) \
	{ \
		play_enemy_sound(e, krampus_hurt_sounds[randrng(7)], "krampus", 2); \
		start_hit_reaction(e, frame, state, submesh_info_krampus, 7); \
	} \
	break;

/* 0x8012d894: the shot lands: blood, the limb's hit count (wounds and lost limbs only at the
   highest violence setting), damage and points (2000 for the kill), then a flinch on a
   limb's 12th or 24th hit. A shot at his body while he lies dead knocks him over again */
int krampus_die(game_object_t *obj, player_stats_t *p)
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
	if(e->ai_state & 0x4000)
	{
		e->ai_state = KRAMPUS_DEAD_FALLING;
		e->frame = 0x270;
		e->flags &= 0xfffd;
		return(0);
	}
	group = p->hit_group;
	g = &e->groups[group];
	if(e->ai_state - KRAMPUS_SECTION < 2u)
	{
		return(0);
	}
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 0:
		case 1:
		case 12:
			LIMB_HIT(krampus_hits_head, e->groups[0].flags |= LIMB_WOUNDED,
				replace_group(e, &e->groups[0], &e->groups[20]))
		case 13:
		case 14:
		case 15:
			LIMB_HIT(krampus_hits_13, e->groups[13].flags |= LIMB_WOUNDED,
				replace_group(e, &e->groups[13], &e->groups[21]))
		case 2:
		case 3:
		case 16:
			LIMB_HIT(krampus_hits_right_arm, e->groups[2].flags |= LIMB_WOUNDED,
				replace_group(e, &e->groups[2], &e->groups[25]);
				replace_group(e, &e->groups[3], &e->groups[22]);
				e->groups[16].flags |= LIMB_WOUNDED)
		case 4:
		case 5:
		case 17:
			LIMB_HIT(krampus_hits_left_arm, e->groups[4].flags |= LIMB_WOUNDED,
				replace_group(e, &e->groups[4], &e->groups[24]);
				replace_group(e, &e->groups[5], &e->groups[23]);
				e->groups[17].flags |= LIMB_WOUNDED)
		case 6:
		case 7:
			LIMB_HIT(krampus_hits_body,
				e->groups[6].flags |= LIMB_WOUNDED;
				e->groups[2].flags |= LIMB_WOUNDED;
				e->groups[4].flags |= LIMB_WOUNDED,
				replace_group(e, &e->groups[6], &e->groups[26]);
				e->groups[7].flags |= LIMB_WOUNDED)
		case 8:
		case 9:
		case 10:
		case 11:
			LIMB_HIT(krampus_hits_legs,
				e->groups[8].flags |= LIMB_WOUNDED;
				e->groups[9].flags |= LIMB_WOUNDED;
				e->groups[10].flags |= LIMB_WOUNDED;
				e->groups[11].flags |= LIMB_WOUNDED,
				replace_group(e, &e->groups[8], &e->groups[27]);
				e->groups[9].flags = 0;
				e->groups[10].flags = 0;
				e->groups[11].flags = 0)
		case 18:
		case 19:
		case 20:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26:
		case 27:
			break;
		default:
			g->flags |= LIMB_WOUNDED;
			break;
		}
	}
	else
	{
		switch(group)
		{
		case 0:
		case 1:
		case 12:
			krampus_hits_head++;
			break;
		case 13:
		case 14:
		case 15:
			krampus_hits_13++;
			break;
		case 2:
		case 3:
		case 16:
			krampus_hits_right_arm++;
			break;
		case 4:
		case 5:
		case 17:
			krampus_hits_left_arm++;
			break;
		case 6:
		case 7:
			krampus_hits_body++;
			break;
		case 8:
		case 9:
		case 10:
		case 11:
			krampus_hits_legs++;
			break;
		case 18:
		case 19:
		case 20:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26:
		case 27:
			break;
		}
	}
	if(!(e->ai_state & 0x6000))
	{
		e->health -= shot_damage_shared(p);
		if(e->health <= 0.0f)
		{
			increment_audit(0x46);
			award_points(p->index, 2000);
			play_enemy_sound(e, 0x1c, "krampus", 8);
			e->killer = p;
			mark_die(obj->id, p);
			e->ai_state = KRAMPUS_DYING;
			e->frame = 0x192;
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
	switch(group)
	{
	case 0:
	case 1:
	case 12:
		LIMB_REACT(krampus_hits_head, 0x159, KRAMPUS_HIT_HEAD)
	case 13:
	case 14:
	case 15:
		LIMB_REACT(krampus_hits_13, 0x12c, KRAMPUS_HIT_13)
	case 2:
	case 3:
	case 16:
		LIMB_REACT(krampus_hits_right_arm, 0x3a, KRAMPUS_HIT_LEFT)
	case 4:
	case 5:
	case 17:
		LIMB_REACT(krampus_hits_left_arm, 0x3a, KRAMPUS_HIT_LEFT)
	case 6:
	case 7:
		LIMB_REACT(krampus_hits_body, 0x3a, KRAMPUS_HIT_LEFT)
	case 8:
	case 9:
	case 10:
	case 11:
		/* the second test reads the body's count, not the legs' (the original's slip) */
		if(krampus_hits_legs == 12)
		{
			play_enemy_sound(e, krampus_hurt_sounds[randrng(7)], "krampus", 2);
			start_hit_reaction(e, 0x17e, KRAMPUS_HIT_LEGS, submesh_info_krampus, 7);
		}
		if(krampus_hits_body == 24)
		{
			play_enemy_sound(e, krampus_hurt_sounds[randrng(7)], "krampus", 2);
			start_hit_reaction(e, 0x17e, KRAMPUS_HIT_LEGS, submesh_info_krampus, 7);
		}
		break;
	case 18:
	case 19:
	case 20:
	case 21:
	case 22:
	case 23:
	case 24:
	case 25:
	case 26:
	case 27:
		break;
	}
	return(0);
}

/* 0x8012e270, 0x8012e2a0: the texmaps, the model, the animation and the path. The eight
   pieces' own animations are dropped: each piece moves with the limb it breaks from */
void load_krampus_texmap(void)
{
	load_texmap(krampus_texmap, 0x45, "kram");
}

void load_krampus(void)
{
	load_model((void **)&krampus_verts, 0x5b6, (void **)&krampus_faces, 0x97a, (void **)&krampus_normals, 0x600, "KRAM.ZM", 0x3724dd42);
	load_anim(submesh_info_krampus, 0x1c, 0x27f, "kram.za", 0x8729ed97);
	load_path(&krampus_path, 0x25c, "kram.pth", 0x2201e6ab);
	free(submesh_info_krampus[20].anim);
	free(submesh_info_krampus[21].anim);
	free(submesh_info_krampus[22].anim);
	free(submesh_info_krampus[23].anim);
	free(submesh_info_krampus[24].anim);
	free(submesh_info_krampus[25].anim);
	free(submesh_info_krampus[27].anim);
	free(submesh_info_krampus[26].anim);
	submesh_info_krampus[20].anim = submesh_info_krampus[0].anim;
	submesh_info_krampus[21].anim = submesh_info_krampus[13].anim;
	submesh_info_krampus[22].anim = submesh_info_krampus[3].anim;
	submesh_info_krampus[23].anim = submesh_info_krampus[5].anim;
	submesh_info_krampus[24].anim = submesh_info_krampus[4].anim;
	submesh_info_krampus[25].anim = submesh_info_krampus[2].anim;
	submesh_info_krampus[27].anim = submesh_info_krampus[8].anim;
	submesh_info_krampus[26].anim = submesh_info_krampus[6].anim;
}

/* 0x8012e448: the shared animations are not the pieces' own, so they are forgotten first */
void remove_krampus_assets(void)
{
	submesh_info_krampus[20].anim = 0;
	submesh_info_krampus[21].anim = 0;
	submesh_info_krampus[22].anim = 0;
	submesh_info_krampus[23].anim = 0;
	submesh_info_krampus[24].anim = 0;
	submesh_info_krampus[25].anim = 0;
	submesh_info_krampus[27].anim = 0;
	submesh_info_krampus[26].anim = 0;
	remove_surface(krampus_texmap, 0x45);
	remove_path(&krampus_path, 0x25c, "kram.pth");
	remove_animation(submesh_info_krampus, 0x1c, 0x27f, "kram.za");
	remove_model((void **)&krampus_verts, 0x5b6, (void **)&krampus_faces, 0x97a, (void **)&krampus_normals, 0x600, "KRAM.ZM");
}

/* 0x8012e528 (Binary Ninja: spawn_krampus): the init callback: the record (32 to the side,
   facing the camera, 400 health by difficulty), the 28 groups with the pieces hidden, the hit
   counts cleared, his entrance roar and the boss music */
int krampus_init(game_object_t *obj)
{
	enemy_t	*e;
	int	i;

	obj->data = malloc(sizeof(enemy_t));
	if(!obj->data)
	{
		return(1);
	}
	the_krampus = e = (enemy_t *)obj->data;
	obj->hit = krampus_hit;
	obj->die = krampus_die;
	obj->remove = krampus_remove;
	e->unk4 = 0x14;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0xc;
	D_801de8c4 = 512.0f;
	e->target = get_target_player();
	e->unk54 = 0;
	krampus_timer1 = 0x14;
	e->rx = 0;
	e->rz = 0;
	e->y = 0;
	e->z = 0;
	e->ry = 3.1415927f;
	e->x = 32.0f;
	e->unkcc = 1.5f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health2(e, 400.0f);
	krampus_max_health = e->health;
	e->groups = malloc(0x1c * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < 0x1c; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[20].flags = 0;
	e->groups[21].flags = 0;
	e->groups[22].flags = 0;
	e->groups[23].flags = 0;
	e->groups[24].flags = 0;
	e->groups[25].flags = 0;
	e->groups[27].flags = 0;
	e->groups[26].flags = 0;
	init_groups(e, submesh_info_krampus, 0x1c);
	krampus_hand1_last[0] = e->groups[16].pos[0];
	krampus_hand1_last[1] = e->groups[16].pos[1];
	krampus_hand1_last[2] = e->groups[16].pos[2];
	krampus_hand2_last[0] = e->groups[17].pos[0];
	krampus_hand2_last[1] = e->groups[17].pos[1];
	krampus_hand2_last[2] = e->groups[17].pos[2];
	e->hit_group = &e->groups[7];
	e->hit_group->floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	krampus_cue_prio_base = 0x40;
	krampus_cue_prio = next_cue_priority();
	krampus_setup(e, 0);
	increment_audit(0x45);
	krampus_hits_left_arm = 0;
	krampus_hits_right_arm = 0;
	krampus_hits_13 = 0;
	krampus_hits_head = 0;
	krampus_hits_body = 0;
	krampus_hits_legs = 0;
	play_enemy_sound(e, 1, "krampus", 7);
	stream_jump(8, 9, 9);
	return(0);
}

/* 0x8012e7f8: the remove callback */
void krampus_remove(game_object_t *obj)
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

/* a swipe's reach test: a hand within 45 of the camera */
#define HAND_NEAR(n)	group_near_camera(&e->groups[n], 45.0f)

/* 0x8012e850: the think callback */
void krampus_think(game_object_t *obj)
{
	enemy_t	*e;
	short	frame;
	int	id;

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
	animate_loose_groups(e, 0x1c);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case KRAMPUS_HIT_LEGS:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x192)
		{
			break;
		}
		goto reacted;
	case KRAMPUS_HIT_RIGHT:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x70)
		{
			break;
		}
		goto reacted;
	case KRAMPUS_HIT_LEFT:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x70)
		{
			break;
		}
		goto reacted;
	case KRAMPUS_HIT_HEAD:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x17e)
		{
			break;
		}
		goto reacted;
	case KRAMPUS_HIT_13:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x159)
		{
			break;
		}
	reacted:
		e->frame = frame;
		blend_to_frame(e, submesh_info_krampus, 7, e->resume_frame, 3);
		e->ai_state = e->resume_state;
		break;
	case KRAMPUS_WALK:
		e->frame++;
		if(e->frame >= 0x3a)
		{
			e->frame = 0x1a;
		}
		face_camera(e, 0.2f);
		krampus_next_attack(e);
		break;
	case KRAMPUS_SWIPE:
		face_camera(e, 0.2f);
		e->frame++;
		switch(e->frame)
		{
		case 0xc0:
			if(HAND_NEAR(17))
			{
				hurt_player(e->target, 0xd2, 7, 0.0f);
			}
			break;
		case 0xd3:
			if(HAND_NEAR(16))
			{
				hurt_player(e->target, 0xd2, 7, 0.0f);
			}
			break;
		}
		if(e->frame < 0xda)
		{
			break;
		}
		krampus_next_attack(e);
		break;
	case KRAMPUS_SWIPE_BOTH:
		face_camera(e, 0.2f);
		e->frame++;
		switch(e->frame)
		{
		case 0x1fa:
			if(HAND_NEAR(17))
			{
				hurt_player(e->target, 0x69, 7, 3.1415927f);
			}
			break;
		case 0x205:
			if(HAND_NEAR(16))
			{
				hurt_player(e->target, 0x69, 7, 0.0f);
			}
			break;
		case 0x212:
			if(HAND_NEAR(17))
			{
				hurt_player(e->target, 0x69, 7, 3.1415927f);
			}
			break;
		case 0x224:
			if(HAND_NEAR(17))
			{
				hurt_player(&player_stats[0], 0x69, 0xa, 0.0f);
			}
			if(HAND_NEAR(16))
			{
				hurt_player(&player_stats[1], 0x69, 0xa, 0.0f);
			}
			break;
		}
		if(e->frame < 0x232)
		{
			break;
		}
		krampus_next_attack(e);
		break;
	case KRAMPUS_KICK:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame == 0x16)
		{
			if(HAND_NEAR(17))
			{
				hurt_player(e->target, 0xd2, 3, 0.0f);
			}
		}
		if(e->frame < 0x1a)
		{
			break;
		}
		krampus_next_attack(e);
		break;
	case KRAMPUS_TAUNT1:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame < 0x253)
		{
			break;
		}
		krampus_next_attack(e);
		break;
	case KRAMPUS_TAUNT2:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame < 0x270)
		{
			break;
		}
		krampus_next_attack(e);
		break;
	case KRAMPUS_ROAR:
		e->frame++;
		if(e->frame < 0x3a)
		{
			break;
		}
		krampus_next_attack(e);
		break;
	case KRAMPUS_STAND:
	case KRAMPUS_STAND2:
		e->frame++;
		if(e->frame >= 0x3a)
		{
			e->frame = 0x1a;
		}
		break;
	case KRAMPUS_DYING:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x1ce)
		{
			break;
		}
		e->frame = frame;
		e->ai_state = KRAMPUS_DEAD;
		e->counter = 0;
		enable_camera_follow();
		break;
	case KRAMPUS_DEAD_FALLING:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame >= 0x27f)
		{
			e->frame = frame;
			e->ai_state = KRAMPUS_DEAD;
		}
		/* fall through: the level-end countdown runs on */
	case KRAMPUS_DEAD:
		e->counter++;
		if(e->counter == 0x28)
		{
			disable_accuracy_tracking();
			add_object1_wrapper(0x800, level_end_stats_wrapper, 0, 0);
		}
		if(e->counter == 0xb4)
		{
			fade_out_level_stream();
			disable_guns();
			fade_out_level_music();
			fade(0.0f, 0x50, 0);
		}
		if(e->counter == 0x104)
		{
			movie_script_abort();
		}
		break;
	case KRAMPUS_IDLE:
		e->frame++;
		if(e->frame >= 0x27f)
		{
			e->frame = 0;
		}
		break;
	case KRAMPUS_THROW_COAL:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame == 0x9d && (e->groups[17].flags & 0x2000))
		{
			attach_effect(e, attach_held_object(&e->groups[17], 0x25, 0.06f, 0.15f, 0.16f, 4.2f, -2.9f, 0, e->scale));
		}
		if(e->frame == 0xae && (e->groups[17].flags & 0x2000))
		{
			throw_volley(e->groups[17].pos, 0x25, e->scale);
			detach_effect(e, &e->groups[17]);
		}
		if(e->frame >= 0xb8)
		{
			e->frame = 0x87;
		}
		break;
	case KRAMPUS_PATH_START:
		e->frame = 0xda;
		path_place(e, krampus_path, e->path, 1.0f);
		e->path++;
		if(e->path < e->unk92 - 0x51)
		{
			break;
		}
		e->ai_state = KRAMPUS_PATH;
		break;
	case KRAMPUS_PATH:
		e->frame++;
		if(e->frame >= 0x12c)
		{
			krampus_next_attack(e);
		}
		e->path++;
		path_place(e, krampus_path, e->path, 1.0f);
		goto path_end;
	case KRAMPUS_SECTION:
		e->frame++;
		if(e->frame >= 0x3a)
		{
			e->frame = 0x1a;
		}
		if(e->frame == 0x1c)
		{
			play_cue_flags_noint(0x11, "rickity", krampus_cue_prio);
		}
		if(e->frame == 0x29)
		{
			play_cue_flags_noint(0x12, "rickity", krampus_cue_prio);
		}
		path_follow(e, krampus_path, e->path, 1.0f);
		if(e->path == e->unk92 - 10)
		{
			e->ai_state = KRAMPUS_SECTION_SMOKE;
			e->frame = 0x1ce;
			play_cue_flags_noint(0x13, "rickity", krampus_cue_prio);
		}
	path_end:
		if(e->path < e->unk92)
		{
			break;
		}
		krampus_next_attack(e);
		break;
	case KRAMPUS_SECTION_SMOKE:
		if((unsigned short)(e->frame++ - 0x1cc) < 0x13)
		{
			id = add_object1_wrapper(0x1000, draw_puff, init_puff, puff_think);
			if(id > 0)
			{
				puff_setup(id, 10, e->groups[12].pivot, e->y, 0.0f, 0.8f, 0.0f, 0.8f, 0.3f, 0.8f, 0xffffff);
			}
			puff_set_size(id, 1.0f);
			id = add_object1_wrapper(0x1000, draw_puff, init_puff, puff_think);
			if(id > 0)
			{
				puff_setup(id, 10, e->groups[15].pivot, e->y, 0.0f, 0.8f, 0.0f, 0.8f, 0.3f, 0.8f, 0xffffff);
			}
			puff_set_size(id, 1.0f);
		}
		if(e->frame >= 0x1ea)
		{
			krampus_next_attack(e);
		}
		path_follow(e, krampus_path, e->path, 1.0f);
		if(e->path >= e->unk92)
		{
			e->path--;
		}
		break;
	}
}

/* 0x8012f320: the state set-up: each state starts its animation at its own frame; the path
   start places him on his path */
void krampus_setup(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	switch(state)
	{
	case KRAMPUS_SWIPE:
		e->frame = 0xb8;
		break;
	case KRAMPUS_WALK:
		e->frame = 0x1a;
		break;
	case KRAMPUS_PATH_START:
		e->frame = 0xda;
		path_place(e, krampus_path, e->path, 1.0f);
		break;
	case KRAMPUS_THROW_COAL:
		e->frame = 0x70;
		break;
	case KRAMPUS_IDLE:
		e->frame = 0;
		break;
	default:
		e->frame = 0;
		break;
	}
}

/* 0x8012f3c8: what next: a playing target (a player who has just continued is passed over
   once), then from more than 150 away he throws coal with a taunt, nearer he picks an attack
   (krampus_attacks) and taunts or growls */
void krampus_next_attack(enemy_t *e)
{
	e->target = get_target_player();
	if(e->target->unkd4 > 0)
	{
		e->target = get_target_player();
	}
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0, -18.0f);
	if(distance_between(&e->x, e->goal) > 150.0f)
	{
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_krampus, 7, 0x70, 3);
		e->ai_state = KRAMPUS_THROW_COAL;
		play_enemy_sound(e, krampus_taunts[randrng(8)], "krampus", 0);
		return;
	}
	pick_random_action(e, krampus_attacks, submesh_info_krampus, 7, 3);
	if(e->ai_state & 0x200)
	{
		play_enemy_sound(e, krampus_taunts[randrng(8)], "krampus", 0);
	}
	else
	{
		play_enemy_sound(e, krampus_growls[randrng(7)], "krampus", 0);
	}
}

/* 0x8012f544: stand still (blend to frame 0x1a, state 2); nothing in the game calls it */
void krampus_stand(enemy_t *e)
{
	e->unk6 = 0;
	e->frame--;
	blend_to_frame(e, submesh_info_krampus, 7, 0x1a, 2);
	e->ai_state = KRAMPUS_STAND;
}

/* 0x8012f59c: his name plate over the health bar: texture 0x7085, 128 by 16 at (x, y) */
void draw_krampus_nameplate(float x, float y, float alpha)
{
	MidVertexWithTA	a, b, c, d;

	guTexSource(find_surface(0x7085)->handle);
	guColorCombineFunction(4);
	guAlphaSource(3);
	grAlphaTestReferenceValue(4);
	a.x = x - 64.0f;
	a.y = y - 8.0f;
	a.oow = 0.90909088f;
	a.sow = 0.90909088f;
	a.tow = 57.272724f;
	b.x = x + 64.0f;
	b.y = y - 8.0f;
	b.oow = 0.90909088f;
	b.sow = 231.81818f;
	b.tow = 57.272724f;
	c.x = x - 64.0f;
	c.y = y + 8.0f;
	c.oow = 0.90909088f;
	c.sow = 0.90909088f;
	c.tow = 0.90909088f;
	d.x = x + 64.0f;
	d.y = y + 8.0f;
	d.oow = 0.90909088f;
	d.sow = 231.81818f;
	d.tow = 0.90909088f;
	d.a = alpha;
	c.a = alpha;
	b.a = alpha;
	a.a = alpha;
	func_800d3528(&a, &d, &b);
	func_800d3528(&a, &c, &d);
}

/* 0x8012f6c8-0x8012f9d8: the movie script's callbacks. The script walks Krampus through five
   sections of his path; for each a timer is set, a test counts it down (true while it runs,
   and always once he is dead) and a start puts him on the section */
#define SECTION_TIMER(set, count, timer) \
void set(void) \
{ \
	if(!(the_krampus->ai_state & 0x6000)) \
	{ \
		timer = 0x14; \
	} \
} \
int count(void) \
{ \
	if(!(the_krampus->ai_state & 0x6000)) \
	{ \
		return(--timer > 0); \
	} \
	return(1); \
}

#define SECTION_START(start, first, last) \
void start(void) \
{ \
	enemy_t	*e; \
	\
	e = the_krampus; \
	if(!(e->ai_state & 0x6000)) \
	{ \
		e->ai_state = KRAMPUS_SECTION; \
		e->unk90 = first; \
		e->path = first; \
		e->unk92 = last; \
		e->frame = 0x1a; \
		e->flags &= 0xfffd; \
	} \
}

SECTION_TIMER(krampus_section1_wait, krampus_section1_waiting, krampus_timer1)	/* 0x8012f6c8, 0x8012f6f0 */
SECTION_START(krampus_section1, 0x45, 0xb2)					/* 0x8012f728 */
SECTION_TIMER(krampus_section2_wait, krampus_section2_waiting, krampus_timer2)	/* 0x8012f774, 0x8012f79c */
SECTION_START(krampus_section2, 0xb3, 0x120)					/* 0x8012f7d4 */
SECTION_TIMER(krampus_section3_wait, krampus_section3_waiting, krampus_timer3)	/* 0x8012f820, 0x8012f848 */
SECTION_START(krampus_section3, 0x121, 0x181)					/* 0x8012f880 */
SECTION_TIMER(krampus_section4_wait, krampus_section4_waiting, krampus_timer4)	/* 0x8012f8cc, 0x8012f8f4 */
SECTION_START(krampus_section4, 0x182, 0x206)					/* 0x8012f92c */
SECTION_TIMER(krampus_section0_wait, krampus_section0_waiting, krampus_timer5)	/* 0x8012f978, 0x8012f9a0 */
SECTION_START(krampus_section0, 0, 0x44)					/* 0x8012f9d8 */
