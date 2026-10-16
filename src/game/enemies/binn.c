/*
 * binn.c - the Big Top innocent (object type 0x2067, the game's "binn"): the little girl of the
 * other levels once more, with the same model (BINN.ZM has INNG.ZM's counts and checksum: 551
 * vertices, 953 faces, 613 normals in 17 submeshes), her own animation (binn.za, 203 frames) and
 * no path; her voice is the "girl" bank. One original file, 0x8013ea08-0x8013f464 (the word at
 * 0x8013ea04 before it is not code; 0x8013f464, a vertical blank handler, begins the next file),
 * tables at 0x801a20b8 (texmaps, the INNG*.WMS names) and 0x801a2398 (submeshes, binn_data.c),
 * her cries at 0x801a25b8.
 * Shooting her costs the player 350 health and a penalty notice; she cries out and fades away,
 * and an enemy holding her (enemy_t.unkd8) lets go. Her other states only play animations: 0 and
 * 3 to 6 loop their stretch, 1 and 2 play theirs once and she goes. The exported names are still
 * the address names spawn.c and wave.c use; each says what it is.
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

extern void	draw_effects(enemy_t *);
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
extern void	clear_enemy_fields(enemy_t *);
extern void	unlink_enemy(enemy_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	show_pickup_message(player_stats_t *, int);
extern void	zombie_walk(enemy_t *);				/* zombie.c: the holder lets go */
extern void	add_world_sprite(float *, int, int);
extern int	randrng(int);
extern int	remove_die();
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *);
extern float	D_801faca4, D_801facb4;					/* the debug aim point (objects.c) */
extern enemy_t	*debug_enemy;
extern char	text_buffer[];

extern texmap_t	binn_texmap[23];		/* binn_data.c */
extern submesh_t binn_submesh[17];

float	*binn_verts;			/* 0x801e591c */
face_t	*binn_faces;			/* 0x80212e08 */
float	*binn_normals;			/* 0x801e1774 */
int	binn_cries[3] = { 4, 5, 6 };	/* 0x801a25b8: "girl" bank calls */

#define BINN_GROUPS	0x11
#define BINN_NVERTS	0x227
#define BINN_NFACES	0x3b9
#define BINN_NNORMALS	0x265
#define BINN_FRAMES	0xcb
#define BINN_CHEST	10		/* the submesh she is hit through */

/* her states (the spawn table's) */
#define BINN_IDLE	0		/* frames 0-202, looping */
#define BINN_ONCE	1		/* frames 0-52 once with call 1, then gone */
#define BINN_ONCE2	2		/* frames 53-108 once with call 3, then gone */
#define BINN_LOOP	3		/* frames 0-52, looping, with call 3 */
#define BINN_LOOP2	4		/* frames 109-128, looping, with call 3 */
#define BINN_LOOP3	5		/* frames 164-202, looping, with call 3 */
#define BINN_LOOP4	6		/* frames 129-163, looping, with call 3 */
#define BINN_SHOT	0x8000		/* fading out */

#define ALPHA_STEP(e)	(*(float *)&(e)->unk44)	/* 0x44: the fade's step per frame */
#define HOLDER(e)	((enemy_t *)(e)->unkd8)	/* 0xd8: the enemy holding her */

int	func_8013ec20(game_object_t *, player_stats_t *);
int	func_8013ece4(game_object_t *, player_stats_t *);
void	func_8013f07c(game_object_t *);
void	func_8013f324(enemy_t *, unsigned short);

/* 0x8013ea08: the draw callback (binn_draw); in debug mode 2 she also takes the debug aim's
   shots. The debug label is red for the debug enemy, else white */
void func_8013ea08(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, binn_submesh, BINN_GROUPS, binn_verts, BINN_NVERTS, binn_faces, BINN_NFACES, binn_normals, BINN_NNORMALS);
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
		func_8013ec20(obj, &player_stats[0]);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[BINN_CHEST].pos, e->radius, 0x600000);
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

/* 0x8013ec20: the hit test (binn_hit): the bounding sphere, then the submesh */
int func_8013ec20(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		if((group = find_submesh_hit(e, binn_submesh, BINN_GROUPS, p)) != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)func_8013ece4;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return 0;
}

/* 0x8013ecdc: an empty function the file keeps */
void func_8013ecdc(void)
{
}

/* 0x8013ece4: she was shot (binn_die): the player loses 350 health and gets the penalty notice,
   a hit mark appears on her if she is on screen, she fades out, whoever held her lets go and she
   cries out. In the character debug mode the shot only selects her */
int func_8013ece4(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e, *holder;
	float	scr[4];

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return 0;
	}
	if(e->ai_state != BINN_SHOT)
	{
		show_pickup_message(p, 5);
		hurt_player(p, 0x15e, 5, 0.0f);
		if(get_screen_pos(e->hit_group->pivot, scr) != -1.0f)
		{
			add_world_sprite(e->groups[2].pivot, 7, 0);
		}
		func_8013f324(e, BINN_SHOT);
		holder = HOLDER(e);
		if(holder)
		{
			holder->unkdc = 0;
			e->unkd8 = 0;
			zombie_walk(holder);
		}
		play_enemy_sound(e, binn_cries[randrng(3)], "girl", 8);
	}
	return 0;
}

/* 0x8013edf0 */
void load_binn_texmap(void)
{
	load_texmap(binn_texmap, 0x17, "binn");
}

/* 0x8013ee20 */
void load_binn(void)
{
	load_model((void **)&binn_verts, BINN_NVERTS, (void **)&binn_faces, BINN_NFACES, (void **)&binn_normals, BINN_NNORMALS, "BINN.ZM", (int)0xe0d58427);
	load_anim(binn_submesh, BINN_GROUPS, BINN_FRAMES, "binn.za", (int)0x8036f250);
}

/* 0x8013eea8: the assets go when the level is unloaded */
void remove_binn_assets(void)
{
	remove_animation(binn_submesh, BINN_GROUPS, BINN_FRAMES, "binn.za");
	remove_model((void **)&binn_verts, BINN_NVERTS, (void **)&binn_faces, BINN_NFACES, (void **)&binn_normals, BINN_NNORMALS, "BINN.ZM");
}

/* 0x8013ef18: the init callback (binn_init): the record (8 units to the left, scale 1, hit radius
   0.9 of it, 0.75 health), seventeen groups, the hit group the chest */
int func_8013ef18(game_object_t *obj)
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
	obj->hit = func_8013ec20;
	obj->die = func_8013ece4;
	obj->remove = func_8013f07c;
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
	e->groups = malloc(BINN_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return 1;
	}
	for(i = 0; i < BINN_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	init_groups(e, binn_submesh, BINN_GROUPS);
	g = e->groups;
	e->hit_group = &g[BINN_CHEST];
	g[BINN_CHEST].floor = 0.0f;
	clear_enemy_fields(e);
	e->prio_base = 0x38;
	e->prio_flags = next_cue_priority();
	func_8013f324(e, BINN_IDLE);
	return 0;
}

/* 0x8013f07c: the remove callback (binn_remove) */
void func_8013f07c(game_object_t *obj)
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

/* 0x8013f0dc: the think callback (binn_think): step the animation for her state; the fade ends
   with her removal once transparent, as do the play-once states at their last frame */
void func_8013f0dc(game_object_t *obj)
{
	enemy_t	*e;
	int	i;

	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, BINN_GROUPS);
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
	case BINN_ONCE:
		if(++e->frame >= 0x35)
		{
			break;
		}
		return;
	case BINN_ONCE2:
		if(++e->frame >= 0x6d)
		{
			break;
		}
		return;
	case BINN_LOOP:
		if(++e->frame >= 0x35)
		{
			e->frame = 0;
		}
		return;
	case BINN_LOOP2:
		if(++e->frame >= 0x81)
		{
			e->frame = 0x6d;
		}
		return;
	case BINN_LOOP3:
		if(++e->frame >= BINN_FRAMES)
		{
			e->frame = 0xa4;
		}
		return;
	case BINN_LOOP4:
		if(++e->frame >= 0xa4)
		{
			e->frame = 0x81;
		}
		return;
	case BINN_IDLE:
		if(++e->frame >= BINN_FRAMES)
		{
			e->frame = 0;
		}
		return;
	case BINN_SHOT:
		e->alpha = e->alpha + ALPHA_STEP(e);
		if(e->alpha > 0.0f)
		{
			for(i = 0; i < BINN_GROUPS; i++)
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

/* 0x8013f324: put her in a state (binn_setup): the voiced states call out and start their
   stretch; the fade starts opaque and loses 10 a frame with every group alpha blended */
void func_8013f324(enemy_t *e, unsigned short state)
{
	int	i;

	e->ai_state = state;
	switch(state)
	{
	case BINN_LOOP:
		e->frame = 0;
		play_enemy_sound(e, 3, "girl", 0);
		break;
	case BINN_LOOP2:
		e->frame = 0x6d;
		play_enemy_sound(e, 3, "girl", 0);
		break;
	case BINN_ONCE2:
		e->frame = 0x35;
		play_enemy_sound(e, 3, "girl", 0);
		break;
	case BINN_LOOP4:
		e->frame = 0x81;
		play_enemy_sound(e, 3, "girl", 0);
		break;
	case BINN_LOOP3:
		e->frame = 0xa4;
		play_enemy_sound(e, 3, "girl", 0);
		break;
	case BINN_ONCE:
		play_enemy_sound(e, 1, "girl", 0);
		e->frame = 0;
		break;
	case BINN_SHOT:
		e->alpha = 255.0f;
		ALPHA_STEP(e) = -10.0f;
		e->flags &= ~2;
		for(i = 0; i < BINN_GROUPS; i++)
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
