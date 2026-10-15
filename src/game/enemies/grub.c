/*
 * grub.c - the Freak Show's grub (object type 0x2023, assets GRUB.ZM / grub.za / grub.pth and
 * the grub*.WMS texmaps: face, tip, flesh; its sounds are in the level's "freaksho" bank). One
 * original file, 0x8011b510-0x8011bf38, its tables at 0x801964b0 (texmaps) and 0x801965d0
 * (submeshes, grub_data.c). The model has 138 vertices, 248 faces and 139 normals in 5
 * submeshes, the animation 79 frames, the path 933 points.
 *
 * It crawls its path: state 0x801 loops it, 0x802 crawls it once (with a call as it sets off and
 * another 15 points before the end) and bites at the end, 50 damage when its head (part 0) is
 * within 28 units, then goes; 0x803 and 0 only animate. One shot at 0.75 health kills it (120
 * points, 10 for a hit that does not): it plays out its death (0x2001) and is removed.
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

extern void	draw_effects(enemy_t *);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern player_stats_t *attack_target(enemy_t *);
extern int	randrng(int);
extern void	path_place(enemy_t *, void *, short, float);
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern int	group_near_camera(group_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern int	remove_die();
extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern char	text_buffer[];
extern enemy_t	*debug_enemy;

extern texmap_t	grub_texmap[9];			/* grub_data.c */
extern submesh_t submesh_info_grub[5];

float	*grub_verts;			/* 0x801e5b30 */
face_t	*grub_faces;			/* 0x8021ef50 */
float	*grub_normals;			/* 0x801e57ec */
void	*grub_path;			/* 0x802372d4 */

#define GRUB_GROUPS	5
#define GRUB_NVERTS	0x8a
#define GRUB_NFACES	0xf8
#define GRUB_NNORMALS	0x8b
#define GRUB_FRAMES	0x4f
#define GRUB_PATH_LEN	0x3a5
#define GRUB_HEAD	0		/* the part that bites */
#define GRUB_HIT_GROUP	1		/* the submesh it is hit through */
#define GRUB_BITE	50		/* the damage of its bite */

int	grub_die(game_object_t *, player_stats_t *);
void	grub_remove(game_object_t *);
void	grub_setup(enemy_t *, unsigned short);

/* 0x8011b510: the draw callback */
void grub_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_grub, GRUB_GROUPS, grub_verts, GRUB_NVERTS, grub_faces, GRUB_NFACES, grub_normals, GRUB_NNORMALS);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->hit_group->pos, e->radius, 0x600000);
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

/* 0x8011b70c: the hit test: the bounding sphere, then the submesh */
int grub_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_grub, GRUB_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)grub_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x8011b7c8: the shot lands: blood and chunks, a squeal; dead, it plays its death (120 points),
   else 10 points */
int grub_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return(0);
	}
	burst_blood(p->hit_point, e->unk98, 0x800000);
	add_chunks(e->hit_group->pos, e->unk98);
	add_world_sprite(p->hit_point, 0, 0);
	if(!(e->ai_state & 0x6000))
	{
		play_enemy_sound(e, randrng(3) + 0x34, "freaksho", 8);
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			award_points(p->index, 120);
			e->ai_state = 0x2001;
			e->killer = p;
			e->frame = 0x3b;
			e->flags &= 0xfffd;
		}
		else
		{
			award_points(p->index, 10);
		}
	}
	return(0);
}

/* 0x8011b8ec */
void load_grub_texmap(void)
{
	load_texmap(grub_texmap, 9, "grub");
}

/* 0x8011b91c */
void load_grub(void)
{
	load_model((void **)&grub_verts, GRUB_NVERTS, (void **)&grub_faces, GRUB_NFACES, (void **)&grub_normals, GRUB_NNORMALS, "GRUB.ZM", 0x855e1911);
	load_anim(submesh_info_grub, GRUB_GROUPS, GRUB_FRAMES, "grub.za", 0x4ebcae02);
	load_path(&grub_path, GRUB_PATH_LEN, "grub.pth", 0x5966b407);
}

/* 0x8011b9c4: the assets go when the level is unloaded */
void remove_grub_assets(void)
{
	remove_surface(grub_texmap, 9);
	remove_path(&grub_path, GRUB_PATH_LEN, "grub.pth");
	remove_animation(submesh_info_grub, GRUB_GROUPS, GRUB_FRAMES, "grub.za");
	remove_model((void **)&grub_verts, GRUB_NVERTS, (void **)&grub_faces, GRUB_NFACES, (void **)&grub_normals, GRUB_NNORMALS, "GRUB.ZM");
}

/* 0x8011ba5c: the init callback: the record, health 0.75, the five groups */
int grub_init(game_object_t *obj)
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
	obj->hit = grub_hit;
	obj->die = grub_die;
	obj->remove = grub_remove;
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
	e->y = 0;
	e->z = 0;
	e->ry = 3.1415927f;
	e->x = 5.0f;
	e->unkcc = 0.32f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 0.75f);
	e->groups = malloc(GRUB_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < GRUB_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	init_groups(e, submesh_info_grub, GRUB_GROUPS);
	g = e->groups;
	e->hit_group = &g[GRUB_HIT_GROUP];
	g[GRUB_HIT_GROUP].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	grub_setup(e, 0);
	return(0);
}

/* 0x8011bbdc: the remove callback */
void grub_remove(game_object_t *obj)
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

/* 0x8011bc34: the think callback */
void grub_think(game_object_t *obj)
{
	enemy_t	*e;

	if(debug_mode & 0x10000)
	{
		return;
	}
	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, GRUB_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x801:
		if(++e->frame >= 0xc)
		{
			e->frame = 0;
		}
		path_place(e, grub_path, e->path, 1.0f);
		if(++e->path > e->unk92)
		{
			e->path = e->unk90;
		}
		break;
	case 0x803:
		if(++e->frame >= 0x3b)
		{
			e->frame = 0xc;
		}
		break;
	case 0x802:
		if(++e->frame >= 0xc)
		{
			e->frame = 0;
		}
		path_place(e, grub_path, e->path, 1.0f);
		if(++e->path == e->unk92 - 15)
		{
			play_enemy_sound(e, 0x3c, "freaksho", 0);
		}
		if(e->path > e->unk92)
		{
			if(group_near_camera(&e->groups[GRUB_HEAD], 28.0f))
			{
				hurt_player(attack_target(e), GRUB_BITE, 9, 0.0f);
			}
			remove_die(obj->id, 0);
		}
		break;
	case 0x2001:
		if(++e->frame >= GRUB_FRAMES)
		{
			remove_die(obj->id, e->killer);
		}
		break;
	case 0:
		if(++e->frame >= GRUB_FRAMES)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x8011be94: the spawn table's state; 0x802 sets off with a call */
void grub_setup(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	switch(state)
	{
	case 0x802:
		play_enemy_sound(e, randrng(5) + 0x37, "freaksho", 0);
	case 0x801:
		e->frame = 0;
		path_place(e, grub_path, e->path, 1.0f);
		break;
	case 0x803:
		e->frame = 0xc;
		break;
	case 0:
		e->frame = 0;
		break;
	default:
		e->frame = 0;
		break;
	}
}
