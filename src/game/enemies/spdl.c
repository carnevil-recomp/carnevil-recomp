/*
 * spdl.c - the Haunted House's SPDL spider (object type 0x2013, assets SPDL.ZM / spdl.za /
 * spdl.pth and the spdr*.WMS texmaps: fur, skull, "pnch" and face). One original file,
 * 0x80106928-0x801072e0, its tables at 0x80192800 (texmaps) and 0x80192900 (submeshes,
 * spdl_data.c). The model has 196 vertices, 267 faces and 217 normals in 6 submeshes, the
 * animation 53 frames, the path 1218 points.
 *
 * It runs along its path and does not fight back on the way: state 0x180 loops the path, 0x181
 * and 0x182 run it once and bite when they reach the player (100 damage). 0x181 bites as soon as
 * its body (submesh 5) is within 10 units of the camera, 0x182 only at the path's end and within
 * 20. After a bite or at the path's end it waits five frames in 0x400 and is removed. One shot
 * at 0.75 health kills it: it is removed at once (250 points), 50 points for a shot that does not.
 */
#include <stdlib.h>
#include <glide/glide.h>
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "texmap.h"
#include "credits.h"			/* debug_mode, player_stats_t */

extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);	/* text at a world position */
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern player_stats_t *attack_target(enemy_t *);
extern void	path_place(enemy_t *, void *, short, float);
extern int	group_near_camera(group_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern int	remove_die();
extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern char	text_buffer[];

extern texmap_t	spdl_texmap[8];			/* spdl_data.c */
extern submesh_t submesh_info_spdl[6];

float	*spdl_verts;			/* 0x801e5b6c */
face_t	*spdl_faces;			/* 0x8021f080 */
float	*spdl_normals;			/* 0x801e5820 */
void	*spdl_path;			/* 0x802373c0 */

#define SPDL_GROUPS	6
#define SPDL_NVERTS	0xc4
#define SPDL_NFACES	0x10b
#define SPDL_NNORMALS	0xd9
#define SPDL_FRAMES	0x35
#define SPDL_PATH_LEN	0x4c2
#define SPDL_BODY	5		/* the submesh it is hit through and measured by */
#define SPDL_BITE	100		/* the damage of its bite */

/* the states */
#define SPDL_LOOP	0x180		/* runs its path round and round */
#define SPDL_RUN	0x181		/* runs its path, biting as soon as it is near */
#define SPDL_LEAP	0x182		/* runs its path (frames 28-52), biting at the end */
#define SPDL_DONE	0x400		/* waits out the counter, then goes */
#define SPDL_DONE_WAIT	5

int	func_80106c08(game_object_t *, player_stats_t *);
void	func_80106fe4(game_object_t *);
void	func_8010725c(enemy_t *, int);

/* 0x80106928: the draw callback */
void func_80106928(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	draw_model(e, submesh_info_spdl, SPDL_GROUPS, spdl_verts, SPDL_NVERTS, spdl_faces, SPDL_NFACES, spdl_normals, SPDL_NNORMALS);
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
	if(debug_mode & 0x1000)
	{
		func_800c99e4(e, 0xff0000);
	}
}

/* 0x80106abc: the hit test: the bounding sphere, then the submesh */
int func_80106abc(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		if((unsigned int)p->unk10 != 0xffff0000)
		{
			p->unk10 = 0xffffff00;
		}
		group = find_submesh_hit(e, submesh_info_spdl, SPDL_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)func_80106c08;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x80106b8c: the damage half of the die callback on its own (nothing calls it) */
void func_80106b8c(game_object_t *obj, player_stats_t *p, float damage, enemy_t *e)
{
	e->health -= damage;
	if(e->health <= 0.0f)
	{
		award_points(p->index, 250);
		remove_die(obj->id, e->killer);
	}
	else
	{
		award_points(p->index, 50);
	}
}

/* 0x80106c08: the shot lands: the part hit turns gory, blood, the damage; dead, it goes at once */
int func_80106c08(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;

	e = (enemy_t *)obj->data;
	g = &e->groups[p->hit_group];
	g->flags |= 0xf0000000;
	burst_blood(p->hit_point, e->y - 20.0f, 0xff0000);
	add_world_sprite(p->hit_point, 0, 0);
	e->health -= shot_damage_of(p);
	if(e->health <= 0.0f)
	{
		award_points(p->index, 250);
		remove_die(obj->id, e->killer);
	}
	else
	{
		award_points(p->index, 50);
	}
	return(0);
}

/* 0x80106d08 */
void load_spdl_texmap(void)
{
	load_texmap(spdl_texmap, 8, "spdl");
}

/* 0x80106d38 */
void load_spdl(void)
{
	load_model((void **)&spdl_verts, SPDL_NVERTS, (void **)&spdl_faces, SPDL_NFACES, (void **)&spdl_normals, SPDL_NNORMALS, "SPDL.ZM", 0x5556b03a);
	load_anim(submesh_info_spdl, SPDL_GROUPS, SPDL_FRAMES, "spdl.za", 0x82625d86);
	load_path(&spdl_path, SPDL_PATH_LEN, "spdl.pth", 0x0fec85a9);
}

/* 0x80106de0: the assets go when the level is unloaded */
void func_80106de0(void)
{
	remove_path(&spdl_path, SPDL_PATH_LEN, "spdl.pth");
	remove_animation(submesh_info_spdl, SPDL_GROUPS, SPDL_FRAMES, "spdl.za");
	remove_model((void **)&spdl_verts, SPDL_NVERTS, (void **)&spdl_faces, SPDL_NFACES, (void **)&spdl_normals, SPDL_NNORMALS, "SPDL.ZM");
	remove_surface(spdl_texmap, 8);
}

/* 0x80106e78: the init callback: the record, scale and health 0.75, the six groups */
int func_80106e78(game_object_t *obj)
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
	obj->hit = func_80106abc;
	obj->die = func_80106c08;
	obj->remove = func_80106fe4;
	e = (enemy_t *)data;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0xc;
	e->ai_state = 0;
	e->target = get_target_player();
	e->rx = 0;
	e->ry = 0;
	e->rz = 0;
	e->z = 0;
	e->x = -7.0f;
	e->y = 2.0f;
	e->unkcc = 0.3f;
	set_enemy_scale(e, 0.75f);
	e->vel[0] = 0;
	e->vel[1] = 0;
	e->vel[2] = 0;
	set_enemy_health(e, 0.75f);
	e->groups = malloc(SPDL_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < SPDL_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	init_groups(e, submesh_info_spdl, SPDL_GROUPS);
	g = e->groups;
	e->hit_group = &g[SPDL_BODY];
	g[SPDL_BODY].floor = 0.0f;
	func_8010725c(e, 0);
	return(0);
}

/* 0x80106fe4: the remove callback */
void func_80106fe4(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	free(e->groups);
	free(e);
}

/* 0x8010701c: the think callback: the animation, the run along the path and the bite */
void func_8010701c(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	switch(e->ai_state)
	{
	case SPDL_LOOP:
		if(++e->frame >= 9)
		{
			e->frame = 0;
		}
		path_place(e, spdl_path, e->path, 1.0f);
		if(++e->path > e->unk92)
		{
			e->path = e->unk90;
		}
		break;
	case SPDL_RUN:
		if(++e->frame >= 9)
		{
			e->frame = 0;
		}
		path_place(e, spdl_path, e->path, 1.0f);
		e->path++;
		if(group_near_camera(&e->groups[SPDL_BODY], 10.0f))
		{
			hurt_player(attack_target(e), SPDL_BITE, 4, 0.0f);
			e->ai_state = SPDL_DONE;
			e->counter = SPDL_DONE_WAIT;
		}
		if(e->path > e->unk92)
		{
			e->ai_state = SPDL_DONE;
			e->counter = SPDL_DONE_WAIT;
		}
		break;
	case SPDL_LEAP:
		if(++e->frame >= SPDL_FRAMES)
		{
			e->frame = 0x1c;
		}
		path_place(e, spdl_path, e->path, 1.0f);
		if(++e->path > e->unk92)
		{
			if(group_near_camera(&e->groups[SPDL_BODY], 20.0f))
			{
				hurt_player(attack_target(e), SPDL_BITE, 4, 0.0f);
			}
			e->ai_state = SPDL_DONE;
			e->counter = SPDL_DONE_WAIT;
		}
		break;
	case SPDL_DONE:
		if(--e->counter == 0)
		{
			remove_die(obj->id, 0);
		}
		break;
	}
}

/* 0x8010725c: the spawn table's state; the path states start where the entry says */
void func_8010725c(enemy_t *e, int state)
{
	e->ai_state = state;
	switch(state)
	{
	case SPDL_LOOP:
	case SPDL_RUN:
		e->frame = 0;
		path_place(e, spdl_path, e->path, 1.0f);
		break;
	case SPDL_LEAP:
		e->frame = 0x1c;
		path_place(e, spdl_path, e->path, 1.0f);
		break;
	default:
		e->frame = 0;
		break;
	}
}
