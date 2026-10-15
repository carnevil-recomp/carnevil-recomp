/*
 * bat.c - the Haunted House bat (object type 0x2005, assets BAT.ZM / bat.za / bat.pth and the
 * bat*.WMS texmaps). One original file, 0x800f38c8-0x800f4614, tables at 0x8018f138 (texmaps)
 * and 0x8018f478 (submeshes, bat_data.c), rodata 0x80171d88-0x80171dec. The model has 340
 * vertices, 420 normals and 480 faces in 15 submeshes, the animation 25 frames (0-8 the wing
 * beat, 9-24 the fall), the path 1949 points.
 *
 * A bat is placed by its groups' animation every frame (pose_groups, model.c) rather than
 * by init_groups. It flies its path (0x180 then attacks, 0x181 then leaves, 0x182 round and
 * round), turns to the camera (0x2000), swoops at the player (0x100: it steers for a point by
 * the camera and, once there, bites: 0x102), and dies by falling (0x400) when one shot of
 * anything takes its 0.75 health; it is removed when the fall or its path ends. The set-up
 * also has a hover (0x600) and a fly-in from 100 units to the side (0x110).
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

extern char	text_buffer[];

extern texmap_t	bat_texmap[26];			/* bat_data.c */
extern submesh_t submesh_info_bat[15];

extern void	draw_effects(enemy_t *);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);	/* text at a world position */
extern void	func_800c8938(float *, float, unsigned int);			/* a debug sphere */
extern void	func_800d14a4(float, float);					/* a debug map dot */
extern void	func_800c99e4(enemy_t *, int);
extern void	pose_groups(enemy_t *, submesh_t *, int);			/* model.c: place the animated groups */
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern int	randrng(int);
extern void	path_place(enemy_t *, void *, short, float);
extern int	face_camera(enemy_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	point_toward_camera(float *, float, float *);			/* objects.c */
extern void	face_point(enemy_t *, float *, float);
extern void	func_800cec74(enemy_t *, float *, float, float *);
extern int	func_800ce428(enemy_t *, int, float *, float);
extern void	func_800ce680(enemy_t *);					/* moves from vel towards goal by counter / blend_len */
extern void	rot_matrix_yxz(float *, float *);
extern void	mat_vec_mul(float *, float *, float *);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern int	remove_die();						/* spawn.c: (id, killer) */
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern int	printf(const char *, ...);

float	*hhbat_verts;			/* 0x801e09d4 */
face_t	*hhbat_faces;			/* 0x801e6eb8 */
float	*hhbat_normals;			/* 0x801de950 */
void	*hhbat_path;			/* 0x801e70a4 */

#define BAT_GROUPS	0xf
#define BAT_NVERTS	0x154
#define BAT_NFACES	0x1e0
#define BAT_NNORMALS	0x1a4
#define BAT_FRAMES	0x19
#define BAT_PATH_LEN	0x79d
#define BAT_FLAP_END	9		/* frames 0-8 beat the wings */
#define BAT_FALL_END	0x19		/* frames 9-24 fall */

/* the states */
#define BAT_SWOOP	0x100		/* flies at a point by the camera */
#define BAT_BITE	0x102
#define BAT_FLY_IN	0x110		/* set-up only: comes in from 100 units to its side */
#define BAT_PATH	0x180		/* flies its path, then swoops */
#define BAT_PATH_OUT	0x181		/* flies its path, then is gone */
#define BAT_PATH_LOOP	0x182		/* flies its path round and round */
#define BAT_FALL	0x400		/* shot or done biting: falls away */
#define BAT_HOVER	0x600
#define BAT_TURN	0x2000		/* turns to the camera, then swoops */

int	bat_die(game_object_t *, player_stats_t *);
void	bat_remove(game_object_t *);
void	bat_setup(enemy_t *, short);

/* 0x800f38c8: the draw callback (type 0x2005): the groups are placed from the animation
   first */
void bat_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	pose_groups(e, submesh_info_bat, BAT_GROUPS);
	draw_model(e, submesh_info_bat, BAT_GROUPS, hhbat_verts, BAT_NVERTS, hhbat_faces, BAT_NFACES, hhbat_normals, BAT_NNORMALS);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->hit_group->pos, e->radius, 0x600000);
		func_800c8938(&e->x, 0.1f, 0xffffff);
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
		func_800c99e4(e, 0xffff);
	}
}

/* 0x800f3a84: the hit test: the bounding sphere, then the submesh; a hit marks the group and
   takes 2 health at once */
int bat_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		if((unsigned int)p->unk10 != 0xffff0000)
		{
			p->unk10 = 0xffffff00;
		}
		group = find_submesh_hit(e, submesh_info_bat, BAT_GROUPS, p);
		if(group != -1)
		{
			g = &e->groups[group];
			g->flags |= 0x8000;
			e->health -= 2.0f;
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
			p->hit_group = group;
			p->die = (void *)bat_die;
		}
	}
	return(0);
}

/* 0x800f3b90: the shot landing with a given damage (nothing in the image calls it; the die
   callback below works out the damage itself): a squeak; when the health runs out, blood and
   a splat, 250 points and the fall, else 50 points */
void bat_damage(void *unused, player_stats_t *p, float damage, enemy_t *e)
{
	float	y;

	y = e->y - 20.0f;
	if(e->ai_state != BAT_FALL)
	{
		play_enemy_sound(e, 0x1f, "hhouse1", 8);
		e->health -= damage;
		if(e->health <= 0.0f)
		{
			burst_blood(p->hit_point, y, 0x800000);
			add_world_sprite(p->hit_point, 0, 0);
			e->ai_state = BAT_FALL;
			e->killer = p;
			award_points(p->index, 250);
			e->frame = BAT_FLAP_END;
			e->counter = 0;
			e->goal[0] = 0.0f;
			e->goal[2] = 0.0f;
			e->goal[1] = 1.0f;
		}
		else
		{
			award_points(p->index, 50);
		}
	}
}

/* 0x800f3c98: the die callback: as above with the shot's own damage */
int bat_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	float	damage;
	float	y;

	e = (enemy_t *)obj->data;
	damage = shot_damage_of(p);
	y = e->y - 20.0f;
	if(e->ai_state != BAT_FALL)
	{
		play_enemy_sound(e, 0x1f, "hhouse1", 8);
		e->health -= damage;
		if(e->health <= 0.0f)
		{
			burst_blood(p->hit_point, y, 0x800000);
			add_world_sprite(p->hit_point, 0, 0);
			e->ai_state = BAT_FALL;
			e->killer = p;
			award_points(p->index, 250);
			e->frame = BAT_FLAP_END;
			e->counter = 0;
			e->goal[0] = 0.0f;
			e->goal[2] = 0.0f;
			e->goal[1] = 1.0f;
		}
		else
		{
			award_points(p->index, 50);
		}
	}
	return(0);
}

/* 0x800f3dac */
void load_bat_texmap(void)
{
	load_texmap(bat_texmap, 0x1a, "bat");
}

/* 0x800f3ddc: the model, the animation and the path */
void load_bat(void)
{
	load_model((void **)&hhbat_verts, BAT_NVERTS, (void **)&hhbat_faces, BAT_NFACES, (void **)&hhbat_normals, BAT_NNORMALS, "BAT.ZM", 0x069b7af0);
	load_anim(submesh_info_bat, BAT_GROUPS, BAT_FRAMES, "bat.za", 0xc1e0d3c9);
	load_path(&hhbat_path, BAT_PATH_LEN, "bat.pth", 0x3644251e);
}

/* 0x800f3e84 */
void remove_bat_assets(void)
{
	remove_surface(bat_texmap, 0x1a);
	remove_path(&hhbat_path, BAT_PATH_LEN, "bat.pth");
	remove_animation(submesh_info_bat, BAT_GROUPS, BAT_FRAMES, "bat.za");
	remove_model((void **)&hhbat_verts, BAT_NVERTS, (void **)&hhbat_faces, BAT_NFACES, (void **)&hhbat_normals, BAT_NNORMALS, "BAT.ZM");
}

/* 0x800f3f1c: the init callback (type 0x2005): the record, 15 groups, 0.75 health by
   difficulty, three times the model's size, on its path */
int bat_init(game_object_t *obj)
{
	enemy_t	*e;
	group_t	*g;
	int	i;

	obj->data = malloc(sizeof(enemy_t));
	if(!obj->data)
	{
		return(1);
	}
	obj->hit = bat_hit;
	obj->die = bat_die;
	obj->remove = bat_remove;
	e = (enemy_t *)obj->data;
	e->counter = 0;
	e->num = 0;
	e->flags = 0xc;
	e->rx = 0;
	e->ry = 0;
	e->rz = 0;
	e->target = get_target_player();
	set_enemy_health(e, 0.75f);
	e->x = 0;
	e->y = 0;
	e->z = 0;
	e->unkcc = 0.25f;
	set_enemy_scale(e, 3.0f);
	e->goal[0] = e->goal[1] = e->goal[2] = 0.0f;
	e->groups = malloc(BAT_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < BAT_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	g = e->groups;
	e->hit_group = &g[4];
	g[4].floor = 0.0f;
	e->unk1a = 0x2005;
	e->unke0 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	bat_setup(e, BAT_PATH);
	pose_groups(e, submesh_info_bat, BAT_GROUPS);
	return(0);
}

/* 0x800f4084: the remove callback: whatever held the bat (its unke0) lets go */
void bat_remove(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(e->unke0)
	{
		((enemy_t *)e->unke0)->unke4 = 0;
	}
	free(e->groups);
	free(e);
}

/* 0x800f40c8: the think callback */
void bat_think(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	switch(e->ai_state)
	{
	case BAT_HOVER:
		if(++e->frame >= BAT_FLAP_END)
		{
			e->frame = 0;
		}
		break;
	case BAT_FALL:
		if(++e->frame >= BAT_FALL_END)
		{
			e->frame--;
		}
		e->x += e->goal[0];
		e->y += e->goal[1];
		e->z += e->goal[2];
		e->goal[1] += -0.22f;
		if(++e->counter > 20)
		{
			if(!(debug_mode & 1))
			{
				remove_die(obj->id, e->killer);
			}
			else
			{
				remove_game_object(obj->id);
			}
		}
		break;
	case BAT_TURN:
		if(++e->frame >= BAT_FLAP_END)
		{
			e->frame = 0;
		}
		if(!face_camera(e, 0.1f))
		{
			e->ai_state = BAT_SWOOP;
			e->target = get_target_player();
		}
		break;
	case BAT_PATH:
		if(++e->frame >= BAT_FLAP_END)
		{
			e->frame = 0;
		}
		path_place(e, hhbat_path, e->path, 1.0f);
		if(++e->path > e->unk92)
		{
			e->ai_state = BAT_SWOOP;
		}
		break;
	case BAT_PATH_OUT:
		if(++e->frame >= BAT_FLAP_END)
		{
			e->frame = 0;
		}
		path_place(e, hhbat_path, e->path, 1.0f);
		if(++e->path > e->unk92)
		{
			if(!(debug_mode & 1))
			{
				remove_die(obj->id, e->killer);
			}
			else
			{
				remove_game_object(obj->id);
			}
		}
		break;
	case BAT_PATH_LOOP:
		if(++e->frame >= BAT_FLAP_END)
		{
			e->frame = 0;
		}
		path_place(e, hhbat_path, e->path, 1.0f);
		if(++e->path > e->unk92)
		{
			e->path = e->unk90;
		}
		break;
	case BAT_SWOOP:
		if(++e->frame >= BAT_FLAP_END)
		{
			e->frame = 0;
		}
		face_point(e, e->goal, 0.2f);
		func_800cec74(e, &camera_x, -3.5f, e->goal);
		if(func_800ce428(e, 4, e->goal, 4.0f))
		{
			hurt_player(e->target, 0x96, 4, 0);
			point_toward_camera(&camera_x, -12.0f, e->goal);
			e->ai_state = BAT_BITE;
			e->counter = 0;
		}
		break;
	case BAT_BITE:
		if(++e->frame >= BAT_FLAP_END)
		{
			e->frame = 0;
			hurt_player(e->target, 0x1e, 4, 0);
			e->ai_state = BAT_FALL;
		}
		break;
	}
}

/* 0x800f44d8: the set-up the spawn table asks for (type 0x2005) */
void bat_setup(enemy_t *e, short state)
{
	float	off[3];
	float	m[9];

	switch(state)
	{
	case BAT_HOVER:
		e->frame = 0;
		e->ai_state = state;
		break;
	case BAT_FLY_IN:
		e->frame = 0;
		e->ai_state = state;
		off[0] = 100.0f;
		off[1] = 0;
		off[2] = 0;
		e->blend_len = 0x14;
		rot_matrix_yxz(m, &e->rx);
		mat_vec_mul(off, m, off);
		e->counter = 0;
		e->vel[0] = e->x + off[0];
		e->vel[1] = e->y + off[1];
		e->vel[2] = e->z + off[2];
		e->goal[0] = e->x;
		e->goal[1] = e->y;
		e->goal[2] = e->z;
		func_800ce680(e);
		break;
	case BAT_PATH:
	case BAT_PATH_OUT:
	case BAT_PATH_LOOP:
		e->frame = randrng(6);
		e->ai_state = state;
		break;
	default:
		printf("Bad bat state\n");
		break;
	}
}
