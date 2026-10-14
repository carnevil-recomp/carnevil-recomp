/*
 * term.c - the Rickety Town flying bug (object type 0x2042, assets TERM.ZM / term.za /
 * term.pth and the TERM*.WMS texmaps; Binary Ninja's guess for the name is "Terminite").
 * One original file, 0x80128398-0x801293b8, tables at 0x8019b0d8 (texmaps) and 0x8019b338
 * (submeshes). The model has 377 vertices, 408 normals and 636 faces in 13 submeshes, the
 * animation 75 frames, the path 1305 points. It follows its path (state 0x801) until the
 * path ends, then flies at the player (1), hovers (3) and strikes (0x201); a hit knocks it
 * into 0x1001-0x1003, a kill into 0x2001/0x2002 and it then drops out of view (0x4000).
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

extern void	draw_effects(enemy_t *);			/* run the attached effects */
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, int, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);		/* objects.c */
extern void	set_enemy_health(enemy_t *, float);		/* health by difficulty */
extern player_stats_t *get_target_player(void);		/* objects.c */
extern player_stats_t *attack_target(enemy_t *);		/* objects.c */
extern int	randrng(int);
extern void	attach_effect(enemy_t *, void *);
extern void	*func_800febd0(float, float, group_t *);	/* a blood effect on a group */
extern void	path_place(enemy_t *, void *, short, float);	/* start along a path */
extern void	path_place_upright(enemy_t *, void *, short, float);	/* step along a path */
extern void	func_800fd6b0(enemy_t *);
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern void	angles_to(float *, float *, float *);	/* turn to face a point */
extern int	group_near_camera(group_t *, float);		/* a group within reach of the player */
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);	/* blend to a frame */
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);	/* hit reaction */
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal(enemy_t *, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);	/* the shot hits the sphere */
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	func_800fdc18(float *, float, int);
extern void	func_800fdcf4(float *, float);
extern void	add_world_sprite(float *, int, int);	/* sprites.c */
extern void	func_800ff190(float *, unsigned int);
extern float	shot_damage_of(player_stats_t *);		/* the shot's damage */
extern void	award_points(int, int);			/* award points */
extern int	mark_die();					/* spawn.c; the original passes the player too */
extern int	remove_die();
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern enemy_t	*debug_enemy;
extern char	text_buffer[];
extern int	violence_level;

float	*term_verts;			/* 0x801e5ba8 */
face_t	*term_faces;			/* 0x8021f0f0 */
float	*term_normals;			/* 0x801e586c */
void	*term_path;			/* 0x802373f4 */
int	term_sounds[3] = { 0x15, 0x16, 0x17 };	/* 0x8019b4d8 */

extern texmap_t	term_texmap[19];		/* terminite_data.c */
extern submesh_t submesh_info_term[13];
int	term_die(game_object_t *, player_stats_t *);
void	term_remove(game_object_t *);
void	term_steer(enemy_t *);
void	term_end_path(enemy_t *);

/* 0x80128398: the draw callback */
void term_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_term, 0xd, term_verts, 0x179, term_faces, 0x27c, term_normals, 0x198);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, (int)e->hit_group->pos[0] + 8, 0xffffff);
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
			draw_string_at_color(0, text_buffer, (int)e->hit_group->pos[0] + 8, 0xff0000);
		}
		else
		{
			draw_string_at_color(0, text_buffer, (int)e->hit_group->pos[0] + 8, 0xffffff);
		}
	}
	if(debug_mode & 0x1000)
	{
		func_800c99e4(e, 0xff);
	}
}

/* 0x80128594: the hit test: when the shot lands inside the bounding sphere and on a
   submesh, the player's shot record takes the hit and the die callback */
int term_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_term, 0xd, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_group = group;
			p->die = (void *)term_die;
			p->hit_id = obj->id;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x80128650: the shot lands: blood, damage, points, death or a hit reaction */
int term_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;
	int	group;
	int	i;

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return(0);
	}
	func_800fdc18(p->hit_point, e->unk98, 0x800000);
	func_800fdcf4(e->hit_group->pos, e->unk98);
	add_world_sprite(p->hit_point, 0, 0);
	group = p->hit_group;
	g = &e->groups[group];
	if(p->shot_weapon == 0x1000005)
	{
		for(i = 0; i < 0xd; i++)
		{
			e->groups[i].flags |= 0x20;
			func_800ff190(e->groups[i].pivot, 0xd0ff00);
		}
		e->health = 0.01f;
	}
	if(p->shot_weapon == 0x1000004)
	{
		if(!(g->flags & 2))
		{
			g->flags |= 2;
			attach_effect(e, func_800febd0(0.0f, (submesh_info_term[group].radius + submesh_info_term[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		e->groups[1].flags |= 0xf0000000;
	}
	if(!(e->ai_state & 0x6000))
	{
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			award_points(p->index, 500);
			play_enemy_sound(e, term_sounds[randrng(3)], "rickity", 2);
			e->killer = p;
			mark_die(obj->id, p);
			if(randrng(100) < 25)
			{
				e->ai_state = 0x2001;
			}
			else
			{
				e->ai_state = 0x2002;
			}
			e->frame = 0x17;
			e->flags &= 0xfffd;
		}
		else
		{
			award_points(p->index, 50);
			play_enemy_sound(e, term_sounds[randrng(3)], "rickity", 2);
		}
	}
	if(e->ai_state & 0xe000)
	{
		return(0);
	}
	if(group == 3)
	{
		start_hit_reaction(e, 0x2d, 0x1003, submesh_info_term, 6);
	}
	else if(group < 4 && group == 1)
	{
		start_hit_reaction(e, 0x2d, 0x1002, submesh_info_term, 6);
	}
	else
	{
		start_hit_reaction(e, 0x2d, 0x1001, submesh_info_term, 6);
	}
	return(0);
}

/* 0x8012896c, 0x8012899c: the texmaps (logged under "teen"), the model, the animation and
   the path */
void load_term_texmap(void)
{
	load_texmap(term_texmap, 0x13, "teen");
}

void load_term(void)
{
	load_model((void **)&term_verts, 0x179, (void **)&term_faces, 0x27c, (void **)&term_normals, 0x198, "TERM.ZM", 0x2cc7f653);
	load_anim(submesh_info_term, 0xd, 0x4b, "term.za", 0xba93a92d);
	load_path(&term_path, 0x519, "term.pth", 0xf6a3d9c6);
}

/* 0x80128a44 */
void remove_term_assets(void)
{
	remove_surface(term_texmap, 0x13);
	remove_path(&term_path, 0x519, "term.pth");
	remove_animation(submesh_info_term, 0xd, 0x4b, "term.za");
	remove_model((void **)&term_verts, 0x179, (void **)&term_faces, 0x27c, (void **)&term_normals, 0x198, "TERM.ZM");
}

/* 0x80129120: the state set-up: the path states start along the path */
void term_setup(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	switch(state)
	{
	case 1:
		e->frame = 0xf;
		break;
	case 0x201:
		e->frame = 0;
		break;
	case 0x801:
		e->frame = 0xf;
		path_place(e, term_path, e->path, 1.0f);
		break;
	default:
		e->frame = 0;
		break;
	}
}

/* 0x80128adc: the init callback: the record, the groups (9 and 10 translucent), the first
   state, a greeting */
int term_init(game_object_t *obj)
{
	enemy_t	*e;
	int	i;

	e = malloc(sizeof(enemy_t));
	obj->data = (short *)e;
	if(!e)
	{
		return(1);
	}
	obj->hit = term_hit;
	obj->die = term_die;
	obj->remove = term_remove;
	e->unk4 = 0x14;
	e->unk2 = 0;
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
	e->x = 8.0f;
	e->unkcc = 0.52f;
	set_enemy_scale(e, 0.4f);
	set_enemy_health(e, 0.75f);
	e->groups = malloc(0xd * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < 0xd; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[10].flags |= 8;
	e->groups[10].flags |= 0x80;
	e->groups[10].alpha = 228.0f;
	e->groups[9].flags |= 8;
	e->groups[9].flags |= 0x80;
	e->groups[9].alpha = 228.0f;
	e->unka8 = 0;
	e->unkac = 0;
	init_groups(e, submesh_info_term, 0xd);
	e->hit_group = &e->groups[6];
	e->groups[6].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	term_setup(e, 0);
	play_enemy_sound(e, 0x14, "rickity", 0);
	return(0);
}

/* 0x80128cd8: the remove callback */
void term_remove(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(!(e->ai_state & 0x6000))
	{
		play_enemy_sound(e, term_sounds[randrng(3)], "rickity", 2);
	}
	if(debug_enemy == e)
	{
		debug_enemy = 0;
	}
	free_effects(e);
	free(e->groups);
	free(e);
}

/* 0x80128d6c: the think callback */
void term_think(game_object_t *obj)
{
	enemy_t	*e;
	short	frame;

	if(debug_mode & 0x10000)
	{
		return;
	}
	if(debug_mode & 0x20000)
	{
		return;
	}
	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, 0xd);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1001:
	case 0x1002:
	case 0x1003:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x3c)
		{
			break;
		}
		e->frame = frame;
		blend_to_frame(e, submesh_info_term, 6, e->resume_frame, 1);
		e->ai_state = e->resume_state;
		break;
	case 1:
		e->frame++;
		if(e->frame >= 0x17)
		{
			e->frame = 0xf;
		}
		angles_to(&camera_x, &e->x, &e->rx);
		term_steer(e);
		break;
	case 0x201:
		angles_to(&camera_x, &e->x, &e->rx);
		e->frame++;
		if(e->frame == 0xc)
		{
			if(group_near_camera(&e->groups[4], 13.0f))
			{
				hurt_player(attack_target(e), 0xaa, 3, 0.0f);
			}
		}
		if(e->frame < 0xf)
		{
			break;
		}
		term_steer(e);
		break;
	case 3:
		angles_to(&camera_x, &e->x, &e->rx);
		e->frame++;
		if(e->frame < 0x4b)
		{
			break;
		}
		term_steer(e);
		break;
	case 2:
		e->frame++;
		if(e->frame >= 0x17)
		{
			e->frame = 0xf;
		}
		break;
	case 0x2001:
	case 0x2002:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x2d)
		{
			break;
		}
		e->frame = frame;
		e->ai_state = 0x4000;
		break;
	case 0x4000:
		e->y += -0.88f;
		break;
	case 0x801:
		e->frame++;
		if(e->frame >= 0x17)
		{
			e->frame = 0xf;
		}
		if(group_near_camera(&e->groups[4], 8.0f))
		{
			hurt_player(attack_target(e), 0xaa, 3, 0.0f);
			remove_die(obj->id, 0);
		}
		path_place_upright(e, term_path, e->path, 1.0f);
		e->path++;
		if(e->unk92 < e->path)
		{
			term_end_path(e);
			func_800fd6b0(e);
		}
		break;
	case 0:
		e->frame++;
		if(e->frame >= 0x4b)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x8012919c: off the path: keep a playing target, steer at the camera and pick the next
   state from the steering result (1 hover, 2 strike, 0 fly) */
void term_steer(enemy_t *e)
{
	player_stats_t *p;
	int	r;
	short	frame;

	if(e->target->state != 4)
	{
		p = get_target_player();
		if(p->state == 4)
		{
			e->target = p;
		}
	}
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0, -8.0f);
	r = step_toward_goal(e, 1.8f);
	switch(r)
	{
	case 1:
		if(e->ai_state == 3)
		{
			e->frame = 0x3c;
			break;
		}
		frame = e->frame;
		if(frame > 0)
		{
			e->frame = frame - 1;
		}
		blend_to_frame(e, submesh_info_term, 6, 0x3c, 1);
		e->ai_state = 3;
		break;
	case 2:
		if(e->ai_state & 0x200)
		{
			e->frame = 0;
			break;
		}
		frame = e->frame;
		if(frame > 0)
		{
			e->frame = frame - 1;
		}
		blend_to_frame(e, submesh_info_term, 6, 0, 1);
		e->ai_state = 0x201;
		break;
	case 0:
		if(e->ai_state == 1)
		{
			break;
		}
		frame = e->frame;
		if(frame > 0)
		{
			e->frame = frame - 1;
		}
		blend_to_frame(e, submesh_info_term, 6, 0xf, 1);
		e->ai_state = 1;
		break;
	}
}

/* 0x80129348: the path ran out: a spawn entry marked 2 hovers in place, the rest hunt */
void term_end_path(enemy_t *e)
{
	if(e->unk6 == 2)
	{
		e->unk6 = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_term, 6, 0xf, 2);
		e->ai_state = 2;
	}
	else
	{
		e->unk6 = 0;
		term_steer(e);
	}
}
