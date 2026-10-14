/*
 * select.c - the level-select gallery's targets: one original file (0x80101908-0x801026b0,
 * its tables at 0x801917e0-0x80191d78, select_data.c). Each level still to be played
 * stands in the gallery as an animated model (the haunted house, the Freak Show head, the
 * Rickety Town car; the big top once the other three are cleared) with a tick sprite over
 * it; shooting one picks that level. The players' aims light the gallery wall: aim_select_light
 * puts a green light where the gun's ray meets the wall plane and the targets' draw adds it
 * for its frame. The record is an enemy's (enemy.h), so draw_model and the shot tests apply.
 */
#include <stdlib.h>
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "texmap.h"
#include "credits.h"			/* player_stats_t, debug_mode */
#include "lights.h"
#include "levelsnd.h"
#include "waves.h"
#include "camera.h"			/* gun_t */

extern int	shot_hits_sphere(float *, float, player_stats_t *);	/* objects.c */
extern void	clear_enemy_fields(enemy_t *);				/* objects.c */
extern void	screen_to_point(float *, float, float, float), view_to_world(float *);	/* vecmat.c */
extern void	draw_point_sprite_half(int, float *, float, float, unsigned int);	/* draw2d.c */
extern int	load_model(void **, int, void **, int, void **, int, char *, int);
extern void	remove_model(void **, int, void **, int, void **, int, char *);
extern void	remove_animation(submesh_t *, int, int, char *);
extern enemy_t	*debug_enemy;
extern int	next_wave;						/* main.c */

/* select_data.c */
extern texmap_t	select_hhouse_texmap[3], select_freakshow_texmap[4], select_rickity_texmap[14], select_bigtop_texmap[1], select_tick_texmap[3];
extern submesh_t hhouse_select_submesh[3], freakshow_select_submesh[4], rickity_select_submesh[11], bigtop_select_submesh[1];
extern float	select_light_colour[2][3];				/* 0x80191d60: green for both players */

/* the models */
float	*hhouse_select_verts, *hhouse_select_normals;		/* 0x801e5b78, 0x801e582c */
face_t	*hhouse_select_faces;					/* 0x8021f08c */
float	*freakshow_select_verts, *freakshow_select_normals;	/* 0x801e592c, 0x801e178c */
face_t	*freakshow_select_faces;				/* 0x80212e6c */
float	*rickity_select_verts, *rickity_select_normals;		/* 0x801e5934, 0x801e4390 */
face_t	*rickity_select_faces;					/* 0x80212e70 */
float	*bigtop_select_verts, *bigtop_select_normals;		/* 0x801e5b90, 0x801e5854 */
face_t	*bigtop_select_faces;					/* 0x8021f0d0 */

/* the aim lights */
int	select_light_p1_on;					/* 0x8021efe4 */
int	select_light_p2_on;					/* 0x8021f074 */
float	select_light_p1_pos[3];					/* 0x801de8b8 */
float	select_light_p2_pos[3];					/* 0x801de8c8 */
int	D_801e5dd0;						/* cleared with the lights, read by nothing */

/* the object types */
#define SELECT_HHOUSE		0x710
#define SELECT_FREAKSHOW	0x711
#define SELECT_RICKITY		0x712
#define SELECT_BIGTOP		0x713

#define TICK_HHOUSE		0x1300			/* the tick sprites' keys (select_tick_texmap) */
#define TICK_FREAKSHOW		0x1310
#define TICK_RICKITY		0x1320

#define WALL_Z			-384.0f			/* the gallery wall the aim lights sit on */

static int	select_target_hit(game_object_t *, player_stats_t *);
static int	select_target_die(game_object_t *, player_stats_t *);
static void	remove_select_target(game_object_t *);

/* 0x80101908: draw a target: the players' aim lights for this frame, the model and its tick
   (0.8 above and a unit in front of the record) */
void draw_select_target(game_object_t *o)
{
	enemy_t	*e;
	float	p[3];
	int	light1, light2;

	e = (enemy_t *)o->data;
	light1 = 0;
	light2 = 0;
	if(select_light_p1_on)
	{
		light1 = add_dynamic_light(1, select_light_p1_pos, select_light_colour[0], 0.8f, 1);
	}
	if(select_light_p2_on)
	{
		light2 = add_dynamic_light(1, select_light_p2_pos, select_light_colour[1], 0.8f, 1);
	}
	p[0] = e->x;
	p[1] = e->y + 0.8f;
	p[2] = e->z - 1.0f;
	switch(o->type)
	{
	case SELECT_HHOUSE:
		draw_model(e, hhouse_select_submesh, 3, hhouse_select_verts, 0x7e, hhouse_select_faces, 0xb8, hhouse_select_normals, 0x41);
		draw_point_sprite_half(TICK_HHOUSE, p, 2.0f, 255.0f, 0xffffffff);
		break;
	case SELECT_FREAKSHOW:
		draw_model(e, freakshow_select_submesh, 4, freakshow_select_verts, 0x88, freakshow_select_faces, 0xc2, freakshow_select_normals, 0x46);
		draw_point_sprite_half(TICK_FREAKSHOW, p, 2.0f, 255.0f, 0xffffffff);
		break;
	case SELECT_RICKITY:
		draw_model(e, rickity_select_submesh, 0xb, rickity_select_verts, 0x134, rickity_select_faces, 0x1a7, rickity_select_normals, 0x113);
		draw_point_sprite_half(TICK_RICKITY, p, 2.0f, 255.0f, 0xffffffff);
		break;
	case SELECT_BIGTOP:
		draw_model(e, bigtop_select_submesh, 1, bigtop_select_verts, 0x28, bigtop_select_faces, 0x3a, bigtop_select_normals, 0x15);
		break;
	}
	if(select_light_p1_on)
	{
		remove_light(light1);
	}
	if(select_light_p2_on)
	{
		remove_light(light2);
	}
}

/* 0x80101bc0: a shot against the target's hit group or a 2.2 sphere around its tick picks
   its level (the big top target picks nothing: the countdown does); in the character
   debug mode it makes the target the debugged enemy instead */
static int select_target_hit(game_object_t *o, player_stats_t *p)
{
	enemy_t	*e;
	float	tick[3];
	int	wave;

	e = (enemy_t *)o->data;
	tick[0] = e->x;
	tick[1] = e->y + 0.8f;
	tick[2] = e->z - 1.0f;
	if(shot_hits_sphere(e->hit_group->pivot, e->radius * e->radius, p) || shot_hits_sphere(tick, 4.84f, p))
	{
		if(debug_mode & DEBUG_CHARACTERS)
		{
			debug_enemy = e;
			return 1;
		}
		switch(o->type)
		{
		case SELECT_HHOUSE:
			wave = WAVE_HHOUSE;
			break;
		case SELECT_FREAKSHOW:
			wave = WAVE_FREAKSHOW;
			break;
		case SELECT_RICKITY:
			wave = WAVE_RICKITY;
			break;
		default:
			return 1;
		}
		next_wave = wave;
		return 1;
	}
	return 0;
}

/* 0x80101cd8 */
static int select_target_die(game_object_t *o, player_stats_t *p)
{
	return 0;
}

/* 0x80101ce0: the gallery's texmaps, models and animations (the "rick" and "frek" labels are
   the original's: it logs the Freak Show head's texmaps under "rick" and the car's under
   "frek") */
void load_gallery_objects(void)
{
	load_texmap(select_tick_texmap, 3, "attract");
	load_texmap(select_hhouse_texmap, 3, "hhou");
	load_texmap(select_freakshow_texmap, 4, "rick");
	load_texmap(select_rickity_texmap, 14, "frek");
	load_texmap(select_bigtop_texmap, 1, "btop");
	load_model((void **)&hhouse_select_verts, 0x7e, (void **)&hhouse_select_faces, 0xb8, (void **)&hhouse_select_normals, 0x41, "HHOU.ZM", (int)0xafb0e9bd);
	load_anim(hhouse_select_submesh, 3, 0x28, "hhou.za", (int)0x2ad31695);
	load_model((void **)&freakshow_select_verts, 0x88, (void **)&freakshow_select_faces, 0xc2, (void **)&freakshow_select_normals, 0x46, "FREK.ZM", (int)0xa3da51c3);
	load_anim(freakshow_select_submesh, 4, 0x14, "frek.za", (int)0xe62bbeb0);
	load_model((void **)&rickity_select_verts, 0x134, (void **)&rickity_select_faces, 0x1a7, (void **)&rickity_select_normals, 0x113, "RICK.ZM", (int)0x352bfb5a);
	load_anim(rickity_select_submesh, 0xb, 0x37, "rick.za", (int)0xa62ba1ae);
	load_model((void **)&bigtop_select_verts, 0x28, (void **)&bigtop_select_faces, 0x3a, (void **)&bigtop_select_normals, 0x15, "BTOP.ZM", (int)0x4acefd35);
}

/* 0x80101f08 */
void remove_gallery_objects(void)
{
	remove_model((void **)&bigtop_select_verts, 0x28, (void **)&bigtop_select_faces, 0x3a, (void **)&bigtop_select_normals, 0x15, "BTOP.ZM");
	remove_animation(rickity_select_submesh, 0xb, 0x37, "rick.za");
	remove_model((void **)&rickity_select_verts, 0x134, (void **)&rickity_select_faces, 0x1a7, (void **)&rickity_select_normals, 0x113, "RICK.ZM");
	remove_animation(freakshow_select_submesh, 4, 0x14, "frek.za");
	remove_model((void **)&freakshow_select_verts, 0x88, (void **)&freakshow_select_faces, 0xc2, (void **)&freakshow_select_normals, 0x46, "FREK.ZM");
	remove_animation(hhouse_select_submesh, 3, 0x28, "hhou.za");
	remove_model((void **)&hhouse_select_verts, 0x7e, (void **)&hhouse_select_faces, 0xb8, (void **)&hhouse_select_normals, 0x41, "HHOU.ZM");
	remove_surface(select_bigtop_texmap, 1);
	remove_surface(select_rickity_texmap, 14);
	remove_surface(select_freakshow_texmap, 4);
	remove_surface(select_hhouse_texmap, 3);
	remove_surface(select_tick_texmap, 3);
}

/* 0x801020b4: a target's record: an enemy facing the camera 10.4 up and 381 back, scaled
   10 (x mirrored) with a 2.7 hit radius, and its groups: the house's roof and ghost half
   transparent, the car with eleven, the big top one. Each type has its own x */
int init_select_target(game_object_t *o)
{
	enemy_t	*e;
	void	*data;
	int	i;

	data = malloc(sizeof(enemy_t));
	o->data = data;
	if(!data)
	{
		return 1;
	}
	o->hit = select_target_hit;
	o->die = select_target_die;
	o->remove = remove_select_target;
	e = (enemy_t *)data;
	e->unk4 = 0x14;
	e->unkcc = 0.27f;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0xc;
	e->rx = 0.0f;
	e->rz = 0.0f;
	e->x = 0.0f;
	e->ry = 3.1415927f;
	e->y = 10.4f;
	e->z = -381.0f;
	e->scale[0] = -10.0f;
	e->scale[1] = 10.0f;
	e->scale[2] = 10.0f;
	e->radius = e->unkcc * 10.0f;
	switch(o->type)
	{
	case SELECT_HHOUSE:
		e->groups = malloc(3 * sizeof(group_t));
		if(!e->groups)
		{
			free(e);
			return 1;
		}
		for(i = 0; i < 3; i++)
		{
			e->groups[i].flags = 0xa000;
		}
		e->groups[0].flags |= 0x200;
		e->groups[2].flags |= 0x100;
		e->groups[2].alpha = 128.0f;
		e->groups[0].flags |= 0x100;
		e->groups[0].alpha = 128.0f;
		init_groups(e, hhouse_select_submesh, 3);
		e->x = -5.6f;
		e->hit_group = &e->groups[1];
		break;
	case SELECT_FREAKSHOW:
		e->groups = malloc(4 * sizeof(group_t));
		if(!e->groups)
		{
			free(e);
			return 1;
		}
		for(i = 0; i < 4; i++)
		{
			e->groups[i].flags = 0xa200;
		}
		init_groups(e, freakshow_select_submesh, 4);
		e->x = 6.7f;
		e->hit_group = &e->groups[3];
		break;
	case SELECT_RICKITY:
		e->groups = malloc(0xb * sizeof(group_t));
		if(!e->groups)
		{
			free(e);
			return 1;
		}
		for(i = 0; i < 0xb; i++)
		{
			e->groups[i].flags = 0xa200;
		}
		init_groups(e, rickity_select_submesh, 0xb);
		e->x = 0.55f;
		e->hit_group = &e->groups[1];
		break;
	case SELECT_BIGTOP:
		e->groups = malloc(sizeof(group_t));
		if(!e->groups)
		{
			free(e);
			return 1;
		}
		for(i = 0; i < 1; i++)
		{
			e->groups[i].flags = 0x8000;
		}
		init_groups(e, bigtop_select_submesh, 1);
		e->x = 0.5f;
		e->y = 3.5f;
		e->z = -378.0f;
		e->hit_group = &e->groups[0];
		break;
	}
	e->hit_group->floor = 0.0f;
	clear_enemy_fields(e);
	e->prio_base = 0x38;
	e->prio_flags = next_cue_priority();
	return 0;
}

/* 0x80102404 */
static void remove_select_target(game_object_t *o)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	if(debug_enemy == e)
	{
		debug_enemy = 0;
	}
	free(e->groups);
	free(e);
}

/* 0x80102454: step the animation (40, 20 and 55 frames; the big top has none and its frame
   limit is whatever the register held) and float the big top up to 12.1 */
void select_target_think(game_object_t *o)
{
	enemy_t	*e;
	int	limit;

	e = (enemy_t *)o->data;
	switch(o->type)
	{
	case SELECT_HHOUSE:
		limit = 0x28;
		break;
	case SELECT_FREAKSHOW:
		limit = 0x14;
		break;
	case SELECT_RICKITY:
		limit = 0x37;
		break;
	case SELECT_BIGTOP:
#ifdef PORT_HOST
		limit = 0x37;		/* the original leaves it uninitialised: the big top has no animation */
#endif
		if(e->y < 12.1f)
		{
			e->y = e->y + 0.15f;
		}
		break;
	default:
		limit = 0x37;
		break;
	}
	e->frame++;
	if(!(e->frame < limit))
	{
		e->frame = 0;
	}
}

/* 0x80102500: a playing player's aim light: where the ray through the gun's screen
   position (from 100 deep to 4 deep) meets the gallery wall at z = -384; off while the gun
   is off the screen or the player is out */
void aim_select_light(player_stats_t *p)
{
	float	at[3], deep[3], near[3];
	float	t;

	if(p->state != 4)
	{
		if(p->index)
		{
			select_light_p2_on = 0;
		}
		else
		{
			select_light_p1_on = 0;
		}
	}
	else if(p->gun->on_screen == 0)
	{
		if(p->index == 0)
		{
			select_light_p1_on = 0;
		}
		else
		{
			select_light_p2_on = 0;
		}
	}
	else
	{
		screen_to_point(deep, (float)p->gun->screen_x, (float)p->gun->screen_y, 100.0f);
		view_to_world(deep);
		screen_to_point(near, (float)p->gun->screen_x, (float)p->gun->screen_y, 4.0f);
		view_to_world(near);
		t = (near[2] - WALL_Z) / (near[2] - deep[2]);
		at[0] = near[0] - (near[0] - deep[0]) * t;
		at[1] = near[1] - (near[1] - deep[1]) * t;
		at[2] = WALL_Z;
		if(p->index == 0)
		{
			select_light_p1_on = 1;
			select_light_p1_pos[0] = at[0];
			select_light_p1_pos[1] = at[1];
			select_light_p1_pos[2] = at[2];
		}
		else
		{
			select_light_p2_on = 1;
			select_light_p2_pos[0] = at[0];
			select_light_p2_pos[1] = at[1];
			select_light_p2_pos[2] = at[2];
		}
	}
}

/* 0x80102690 */
void reset_select_lights(void)
{
	select_light_p1_on = 0;
	select_light_p2_on = 0;
	D_801e5dd0 = 0;
}
