/*
 * pickup_health.c - the health pickup (one original file, 0x80102e58-0x80103568; model
 * HLTH.ZM, texmap "hlth"). A beating heart at half scale; shooting it heals the shooter by
 * 250 and gives 1000 points.
 */
#include <stdlib.h>
#include "pickups.h"

extern void	heal_player(player_stats_t *, float);		/* player.c */

float	*health_verts, *health_normals;			/* 0x801e5b34, 0x801e57f4 */
face_t	*health_faces;					/* 0x8021ef54 */

static int	health_pickup_hit(game_object_t *, player_stats_t *);
static int	health_pickup_die(game_object_t *, player_stats_t *);
static void	remove_health_pickup(game_object_t *);

#define HEALTH_SCALE		0.5f
#define HEALTH_BEAT_FRAMES	21
#define BEAT(e)		(*(unsigned short *)&(e)->unk4)	/* enemy_t.unk4: the frame of the beat */

/* 0x80102e58: draw the heart; once taken it is lit in the taker's colour fading by 12 a
   frame. Fewer debug overlays than the other pickups (no map marker, the label always white) */
void draw_health_pickup(game_object_t *o)
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
	draw_model(e, health_submesh, 1, health_verts, 0x24, health_faces, 0x44, health_normals, 0x28);
	if(e->ai_state == PICKUP_TAKEN)
	{
		remove_light(light);
	}
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->hit_group->pivot, e->radius, 0x600000);
	}
	if(debug_mode & 0x1000)
	{
		func_800c99e4(e, 0xff0000);
	}
}

/* 0x8010308c: a shot against the heart's sphere */
static int health_pickup_hit(game_object_t *o, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	if(shot_hits_sphere(e->hit_group->pivot, e->radius * e->radius, p))
	{
		p->hit = 1;
		p->hit_id = o->id;
		p->die = (void *)health_pickup_die;
		p->hit_point[0] = p->aim[0];
		p->hit_point[1] = p->aim[1];
		p->hit_point[2] = p->aim[2];
	}
	return 0;
}

/* 0x80103118: taken: 1000 points, audit 0x4f, "HEALTH UP" and cue 0x7b, 250 health */
static int health_pickup_die(game_object_t *o, player_stats_t *p)
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
		increment_audit(0x4f);
		show_pickup_message(p, PICKUP_MSG_HEALTH);
		play_rotating_cue(0x7b, "generic1");
		heal_player(p, 250.0f);
		e->ai_state = PICKUP_TAKEN;
		e->unk20 = p->colour;
		e->counter = 0;
		e->killer = p;
		return 0;
	}
	return 0;
}

/* 0x801031d4 */
void load_health_texmap(void)
{
	load_texmap(health_texmap, 1, "hlth");
}

/* 0x80103204 */
void load_health_model(void)
{
	load_model((void **)&health_verts, 0x24, (void **)&health_faces, 0x44, (void **)&health_normals, 0x28, "HLTH.ZM", (int)0x79f4cd6c);
}

/* 0x80103264 */
void remove_health_assets(void)
{
	remove_surface(health_texmap, 1);
	remove_model((void **)&health_verts, 0x24, (void **)&health_faces, 0x44, (void **)&health_normals, 0x28, "HLTH.ZM");
}

/* 0x801032c8: one group at the origin, hit radius 0.35 at scale 1, the beat at frame 0 */
int init_health_pickup(game_object_t *o)
{
	enemy_t	*e;
	int	i;

	o->data = malloc(sizeof(enemy_t));
	if(o->data == 0)
	{
		return 1;
	}
	o->hit = health_pickup_hit;
	o->die = health_pickup_die;
	o->remove = remove_health_pickup;
	e = (enemy_t *)o->data;
	e->counter = 0;
	e->unk4 = 0;
	e->frame = 0;
	e->ai_state = 0;
	e->flags = 0xc;
	e->rx = 0.0f;
	e->ry = 0.0f;
	e->rz = 0.0f;
	e->x = 0.0f;
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
		e->groups[i].flags = 0x8000;
		e->groups[i].pos[0] = 0.0f;
		e->groups[i].pos[1] = 0.0f;
		e->groups[i].pos[2] = 0.0f;
		e->groups[i].pivot[0] = 0.0f;
		e->groups[i].pivot[1] = 0.0f;
		e->groups[i].pivot[2] = 0.0f;
	}
	e->unkcc = 0.35f;
	set_enemy_scale(e, 1.0f);
	e->unk98 = 0.0f;
	e->ai_state = PICKUP_WAITING;
	e->hit_group = e->groups;
	return 0;
}

/* 0x80103420: spin and beat (unk4 steps through health_pulse); once taken shrink over 16
   frames and go. Unlike the others it is not frozen by the character debug mode */
void health_pickup_think(game_object_t *o)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	e->ry = e->ry + 0.2f;
	if(++BEAT(e) >= HEALTH_BEAT_FRAMES)
	{
		BEAT(e) = 0;
	}
	set_enemy_scale(e, health_pulse[BEAT(e)] * HEALTH_SCALE);
	if(e->ai_state == PICKUP_TAKEN)
	{
		e->counter++;
		set_enemy_scale(e, (16.0f - e->counter) / 16.0f * HEALTH_SCALE);
		if(e->counter >= PICKUP_SHRINK_FRAMES)
		{
			remove_die(o->id, e->killer);
		}
	}
}

/* 0x80103518 */
static void remove_health_pickup(game_object_t *o)
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
