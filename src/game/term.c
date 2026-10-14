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

extern void	func_800fd9ac(enemy_t *);			/* run the attached effects */
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, int, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);		/* objects.c */
extern void	set_enemy_health(enemy_t *, float);		/* health by difficulty */
extern player_stats_t *get_target_player(void);		/* objects.c */
extern player_stats_t *get_player_info_struct2_(enemy_t *);
extern int	randrng(int);
extern void	func_800fd990(enemy_t *, void *);		/* attach an effect */
extern void	*func_800febd0(float, float, group_t *);	/* a blood effect on a group */
extern void	path_place(enemy_t *, void *, short, float);	/* start along a path */
extern void	path_place_upright(enemy_t *, void *, short, float);	/* step along a path */
extern void	func_800fd6b0(enemy_t *);
extern void	func_800fda34(enemy_t *);
extern void	func_800fda88(enemy_t *);
extern void	func_80114b8c(enemy_t *, int);
extern void	func_800fc34c(float *, float *, float *);	/* turn to face a point */
extern int	group_near_camera(group_t *, float);		/* a group within reach of the player */
extern void	decrenent_health_(player_stats_t *, int, int, int);
extern void	func_800ced14(enemy_t *, submesh_t *, int, int, int);	/* blend to a frame */
extern void	func_800cef8c(enemy_t *, int, int, submesh_t *, int);	/* hit reaction */
extern void	func_800fb934(enemy_t *, float *, float *, float, int, float);
extern int	func_800fbc6c(enemy_t *, float);
extern int	func_800cdeac(float *, float, player_stats_t *);	/* the shot hits the sphere */
extern int	do_something_with_submesh(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	func_800fdc18(float *, float, int);
extern void	func_800fdcf4(float *, float);
extern void	func_800db810(float *, int, int);
extern void	func_800ff190(float *, unsigned int);
extern float	func_800e29b0(player_stats_t *);		/* the shot's damage */
extern void	func_800e2958(int, int);			/* award points */
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
	func_800fd9ac(e);
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
	if(func_800cdeac(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = do_something_with_submesh(e, submesh_info_term, 0xd, p);
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
	func_800db810(p->hit_point, 0, 0);
	group = p->hit_group;
	g = &e->groups[group];
	if(p->weapon == 0x1000005)
	{
		for(i = 0; i < 0xd; i++)
		{
			e->groups[i].flags |= 0x20;
			func_800ff190(e->groups[i].pivot, 0xd0ff00);
		}
		e->health = 0.01f;
	}
	if(p->weapon == 0x1000004)
	{
		if(!(g->flags & 2))
		{
			g->flags |= 2;
			func_800fd990(e, func_800febd0(0.0f, (submesh_info_term[group].radius + submesh_info_term[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		e->groups[1].flags |= 0xf0000000;
	}
	if(!(e->ai_state & 0x6000))
	{
		e->health -= func_800e29b0(p);
		if(e->health <= 0.0f)
		{
			func_800e2958(p->index, 500);
			play_enemy_sound((snd_object_t *)e, term_sounds[randrng(3)], "rickity", 2);
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
			func_800e2958(p->index, 50);
			play_enemy_sound((snd_object_t *)e, term_sounds[randrng(3)], "rickity", 2);
		}
	}
	if(e->ai_state & 0xe000)
	{
		return(0);
	}
	if(group == 3)
	{
		func_800cef8c(e, 0x2d, 0x1003, submesh_info_term, 6);
	}
	else if(group < 4 && group == 1)
	{
		func_800cef8c(e, 0x2d, 0x1002, submesh_info_term, 6);
	}
	else
	{
		func_800cef8c(e, 0x2d, 0x1001, submesh_info_term, 6);
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
	obj->unused18 = (int)(long)term_hit;
	obj->unused1c = (int)(long)term_die;
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
	e->groups[6].unk74 = 0;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	term_setup(e, 0);
	play_enemy_sound((snd_object_t *)e, 0x14, "rickity", 0);
	return(0);
}

/* 0x80128cd8: the remove callback */
void term_remove(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(!(e->ai_state & 0x6000))
	{
		play_enemy_sound((snd_object_t *)e, term_sounds[randrng(3)], "rickity", 2);
	}
	if(debug_enemy == e)
	{
		debug_enemy = 0;
	}
	func_800fda88(e);
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
	func_800fda34(e);
	func_80114b8c(e, 0xd);
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
		func_800ced14(e, submesh_info_term, 6, e->resume_frame, 1);
		e->ai_state = e->resume_state;
		break;
	case 1:
		e->frame++;
		if(e->frame >= 0x17)
		{
			e->frame = 0xf;
		}
		func_800fc34c(&camera_x, &e->x, &e->rx);
		term_steer(e);
		break;
	case 0x201:
		func_800fc34c(&camera_x, &e->x, &e->rx);
		e->frame++;
		if(e->frame == 0xc)
		{
			if(group_near_camera(&e->groups[4], 13.0f))
			{
				decrenent_health_(get_player_info_struct2_(e), 0xaa, 3, 0);
			}
		}
		if(e->frame < 0xf)
		{
			break;
		}
		term_steer(e);
		break;
	case 3:
		func_800fc34c(&camera_x, &e->x, &e->rx);
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
			decrenent_health_(get_player_info_struct2_(e), 0xaa, 3, 0);
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
	func_800fb934(e, &camera_x, &e->unk70, 2.0f, 0, -8.0f);
	r = func_800fbc6c(e, 1.8f);
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
		func_800ced14(e, submesh_info_term, 6, 0x3c, 1);
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
		func_800ced14(e, submesh_info_term, 6, 0, 1);
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
		func_800ced14(e, submesh_info_term, 6, 0xf, 1);
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
		func_800ced14(e, submesh_info_term, 6, 0xf, 2);
		e->ai_state = 2;
	}
	else
	{
		e->unk6 = 0;
		term_steer(e);
	}
}
