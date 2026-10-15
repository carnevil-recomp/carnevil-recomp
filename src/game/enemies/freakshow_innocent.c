/*
 * freakshow_innocent.c - the Freak Show innocent (object type 0x2028, the game's "finn"): the
 * same little girl as the Haunted House's, with the same model (FINN.ZM has INNG.ZM's counts and
 * checksum: 551 vertices, 953 faces, 613 normals in 17 submeshes), her own animation (finn.za,
 * 105 frames) and a 200-point path (finn.pth); her voice is the "girl" bank. One original file,
 * 0x801222d0-0x80122e3c (the word at 0x80122e3c is not code), tables at 0x80198fd0 (texmaps,
 * the INNG*.WMS names) and 0x801992b0 (submeshes), her cries at 0x801994d0.
 * Her texmap loader (0x801226b8) has no caller in the game: she draws with the Haunted House
 * girl's textures, which main_proc loads at boot under the same keys.
 * Shooting her costs the player 350 health and a penalty notice; she cries out and fades away,
 * and an enemy holding her (enemy_t.unkd8) lets go. The exported names are still the address
 * names spawn.c and wave.c use; each says what it is.
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
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	show_pickup_message(player_stats_t *, int);			/* player.c's file: a penalty notice */
extern void	func_800fa1c4(enemy_t *);				/* zombie.c: the holder lets go */
extern void	add_world_sprite(float *, int, int);
extern int	randrng(int);
extern int	remove_die();						/* spawn.c */
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *);
extern void	remove_path(void **, int, char *);
extern float	D_801faca4, D_801facb4;					/* the debug aim point (objects.c) */
extern enemy_t	*debug_enemy;
extern char	text_buffer[];

extern void	path_place(enemy_t *, float *, int, float);	/* enemy.c */

extern texmap_t	finn_texmap[23];		/* freakshow_innocent_data.c */
extern submesh_t finn_submesh[17];

float	*finn_verts;			/* 0x801e5b0c */
face_t	*finn_faces;			/* 0x80212e88 */
float	*finn_normals;			/* 0x801e57b8 */
float	*finn_path;			/* 0x802372b0: finn.pth, x, y, z and three angles a point */
int	finn_cries[3] = { 4, 5, 6 };	/* 0x801994d0: "girl" bank calls */

#define FINN_GROUPS	0x11
#define FINN_NVERTS	0x227
#define FINN_NFACES	0x3b9
#define FINN_NNORMALS	0x265
#define FINN_FRAMES	0x69
#define FINN_PATH_LEN	0xc8
#define FINN_CHEST	10		/* the submesh she is hit through */

/* her states (the spawn table's) */
#define FINN_IDLE	0		/* frames 0-104, looping */
#define FINN_LOOP	2		/* frames 14-32, looping, with call 3 */
#define FINN_TURN	3		/* frames 43-64 once with call 3, then a blend into FINN_STAND */
#define FINN_STAND	4		/* frames 65-104, looping, with call 1 */
#define FINN_PATH_LOOP	0x801		/* frames 0-13 looping while she runs her path between the
					   spawn entry's two points, round and round, with call 3 */
#define FINN_PATH_RUN	0x802		/* the same once, gone at the end */
#define FINN_SHOT	0x8000		/* fading out */

#define ALPHA_STEP(e)	(*(float *)&(e)->unk44)	/* 0x44: the fade's step per frame */
#define HOLDER(e)	((enemy_t *)(e)->unkd8)	/* 0xd8: the enemy holding her */

int	func_801224e8(game_object_t *, player_stats_t *);
int	func_801225ac(game_object_t *, player_stats_t *);
void	func_80122974(game_object_t *);
void	func_80122ca8(enemy_t *, unsigned short);

/* 0x801222d0: the draw callback (finn_draw); in debug mode 2 she also takes the debug aim's
   shots. The debug label is red for the debug enemy, else white */
void func_801222d0(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, finn_submesh, FINN_GROUPS, finn_verts, FINN_NVERTS, finn_faces, FINN_NFACES, finn_normals, FINN_NNORMALS);
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
		func_801224e8(obj, &player_stats[0]);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[FINN_CHEST].pos, e->radius, 0x600000);
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

/* 0x801224e8: the hit test (finn_hit): the bounding sphere, then the submesh */
int func_801224e8(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		if((group = find_submesh_hit(e, finn_submesh, FINN_GROUPS, p)) != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)func_801225ac;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return 0;
}

/* 0x801225a4: an empty function the file keeps */
void func_801225a4(void)
{
}

/* 0x801225ac: she was shot (finn_die): the player loses 350 health and gets the penalty
   notice, a hit mark appears on her if she is on screen, she fades out, whoever held her lets
   go and she cries out. In the character debug mode the shot only selects her */
int func_801225ac(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e, *holder;
	float	scr[4];

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return 0;
	}
	if(e->ai_state != FINN_SHOT)
	{
		show_pickup_message(p, 5);
		hurt_player(p, 0x15e, 5, 0.0f);
		if(get_screen_pos(e->hit_group->pivot, scr) != -1.0f)
		{
			add_world_sprite(e->groups[2].pivot, 7, 0);
		}
		func_80122ca8(e, FINN_SHOT);
		holder = HOLDER(e);
		if(holder)
		{
			holder->unkdc = 0;
			e->unkd8 = 0;
			func_800fa1c4(holder);
		}
		play_enemy_sound(e, finn_cries[randrng(3)], "girl", 2);
	}
	return 0;
}

/* 0x801226b8: her texmaps (nothing calls it) */
void func_801226b8(void)
{
	load_texmap(finn_texmap, 0x17, "finn");
}

/* 0x801226e8 */
void load_finn(void)
{
	load_model((void **)&finn_verts, FINN_NVERTS, (void **)&finn_faces, FINN_NFACES, (void **)&finn_normals, FINN_NNORMALS, "FINN.ZM", (int)0xe0d58427);
	load_anim(finn_submesh, FINN_GROUPS, FINN_FRAMES, "finn.za", (int)0x1f27ec68);
	load_path((void **)&finn_path, FINN_PATH_LEN, "finn.pth", 0x4847026d);
}

/* 0x80122790: the assets go when the level is unloaded (sic: the game's name) */
void remove_finn_assts(void)
{
	remove_path((void **)&finn_path, FINN_PATH_LEN, "finn.pth");
	remove_animation(finn_submesh, FINN_GROUPS, FINN_FRAMES, "finn.za");
	remove_model((void **)&finn_verts, FINN_NVERTS, (void **)&finn_faces, FINN_NFACES, (void **)&finn_normals, FINN_NNORMALS, "FINN.ZM");
}

/* 0x80122818: the init callback (finn_init): the record (scale 1, hit radius as scaled,
   0.75 health), seventeen groups, the hit group the chest */
int func_80122818(game_object_t *obj)
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
	obj->hit = func_801224e8;
	obj->die = func_801225ac;
	obj->remove = func_80122974;
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
	e->x = 0.0f;
	e->y = 0.0f;
	e->z = 0.0f;
	e->unkcc = 1.0f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 0.75f);
	e->groups = malloc(FINN_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return 1;
	}
	for(i = 0; i < FINN_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	init_groups(e, finn_submesh, FINN_GROUPS);
	g = e->groups;
	e->hit_group = &g[FINN_CHEST];
	g[FINN_CHEST].floor = 0.0f;
	clear_enemy_fields(e);
	e->prio_base = 0x38;
	e->prio_flags = next_cue_priority();
	func_80122ca8(e, FINN_IDLE);
	return 0;
}

/* 0x80122974: the remove callback (finn_remove) */
void func_80122974(game_object_t *obj)
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

/* 0x801229d4: the think callback (finn_think): step the animation for her state; the path
   states move her one point a frame; the fade ends with her removal once transparent */
void func_801229d4(game_object_t *obj)
{
	enemy_t	*e;
	int	i;

	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, FINN_GROUPS);
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
	case FINN_LOOP:
		if(++e->frame >= 0x21)
		{
			e->frame = 0xe;
		}
		return;
	case FINN_STAND:
		if(++e->frame >= FINN_FRAMES)
		{
			e->frame = 0x41;
		}
		return;
	case FINN_TURN:
		if(++e->frame >= 0x41)
		{
			e->frame--;
			blend_to_frame(e, finn_submesh, 0xc, 0x41, 3);
			e->ai_state = FINN_STAND;
		}
		return;
	case FINN_PATH_LOOP:
		if(++e->frame >= 0xe)
		{
			e->frame = 0;
		}
		path_place(e, finn_path, e->path, 1.0f);
		if(++e->path > e->unk92)
		{
			e->path = e->unk90;
		}
		return;
	case FINN_PATH_RUN:
		if(++e->frame >= 0xe)
		{
			e->frame = 0;
		}
		path_place(e, finn_path, e->path, 1.0f);
		if(++e->path > e->unk92)
		{
			break;
		}
		return;
	case FINN_IDLE:
		if(++e->frame >= FINN_FRAMES)
		{
			e->frame = 0;
		}
		return;
	case FINN_SHOT:
		e->alpha = e->alpha + ALPHA_STEP(e);
		if(e->alpha > 0.0f)
		{
			for(i = 0; i < FINN_GROUPS; i++)
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

/* 0x80122ca8: put her in a state (finn_setup): the voiced states call out; the path states put
   her on her first point; the fade starts opaque and loses 10 a frame with every group alpha
   blended */
void func_80122ca8(enemy_t *e, unsigned short state)
{
	int	i;

	e->ai_state = state;
	switch(state)
	{
	case FINN_PATH_LOOP:
		e->frame = 0;
		play_enemy_sound(e, 3, "girl", 0);
		path_place(e, finn_path, e->path, 1.0f);
		break;
	case FINN_PATH_RUN:
		e->frame = 0;
		path_place(e, finn_path, e->path, 1.0f);
		play_enemy_sound(e, 3, "girl", 0);
		break;
	case FINN_LOOP:
		e->frame = 0xe;
		play_enemy_sound(e, 3, "girl", 0);
		break;
	case FINN_TURN:
		e->frame = 0x2b;
		play_enemy_sound(e, 3, "girl", 0);
		break;
	case FINN_STAND:
		play_enemy_sound(e, 1, "girl", 0);
		e->frame = 0x41;
		break;
	case FINN_SHOT:
		e->alpha = 255.0f;
		ALPHA_STEP(e) = -10.0f;
		e->flags &= ~2;
		for(i = 0; i < FINN_GROUPS; i++)
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
