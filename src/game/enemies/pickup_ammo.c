/*
 * pickup_ammo.c - the extra clip pickup (one original file, 0x80103568-0x80103c90; model
 * AMMO.ZM, texmaps "ammo"). Shooting it gives the shooter the pistol with the bigger clip
 * (give_pistol) and 1000 points. Its code has fewer debug hooks than the shotgun's.
 */
#include <stdlib.h>
#include <glide/glide.h>
#include "pickups.h"

extern void	give_pistol(player_stats_t *);		/* shoot.c */

float	*ammo_verts, *ammo_normals;			/* 0x801e5948, 0x801e43a4 */
face_t	*ammo_faces;					/* 0x80212e80 */

static int	ammo_pickup_hit(game_object_t *, player_stats_t *);
static int	ammo_pickup_die(game_object_t *, player_stats_t *);
static void	remove_ammo_pickup(game_object_t *);

/* 0x80103568: draw the pickup; once taken it is lit by a light of the taker's colour that
   fades by 12 a frame. The debug overlays as for the enemies */
void draw_ammo_pickup(game_object_t *o)
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
	draw_model(e, ammo_submesh, 1, ammo_verts, 0x4a, ammo_faces, 0x90, ammo_normals, 0x4a);
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
		func_800c8938(e->hit_group->pos, e->radius, 0x600000);
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
}

/* 0x80103804: a shot against the pickup's sphere */
static int ammo_pickup_hit(game_object_t *o, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		p->hit = 1;
		p->hit_id = o->id;
		p->die = (void *)ammo_pickup_die;
		p->hit_point[0] = p->aim[0];
		p->hit_point[1] = p->aim[1];
		p->hit_point[2] = p->aim[2];
	}
	return 0;
}

/* 0x80103890: taken: 1000 points, audit 0x52, "INCREASE CLIP" and cue 0x7c, the pistol
   with the bigger clip; then it shrinks away in the taker's colour */
static int ammo_pickup_die(game_object_t *o, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	if(e->ai_state == PICKUP_WAITING)
	{
		award_points(p->index, 1000);
		increment_audit(0x52);
		show_pickup_message(p, PICKUP_MSG_CLIP);
		play_rotating_cue(0x7c, "generic1");
		give_pistol(p);
		e->ai_state = PICKUP_TAKEN;
		e->unk20 = p->colour;
		e->counter = 0;
		e->killer = p;
		return 0;
	}
	return 0;
}

/* 0x80103928 */
void load_ammo_texmap(void)
{
	load_texmap(ammo_texmap, 2, "ammo");
}

/* 0x80103958 */
void load_ammo_model(void)
{
	load_model((void **)&ammo_verts, 0x4a, (void **)&ammo_faces, 0x90, (void **)&ammo_normals, 0x4a, "AMMO.ZM", (int)0x6db3a32b);
}

/* 0x801039b8 */
void remove_ammo_assets(void)
{
	remove_surface(ammo_texmap, 2);
	remove_model((void **)&ammo_verts, 0x4a, (void **)&ammo_faces, 0x90, (void **)&ammo_normals, 0x4a, "AMMO.ZM");
}

/* 0x80103a1c: one group, hit radius 0.25 at scale 1, waiting to be shot */
int init_ammo_pickup(game_object_t *o)
{
	enemy_t	*e;
	int	i;

	o->data = malloc(sizeof(enemy_t));
	if(o->data == 0)
	{
		return 1;
	}
	o->hit = ammo_pickup_hit;
	o->die = ammo_pickup_die;
	o->remove = remove_ammo_pickup;
	e = (enemy_t *)o->data;
	e->counter = 0;
	e->frame = 0;
	e->ai_state = 0;
	e->flags = 0xc;
	e->rx = 0.0f;
	e->ry = 0.0f;
	e->rz = 0.0f;
	e->x = 8.0f;
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

/* 0x80103bb0: spin; once taken shrink over 16 frames and go */
void ammo_pickup_think(game_object_t *o)
{
	enemy_t	*e;

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

/* 0x80103c54 */
static void remove_ammo_pickup(game_object_t *o)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	free(e->groups);
	free(e);
}
