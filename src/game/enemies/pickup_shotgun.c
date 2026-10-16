/*
 * pickup_shotgun.c - the shotgun pickup (one original file, 0x801026b0-0x80102e58; model
 * SHOT.ZM, texmaps "shot"). Shooting it gives the shooter the shotgun and 1000 points.
 */
#include <stdlib.h>
#include <glide/glide.h>
#include "pickups.h"

float	*shotgun_verts, *shotgun_normals;		/* 0x801e5bcc, 0x801e5930 */
face_t	*shotgun_faces;					/* 0x8022b184 */

static int	shotgun_pickup_hit(game_object_t *, player_stats_t *);
static int	shotgun_pickup_die(game_object_t *, player_stats_t *);
static void	remove_shotgun_pickup(game_object_t *);

/* 0x801026b0: draw the pickup; once taken it is lit by a light of the taker's colour that
   fades by 12 a frame. The debug overlays as for the enemies */
void draw_shotgun_pickup(game_object_t *o)
{
	enemy_t	*e;
	float	colour[3], p[3];
	int	light;

	e = (enemy_t *)o->data;
	light = 0;
	if(e->ai_state == PICKUP_TAKEN)
	{
		colour[0] = (float)(((unsigned int)e->unk20 >> 16) & 0xff) - (float)(e->counter * 12);
		colour[1] = (float)(((unsigned int)e->unk20 >> 8) & 0xff) - (float)(e->counter * 12);
		colour[2] = (float)((unsigned int)e->unk20 & 0xff) - (float)(e->counter * 12);
		point_toward_camera(e->hit_group->pos, 1.0f, p);
		light = add_dynamic_light(0x11, p, colour, 3.0f, 15);
	}
	draw_model(e, shotgun_submesh, 1, shotgun_verts, 0x1c, shotgun_faces, 0x34, shotgun_normals, 0x1c);
	if(e->ai_state == PICKUP_TAKEN)
	{
		remove_light(light);
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
	if(debug_mode & 0x100)
	{
		func_800c8938(e->hit_group->pivot, e->radius, 0x600000);
	}
	if(debug_mode & 0x20)
	{
		guAlphaSource(1);
		guColorCombineFunction(1);
		grConstantColorValue(0xffffffff);
		grAlphaBlendFunction(1, 5, 1, 5);
		func_800d14a4(e->x * 0.12f + 200.0f, e->z * 0.12f + 128.0f);
		grAlphaBlendFunction(4, 0, 4, 0);
	}
	if(debug_mode & 0x1000)
	{
		func_800c99e4(e, 0xff0000);
	}
}

/* 0x8010297c: a shot against the pickup's sphere */
static int shotgun_pickup_hit(game_object_t *o, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	if(shot_hits_sphere(e->hit_group->pivot, e->radius * e->radius, p))
	{
		p->hit = 1;
		p->hit_id = o->id;
		p->die = (void *)shotgun_pickup_die;
		p->hit_point[0] = p->aim[0];
		p->hit_point[1] = p->aim[1];
		p->hit_point[2] = p->aim[2];
	}
	return 0;
}

/* 0x80102a08: taken: 1000 points, audit 0x4e, the "SHOTGUN X 10" message and cue 0x83, the
   shotgun; then it shrinks away in the taker's colour */
static int shotgun_pickup_die(game_object_t *o, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	if(debug_mode & DEBUG_CHARACTERS_BIT)
	{
		debug_enemy = e;
		return 0;
	}
	if(e->ai_state == PICKUP_WAITING)
	{
		award_points(p->index, 1000);
		increment_audit(0x4e);
		show_pickup_message(p, PICKUP_MSG_SHOTGUN);
		play_rotating_cue(0x83, "generic1");
		set_weapon(p, 2);
		e->ai_state = PICKUP_TAKEN;
		e->unk20 = p->colour;
		e->counter = 0;
		e->killer = p;
		return 0;
	}
	return 0;
}

/* 0x80102ac4 */
void load_shotgun_texmap(void)
{
	load_texmap(shotgun_texmap, 2, "shot");
}

/* 0x80102af4 */
void load_shotgun_model(void)
{
	load_model((void **)&shotgun_verts, 0x1c, (void **)&shotgun_faces, 0x34, (void **)&shotgun_normals, 0x1c, "SHOT.ZM", (int)0x7553d66f);
}

/* 0x80102b54 */
void remove_shotgun_assets(void)
{
	remove_surface(shotgun_texmap, 2);
	remove_model((void **)&shotgun_verts, 0x1c, (void **)&shotgun_faces, 0x34, (void **)&shotgun_normals, 0x1c, "SHOT.ZM");
}

/* 0x80102bb8: one group, hit radius 0.25 at scale 1, waiting to be shot */
int init_shotgun_pickup(game_object_t *o)
{
	enemy_t	*e;
	int	i;

	o->data = malloc(sizeof(enemy_t));
	if(o->data == 0)
	{
		return 1;
	}
	o->hit = shotgun_pickup_hit;
	o->die = shotgun_pickup_die;
	o->remove = remove_shotgun_pickup;
	e = (enemy_t *)o->data;
	e->counter = 0;
	e->frame = 0;
	e->ai_state = 0;
	e->flags = 0xc;
	e->rx = 0.0f;
	e->ry = 0.0f;
	e->rz = 0.0f;
	e->x = 4.0f;
	e->y = 0.0f;
	e->z = 0.0f;
	e->groups = malloc(sizeof(group_t));
	if(e->groups == 0)
	{
		free(e);
		return 1;
	}
	for(i = 0; i < 1; i++)
	{
		e->groups[i].flags = 0x9000;
		e->groups[i].pos[0] = 0.0f;
		e->groups[i].pos[1] = 0.0f;
		e->groups[i].pos[2] = 0.0f;
		e->groups[i].pivot[0] = 0.0f;
		e->groups[i].pivot[1] = 0.0f;
		e->groups[i].pivot[2] = 0.0f;
		e->groups[i].rot[0] = 4.712388f;
		e->groups[i].rot[1] = 0.0f;
		e->groups[i].rot[2] = 0.0f;
	}
	e->unkcc = 0.25f;
	set_enemy_scale(e, 1.0f);
	e->hit_group = e->groups;
	e->hit_group->rot[0] = 3.6651905f;
	e->unk98 = 0.0f;
	e->ai_state = PICKUP_WAITING;
	return 0;
}

/* 0x80102d4c: spin; once taken shrink over 16 frames and go */
void shotgun_pickup_think(game_object_t *o)
{
	enemy_t	*e;

	if(debug_mode & DEBUG_CHARACTERS_BIT)
	{
		return;
	}
	e = (enemy_t *)o->data;
	e->ry = e->ry + 0.2f;
	if(e->ai_state == PICKUP_TAKEN)
	{
		e->counter++;
		set_enemy_scale(e, (16.0f - e->counter) / 16.0f);
		if(e->counter >= PICKUP_SHRINK_FRAMES)
		{
			remove_die(o->id, e->killer);
		}
	}
}

/* 0x80102e04 */
static void remove_shotgun_pickup(game_object_t *o)
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
