/*
 * vict.c - VICT (object type 0x2012, the game's name for it), a Haunted House enemy whose
 * texmaps are a mangled body: VICTSPIN, VICTUARM/VICTLARM, VICTBHND/VICTFHND, VICTNECK,
 * VICTFACE, VICTFTCH, VICTBON, VICTGUT, VICTBKCH. Model VICT.ZM / vict.za: 295 vertices,
 * 356 normals, 528 faces in 10 submeshes, 78 animation frames. One original file,
 * 0x80105e38-0x80106928, tables at 0x801924e0 (texmaps) and 0x801926c0 (submeshes).
 * It waits (state 0x101) or rests (1); a shot while waiting plays its flinch (0x400, frames
 * 63-77) and back to waiting; in 0x100 it comes at the player (frames 48-62 looping), and when
 * it reaches them they lose 100 health and it is gone. Killed (any other state): 500 points,
 * its parts fly off and it stays dead (0x200, no more hits). The exported names are still the
 * address names spawn.c and the asm use; each says what it is.
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
extern void	zombie_walk(enemy_t *);				/* the holder lets go */
extern void	add_world_sprite(float *, int, int);
extern int	randrng(int);
extern int	remove_die();						/* spawn.c */
extern int	load_model(void **, int, void **, int, void **, int, char *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *);
extern float	D_801faca4, D_801facb4;					/* the debug aim point (objects.c) */
extern enemy_t	*debug_enemy;
extern char	text_buffer[];

extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	award_points(int, int);
extern void	detach_group(group_t *, float *, float, float *);	/* model.c: a part flies off */
extern int	charge_camera(enemy_t *, float, float, float, float);	/* enemy.c's file: turn to the camera and step
									   towards a point near it, 2 on arrival */
extern int	violence_level;
extern void	remove_surface(texmap_t *, int);

extern texmap_t	vict_texmap[15];			/* vict_data.c */
extern submesh_t submesh_info_vict[10];

float	*vict_verts;			/* 0x801e5b9c */
face_t	*vict_faces;			/* 0x8021f0e0 */
float	*vict_normals;			/* 0x801e585c */

/* its states (the spawn table's) */
#define VICT_REST		1		/* frame 0, held */
#define VICT_CHARGE		0x100		/* frames 48-62 looping, coming at the player */
#define VICT_WAIT		0x101		/* frame 63, held */
#define VICT_DEAD		0x200		/* parts flown off; no more hits */
#define VICT_FLINCH		0x400		/* frames 63-77, then waiting again */

#define FLOOR(e)	((e)->unk98)		/* 0x98: where its loose parts land */
#define HOLDER(e)	((enemy_t *)(e)->unkd8)

int	vict_hit(game_object_t *, player_stats_t *);
int	vict_die(game_object_t *, player_stats_t *);
void	vict_remove(game_object_t *);
void	vict_setup(enemy_t *, unsigned short);

/* 0x80105e38: the draw callback; the debug label is drawn twice, white, then red
   for the debug enemy */
void vict_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_vict, 0xa, vict_verts, 0x127, vict_faces, 0x210, vict_normals, 0x164);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 2)
	{
		screen_to_ray(player_stats[0].aim_dir, D_801faca4, D_801facb4);
		player_stats[0].aim_dir[2] = 1.0f / player_stats[0].aim_dir[2];
		vict_hit(obj, &player_stats[0]);
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

/* 0x80106098: the hit test: the bounding sphere, then the submesh; none once dead */
int vict_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(e->ai_state != VICT_DEAD)
	{
		if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
		{
			if((group = find_submesh_hit(e, submesh_info_vict, 0xa, p)) != -1)
			{
				p->hit = 1;
				p->hit_id = obj->id;
				p->hit_group = group;
				p->die = (void *)vict_die;
				p->hit_point[0] = p->aim[0];
				p->hit_point[1] = p->aim[1];
				p->hit_point[2] = p->aim[2];
			}
		}
	}
	return 0;
}

/* 0x80106160: an empty function the file keeps */
void func_80106160(void)
{
}

/* 0x80106168: the shot lands: blood, chunks and a hit mark; whoever it held lets
   go. Waiting, it flinches; otherwise it dies: 500 points and every part flies off (at the
   highest violence from three points, the body and each arm's upper part, else all from the
   first). In the character debug mode the shot only selects it */
int vict_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e, *holder;

	e = (enemy_t *)obj->data;
	holder = HOLDER(e);
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return 0;
	}
	if(holder)
	{
		e->unkd8 = 0;
		holder->unkdc = 0;
	}
	burst_blood(p->hit_point, FLOOR(e), 0x800000);
	add_chunks(e->hit_group->pos, FLOOR(e));
	add_world_sprite(p->hit_point, 0, 0);
	if(e->ai_state == VICT_WAIT || e->ai_state == VICT_FLINCH)
	{
		e->ai_state = VICT_FLINCH;
		e->frame = 0x3f;
		e->flags &= ~2;
	}
	else
	{
		award_points(p->index, 500);
		e->ai_state = VICT_DEAD;
		if(violence_level >= 3)
		{
			detach_group(&e->groups[0], e->groups[0].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[4], e->groups[0].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[8], e->groups[0].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[9], e->groups[0].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[1], e->groups[2].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[2], e->groups[2].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[3], e->groups[2].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[5], e->groups[6].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[6], e->groups[6].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[7], e->groups[6].pivot, FLOOR(e), e->scale);
		}
		else
		{
			detach_group(&e->groups[0], e->groups[0].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[4], e->groups[0].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[8], e->groups[0].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[9], e->groups[0].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[1], e->groups[0].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[2], e->groups[0].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[3], e->groups[0].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[5], e->groups[0].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[6], e->groups[0].pivot, FLOOR(e), e->scale);
			detach_group(&e->groups[7], e->groups[0].pivot, FLOOR(e), e->scale);
		}
	}
	return 0;
}

/* 0x8010644c */
void load_vict_texmap(void)
{
	load_texmap(vict_texmap, 0xf, "vict");
}

/* 0x8010647c */
void load_vict(void)
{
	load_model((void **)&vict_verts, 0x127, (void **)&vict_faces, 0x210, (void **)&vict_normals, 0x164, "VICT.ZM", (int)0x0ef590a2);
	load_anim(submesh_info_vict, 0xa, 0x4e, "vict.za", (int)0x64f709c8);
}

/* 0x80106504: remove_vict_assets: the texmaps, the animation and the model */
void remove_vict_assets(void)
{
	remove_surface(vict_texmap, 0xf);
	remove_animation(submesh_info_vict, 0xa, 0x4e, "vict.za");
	remove_model((void **)&vict_verts, 0x127, (void **)&vict_faces, 0x210, (void **)&vict_normals, 0x164, "VICT.ZM");
}

/* 0x80106584: the init callback: the record (scale 1, hit radius 0.42, 0.75
   health, turned half round), ten groups, the hit group the body; it starts waiting */
int vict_init(game_object_t *obj)
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
	obj->hit = vict_hit;
	obj->die = vict_die;
	obj->remove = vict_remove;
	e = (enemy_t *)data;
	e->unk4 = 0x14;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0xc;
	e->target = get_target_player();
	e->unk54 = 0;
	e->rx = 0.0f;
	e->ry = 3.14159274f;
	e->rz = 0.0f;
	e->x = 0.0f;
	e->y = 0.0f;
	e->z = 0.0f;
	e->unkcc = 0.42f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 0.75f);
	e->groups = malloc(0xa * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return 1;
	}
	for(i = 0; i < 0xa; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->unk1a = 0x2012;
	init_groups(e, submesh_info_vict, 0xa);
	g = e->groups;
	e->hit_group = &g[9];
	g[9].floor = 0.0f;
	FLOOR(e) = 0.0f;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	vict_setup(e, VICT_WAIT);
	return 0;
}

/* 0x801066f4: the remove callback */
void vict_remove(game_object_t *obj)
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

/* 0x8010674c: the think callback: the flinch plays out back to waiting; the
   charge steps it towards a point near the camera (2 a step, 10.5 below and 3 in front of the
   view), and on arrival the player loses 100 health and it is gone; its parts' floor follows
   it 40 below */
void vict_think(game_object_t *obj)
{
	enemy_t	*e;

	if(debug_mode & 0x10000)
	{
		return;
	}
	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, 0xa);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case VICT_WAIT:
	case VICT_REST:
	case VICT_DEAD:
		break;
	case VICT_FLINCH:
		e->frame++;
		if(e->frame >= 0x4e)
		{
			e->frame = 0x3f;
			e->ai_state = VICT_WAIT;
		}
		break;
	case VICT_CHARGE:
		e->frame++;
		if(e->frame >= 0x3f)
		{
			e->frame = 0x30;
		}
		if(charge_camera(e, 2.0f, 0.0f, -10.5f, 3.0f) == 2)
		{
			hurt_player(e->target, 100, 3, 0.0f);
			remove_game_object(obj->id);
		}
		FLOOR(e) = e->y - 40.0f;
		break;
	default:
		printf("Unknown VICT ai_state 0x%04x\n", e->ai_state);
		break;
	}
}

/* 0x801068c8: put it in a state and its first frame */
void vict_setup(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	switch(state)
	{
	case VICT_WAIT:
		e->frame = 0x3f;
		break;
	case VICT_CHARGE:
		e->frame = 0x30;
		FLOOR(e) = e->y;
		break;
	case VICT_REST:
		e->frame = 0;
		break;
	default:
		e->frame = 0x30;
		break;
	}
}
