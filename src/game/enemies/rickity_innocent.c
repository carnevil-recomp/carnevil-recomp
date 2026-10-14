/*
 * rickity_innocent.c - the Rickety Town innocent: a little girl (object type 0x2043, assets
 * RINN.ZM / rinn.za and the INNG*.WMS texmaps, her voice in the "girl" bank). One original
 * file, 0x801293b8-0x80129d54, tables at 0x8019b4e8 (texmaps), 0x8019b7c8 (submeshes) and
 * 0x8019b9e8 (her cries). The model has 551 vertices, 613 normals and 953 faces in 17
 * submeshes, the animation 109 frames. She plays her animation where the spawn table puts
 * her; shooting her costs the player 350 health and a penalty notice, she cries out and
 * fades away. An enemy holding her (enemy_t.unkd8, the link the pairing code keeps) lets go.
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
#include "camera.h"

extern void	draw_effects(enemy_t *);			/* run the attached effects */
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, int, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);		/* debug sphere */
extern void	func_800d14a4(float, float);				/* debug map marker */
extern void	func_800c99e4(enemy_t *, int);				/* debug: the groups' frames */
extern void	screen_to_ray(float *, float, float);			/* vecmat.c */
extern float	get_screen_pos(float *, float *);
extern void	set_enemy_scale(enemy_t *, float), set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern void	clear_enemy_fields(enemy_t *);				/* objects.c */
extern void	unlink_enemy(enemy_t *);				/* unlink the enemy's pair links */
extern void	animate_loose_groups(enemy_t *, int);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	show_pickup_message(player_stats_t *, int);			/* player.c's file: a penalty notice */
extern void	func_800fa1c4(enemy_t *);				/* the holder lets go */
extern void	add_world_sprite(float *, int, int);
extern int	randrng(int);
extern int	remove_die();						/* spawn.c */
extern int	load_model(void **, int, void **, int, void **, int, char *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *);
extern float	D_801faca4, D_801facb4;					/* the debug aim point (objects.c) */
extern enemy_t	*debug_enemy;
extern char	text_buffer[];

extern texmap_t	rinn_texmap[23];				/* rickity_innocent_data.c */
extern submesh_t rinn_submesh[17];

float	*rinn_verts;			/* 0x801e5ba0 */
face_t	*rinn_faces;			/* 0x8021f0e8 */
float	*rinn_normals;			/* 0x801e5864 */
int	rinn_cries[3] = { 4, 5, 6 };	/* 0x8019b9e8: "girl" bank calls */

/* her states */
#define INNOCENT_IDLE		0		/* the whole animation, looping */
#define INNOCENT_LEAVE1		1		/* frames 0-52 once, then gone */
#define INNOCENT_LEAVE2		2		/* frames 53-108 once, then gone */
#define INNOCENT_LOOP		3		/* frames 0-52, looping */
#define INNOCENT_SHOT		0x8000		/* fading out */

#define ALPHA_STEP(e)	(*(float *)&(e)->unk44)	/* 0x44: the fade's step per frame */
#define HOLDER(e)	((enemy_t *)(e)->unkd8)	/* 0xd8: the enemy holding her */

int	rinn_hit(game_object_t *, player_stats_t *);
int	rinn_die(game_object_t *, player_stats_t *);
void	rinn_remove(game_object_t *);
void	rinn_setup(enemy_t *, unsigned short);

/* 0x801293b8: the draw callback; in debug mode 2 she also takes the debug aim's shots */
void rinn_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, rinn_submesh, 0x11, rinn_verts, 0x227, rinn_faces, 0x3b9, rinn_normals, 0x265);
	draw_effects(e);
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
	if(debug_mode & 2)
	{
		screen_to_ray(player_stats[0].aim_dir, D_801faca4, D_801facb4);
		player_stats[0].aim_dir[2] = 1.0f / player_stats[0].aim_dir[2];
		rinn_hit(obj, &player_stats[0]);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[10].pos, e->radius, 0x600000);
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

/* 0x801295d0: the hit test: the bounding sphere, then the submesh */
int rinn_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		if((group = find_submesh_hit(e, rinn_submesh, 0x11, p)) != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)rinn_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return 0;
}

/* 0x8012968c: an empty function the file keeps */
void func_8012968c(void)
{
}

/* 0x80129694: she was shot: the player loses 350 health and gets the penalty notice, a hit
   mark appears on her if she is on screen, she fades out, whoever held her lets go and she
   cries out. In the character debug mode the shot only selects her */
int rinn_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e, *holder;
	float	scr[4];

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return 0;
	}
	if(e->ai_state != INNOCENT_SHOT)
	{
		show_pickup_message(p, 5);
		hurt_player(p, 0x15e, 5, 0.0f);
		if(get_screen_pos(e->hit_group->pivot, scr) != -1.0f)
		{
			add_world_sprite(e->groups[2].pivot, 7, 0);
		}
		rinn_setup(e, INNOCENT_SHOT);
		holder = HOLDER(e);
		if(holder)
		{
			holder->unkdc = 0;
			e->unkd8 = 0;
			func_800fa1c4(holder);
		}
		play_enemy_sound(e, rinn_cries[randrng(3)], "girl", 8);
	}
	return 0;
}

/* 0x801297a0 */
void load_rinn_texmap(void)
{
	load_texmap(rinn_texmap, 0x17, "rinn");
}

/* 0x801297d0 */
void load_rinn(void)
{
	load_model((void **)&rinn_verts, 0x227, (void **)&rinn_faces, 0x3b9, (void **)&rinn_normals, 0x265, "RINN.ZM", (int)0xe0d58427);
	load_anim(rinn_submesh, 0x11, 0x6d, "rinn.za", (int)0xa2edc822);
}

/* 0x80129858 */
void remove_rinn_assets(void)
{
	remove_animation(rinn_submesh, 0x11, 0x6d, "rinn.za");
	remove_model((void **)&rinn_verts, 0x227, (void **)&rinn_faces, 0x3b9, (void **)&rinn_normals, 0x265, "RINN.ZM");
}

/* 0x801298c8: the init callback: the record (scale 1, hit radius 0.9 of it, 0.75 health),
   seventeen groups, the hit group the chest */
int rinn_init(game_object_t *obj)
{
	enemy_t	*e;
	group_t	*g;
	void	*data;
	int	i;

	data = malloc(sizeof(enemy_t));
	obj->data = data;
	if(!data)
	{
		return 1;
	}
	obj->hit = rinn_hit;
	obj->die = rinn_die;
	obj->remove = rinn_remove;
	e = (enemy_t *)data;
	e->unk4 = 0x14;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0xc;
	e->target = get_target_player();
	e->rx = 0.0f;
	e->ry = 0.0f;
	e->rz = 0.0f;
	e->y = 0.0f;
	e->z = 0.0f;
	e->x = -8.0f;
	e->unkcc = 0.9f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 0.75f);
	e->groups = malloc(0x11 * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return 1;
	}
	for(i = 0; i < 0x11; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	init_groups(e, rinn_submesh, 0x11);
	g = e->groups;
	e->hit_group = &g[10];
	g[10].floor = 0.0f;
	clear_enemy_fields(e);
	e->prio_base = 0x38;
	e->prio_flags = next_cue_priority();
	rinn_setup(e, INNOCENT_IDLE);
	return 0;
}

/* 0x80129a2c: the remove callback */
void rinn_remove(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(debug_enemy == e)
	{
		debug_enemy = 0;
	}
	unlink_enemy(e);
	free_effects(e);
	free(e->groups);
	free(e);
}

/* 0x80129a8c: the think callback: step the animation for her state; the one-shot states
   end with her removal, the fade with her gone once transparent */
void rinn_think(game_object_t *obj)
{
	enemy_t	*e;
	int	i;

	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, 0x11);
	if(e->flags & 2)
	{
		return;
	}
	if(debug_mode & 0x10000)
	{
		return;
	}
	switch(e->ai_state)
	{
	case INNOCENT_LEAVE1:
		e->frame++;
		if(e->frame < 0x35)
		{
			return;
		}
		break;
	case INNOCENT_LEAVE2:
		e->frame++;
		if(e->frame < 0x6d)
		{
			return;
		}
		break;
	case INNOCENT_LOOP:
		e->frame++;
		if(e->frame >= 0x35)
		{
			e->frame = 0;
		}
		return;
	case INNOCENT_IDLE:
		e->frame++;
		if(e->frame >= 0x6d)
		{
			e->frame = 0;
		}
		return;
	case INNOCENT_SHOT:
		e->alpha = e->alpha + ALPHA_STEP(e);
		if(e->alpha > 0.0f)
		{
			for(i = 0; i < 0x11; i++)
			{
				e->groups[i].flags |= 0x80;
				e->groups[i].alpha = e->alpha;
			}
			return;
		}
		break;
	default:
		return;
	}
	remove_die(obj->id, 0);
}

/* 0x80129c40: put her in a state: the leaving states call out, the fade starts opaque and
   loses 10 a frame with every group alpha blended */
void rinn_setup(enemy_t *e, unsigned short state)
{
	int	i;

	e->ai_state = state;
	switch(state)
	{
	case INNOCENT_LOOP:
		e->frame = 0;
		play_enemy_sound(e, 3, "girl", 0);
		break;
	case INNOCENT_LEAVE2:
		e->frame = 0x35;
		play_enemy_sound(e, 3, "girl", 0);
		break;
	case INNOCENT_LEAVE1:
		play_enemy_sound(e, 1, "girl", 0);
		e->frame = 0;
		break;
	case INNOCENT_SHOT:
		e->alpha = 255.0f;
		ALPHA_STEP(e) = -10.0f;
		e->flags &= ~2;
		for(i = 0; i < 0x11; i++)
		{
			e->groups[i].flags |= 0x80;
			e->groups[i].alpha = e->alpha;
		}
		break;
	default:
		e->frame = 0;
		break;
	}
}
