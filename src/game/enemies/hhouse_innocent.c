/*
 * hhouse_innocent.c - the Haunted House innocent (object type 0x2011): the same little girl as
 * Rickety Town's (her own copy of the model, INNG.ZM / inng.za, 551 vertices, 613 normals,
 * 953 faces in 17 submeshes, 243 animation frames; the INNG*.WMS texmaps; her voice in the
 * "girl" bank). One original file, 0x80105388-0x80105e38, tables at 0x80191fd0 (texmaps),
 * 0x801922b0 (submeshes) and 0x801924d0 (her cries), plus the walk she follows at 0x80182938.
 * The game loads these textures once at boot (main_proc), so Rickety Town's girl
 * (rickity_innocent.c) draws with them too; her own copy of the table is never loaded.
 * Shooting her costs the player 350 health and a penalty notice, she cries out and fades away;
 * an enemy holding her (enemy_t.unkd8) lets go. The exported names are still the address
 * names spawn.c and the asm use; each says what it is.
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

extern void	path_place(enemy_t *, float *, int, float);	/* enemy.c */

extern texmap_t	inng_texmap[0x17];		/* hhouse_innocent_data.c */
extern submesh_t inng_submesh[17];
extern float	inng_path[86 * 6];		/* the walk of state INNOCENT_WALK: x, y, z and three angles a point */

float	*inng_verts;			/* 0x801e5b1c */
face_t	*inng_faces;			/* 0x80212e90 */
float	*inng_normals;			/* 0x801e57c4 */
int	inng_cries[3] = { 4, 5, 6 };	/* 0x801924d0: "girl" bank calls */

/* her states (the spawn table's) */
#define INNOCENT_IDLE		0		/* frames 0-242, looping */
#define INNOCENT_LEAVE		0x102		/* frames 120-172 once with call 1, then gone */
#define INNOCENT_WALK		0x180		/* frames 0-60 looping while she follows inng_path from the
						   spawn entry's start point to its end, with call 2 */
#define INNOCENT_HOLD		0x400		/* frames 104-119 once, then she holds the last */
#define INNOCENT_POSE		0x2000		/* frame 61, held */
#define INNOCENT_CRY		0x2001		/* frames 193-242 looping, with call 3 */
#define INNOCENT_SHOT		0x8000		/* fading out */

#define ALPHA_STEP(e)	(*(float *)&(e)->unk44)	/* 0x44: the fade's step per frame */
#define HOLDER(e)	((enemy_t *)(e)->unkd8)	/* 0xd8: the enemy holding her */

int	inng_hit(game_object_t *, player_stats_t *);
int	inng_die(game_object_t *, player_stats_t *);
void	inng_remove(game_object_t *);
void	inng_setup(enemy_t *, unsigned short);

/* 0x80105388: the draw callback; in debug mode 2 she also takes the debug aim's
   shots. The debug label is red for the debug enemy, magenta while she walks, else white */
void inng_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, inng_submesh, 0x11, inng_verts, 0x227, inng_faces, 0x3b9, inng_normals, 0x265);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		if(e == debug_enemy)
		{
			draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xff0000);
		}
		else if(e->ai_state == INNOCENT_WALK)
		{
			draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xff00ff);
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
		inng_hit(obj, &player_stats[0]);
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

/* 0x801055c4: the hit test: the bounding sphere, then the submesh */
int inng_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		if((group = find_submesh_hit(e, inng_submesh, 0x11, p)) != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)inng_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return 0;
}

/* 0x80105680: an empty function the file keeps */
void func_80105680(void)
{
}

/* 0x80105688: she was shot: the player loses 350 health and gets the penalty
   notice, a hit mark appears on her if she is on screen, she fades out, whoever held her lets
   go and she cries out. In the character debug mode the shot only selects her */
int inng_die(game_object_t *obj, player_stats_t *p)
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
		inng_setup(e, INNOCENT_SHOT);
		holder = HOLDER(e);
		if(holder)
		{
			holder->unkdc = 0;
			e->unkd8 = 0;
			zombie_walk(holder);
		}
		play_enemy_sound(e, inng_cries[randrng(3)], "girl", 8);
	}
	return 0;
}

/* 0x80105794 */
void load_inng_texmap(void)
{
	load_texmap(inng_texmap, 0x17, "inng");
}

/* 0x801057c4 */
void load_inng(void)
{
	load_model((void **)&inng_verts, 0x227, (void **)&inng_faces, 0x3b9, (void **)&inng_normals, 0x265, "INNG.ZM", (int)0xe0d58427);
	load_anim(inng_submesh, 0x11, 0xf3, "inng.za", (int)0x674a9197);
}

/* 0x8010584c: remove_inng_assets */
void remove_inng_assets(void)
{
	remove_animation(inng_submesh, 0x11, 0xf3, "inng.za");
	remove_model((void **)&inng_verts, 0x227, (void **)&inng_faces, 0x3b9, (void **)&inng_normals, 0x265, "INNG.ZM");
}

/* 0x801058bc: the init callback: the record (scale 1, hit radius as scaled,
   0.75 health), seventeen groups, the hit group the chest */
int inng_init(game_object_t *obj)
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
	obj->hit = inng_hit;
	obj->die = inng_die;
	obj->remove = inng_remove;
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
	init_groups(e, inng_submesh, 0x11);
	g = e->groups;
	e->hit_group = &g[10];
	g[10].floor = 0.0f;
	clear_enemy_fields(e);
	e->prio_base = 0x38;
	e->prio_flags = next_cue_priority();
	inng_setup(e, INNOCENT_IDLE);
	return 0;
}

/* 0x80105a18: the remove callback */
void inng_remove(game_object_t *obj)
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

/* 0x80105a78: the think callback: step the animation for her state; the walk
   moves her one path point a frame and plays the idle animation's step as well; the leave
   ends with her removal, the fade with her gone once transparent */
void inng_think(game_object_t *obj)
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
	case INNOCENT_CRY:
		e->frame++;
		if(e->frame >= 0xf3)
		{
			e->frame = 0xc1;
		}
		return;
	case INNOCENT_HOLD:
		if(e->frame < 0x77)
		{
			e->frame++;
		}
		return;
	case INNOCENT_POSE:
		return;
	case INNOCENT_LEAVE:
		e->frame++;
		if(e->frame < 0xad)
		{
			return;
		}
		break;
	case INNOCENT_WALK:
		e->frame++;
		if(e->frame >= 0x3d)
		{
			e->frame = 0;
		}
		path_place(e, inng_path, e->path, 1.0f);
		e->path++;
		if(e->path >= e->unk92)
		{
			remove_die(obj->id, 0);
		}
		/* fall through */
	case INNOCENT_IDLE:
		e->frame++;
		if(e->frame >= 0xf3)
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

/* 0x80105cb4: put her in a state: the voiced states call out; the walk puts her
   on her first path point; the fade starts opaque and loses 10 a frame with every group alpha
   blended */
void inng_setup(enemy_t *e, unsigned short state)
{
	int	i;

	e->ai_state = state;
	switch(state)
	{
	case INNOCENT_CRY:
		e->frame = 0xc1;
		play_enemy_sound(e, 3, "girl", 0);
		break;
	case INNOCENT_LEAVE:
		play_enemy_sound(e, 1, "girl", 0);
		e->frame = 0x78;
		break;
	case INNOCENT_HOLD:
		e->frame = 0x68;
		break;
	case INNOCENT_POSE:
		e->frame = 0x3d;
		break;
	case INNOCENT_WALK:
		play_enemy_sound(e, 2, "girl", 0);
		e->frame = 0;
		path_place(e, inng_path, e->path, 1.0f);
		e->counter = 0;
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
