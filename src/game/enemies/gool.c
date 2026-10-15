/*
 * gool.c - the Haunted House ghoul (object type 0x1102; GOOL.ZM / gool.za and the gool*.WMS
 * texmaps, sound bank "goul"). One original file, 0x800ea8e8-0x800ebb14 (the word at 0x800ebb14
 * is the padding before levelsnd.c), rodata 0x80170e48-0x80170f04, tables at 0x8018db98
 * (texmaps) and 0x8018deb8 (submeshes, gool_data.c). The model has 443 vertices, 523 normals
 * and 742 faces in 11 submeshes, the animation 223 frames. It walks a path of its own
 * (D_8017dfe8, spawn_paths_data.c) from point 0x1d5 to 0x1fd, with two red eyes flickering on
 * its head (submesh 5), then comes at the player and claws (submeshes 6 and 7 strike on fixed
 * frames).
 *
 * States: 0x180 walking the path (then at the player), 0x181 walking the path and then gone,
 * 0x120 hovering towards the player, 0x110 the claw attack, 0x100 a pause, 0x230-0x232 hit
 * reactions (blending back to the state they interrupted), 0x401 dying: it fades out.
 *
 * The exported names are the address names the shared files (spawn.c, wave.c) use:
 * gool_draw is the draw callback, gool_init the init, gool_think the think,
 * gool_setup the state set-up.
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

extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);	/* text at a world position */
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	draw_marker(float *, float, unsigned int);
extern void	func_800ceaf0(float *, enemy_t *, submesh_t *, int, float *);	/* objects.c: a point in a submesh, in the world */
extern void	pose_groups(enemy_t *, submesh_t *, int);	/* model.c: the groups placed at the current frame */
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern int	randrng(int);
extern void	path_place(enemy_t *, float *, int, float);
extern void	animate_loose_groups(enemy_t *, int);
extern int	face_camera(enemy_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_from_pose(enemy_t *, submesh_t *, int, int);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal(enemy_t *, float);
extern void	angles_to(float *, float *, float *);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern void	detach_group(group_t *, float *, float, float *);	/* model.c: a part flies off */
extern int	remove_die();
extern short	remove_game_object(short);
extern int	remove_light(int);
extern unsigned int next_cue_priority(void);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *);
extern enemy_t	*debug_enemy;
extern char	text_buffer[];
extern int	violence_level;

extern texmap_t	gool_texmap[25];		/* gool_data.c */
extern submesh_t gool_anim_blob[11];
extern float	D_8017dfe8[][6];		/* spawn_paths_data.c: its path */

/* the fade-out rate while dying (the record's word at 0x44) */
#define GOOL_FADE(e)	(*(float *)&(e)->unk44)

float	*gool_verts;			/* 0x801e5b4c */
face_t	*gool_faces;			/* 0x8021efe8 */
float	*gool_normals;			/* 0x801e5808 */
float	D_8018e01c[3] = { -0.04f, 0.07f, 0.0f };	/* the two eyes in the head's submesh */
float	D_8018e028[3] = { 0.038f, 0.07f, 0.0f };
float	D_8018e034 = 0.0f;		/* 0 for the first ghoul, then 1, then 2: where the next one starts */

int	gool_die(game_object_t *, player_stats_t *);
void	gool_remove(game_object_t *);
void	gool_setup(enemy_t *, unsigned short);
void	gool_claw(enemy_t *);
void	gool_approach(enemy_t *);

/* 0x800ea8e8: the draw callback: the eyes (unless it is dying), then the model */
void gool_draw(game_object_t *obj)
{
	enemy_t	*e;
	float	pt[3];

	e = (enemy_t *)obj->data;
	if(e->ai_state != 0x401)
	{
		e->alpha = randrng(2000) / 10000.0f + 0.1f;
		func_800ceaf0(pt, e, gool_anim_blob, 5, D_8018e01c);
		draw_marker(pt, e->alpha, 0xff0000);
		func_800ceaf0(pt, e, gool_anim_blob, 5, D_8018e028);
		draw_marker(pt, e->alpha, 0xff0000);
	}
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	draw_model(e, gool_anim_blob, 0xb, gool_verts, 0x1bb, gool_faces, 0x2e6, gool_normals, 0x20b);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		if(e == debug_enemy)
		{
			draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xff0000);
		}
		else if(e->ai_state == 0x401)
		{
			draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xff0000);
		}
		else
		{
			draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
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
		grConstantColorValue(0xffffff00);
		grAlphaBlendFunction(1, 5, 1, 5);
		func_800d14a4(e->x * 0.12f + 200.0f, e->z * 0.12f + 128.0f);
		grAlphaBlendFunction(4, 0, 4, 0);
	}
	if(debug_mode & 0x1000)
	{
		func_800c99e4(e, 0xff00);
	}
}

/* 0x800eab58: the hit test */
int gool_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pivot, e->radius * e->radius, p))
	{
		if(p->unk10 != 0xffff0000)
		{
			p->unk10 = 0xffffff00;
		}
		group = find_submesh_hit(e, gool_anim_blob, 0xb, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)gool_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x800eac28: the damage: dying (sparks at violence 1 and up, the last cry, 500 points, a fade)
   or a cry, 50 points and a hit reaction chosen by the part */
void gool_damage(game_object_t *obj, player_stats_t *p, float damage, enemy_t *e)
{
	int	group;
	int	i;

	group = p->hit_group;
	if(e->ai_state == 0x401)
	{
		return;
	}
	e->health -= damage;
	if(e->health <= 0.0f)
	{
		if(violence_level > 0)
		{
			add_world_sprite(p->hit_point, 3, 0);
			add_world_sprite(p->hit_point, 3, 0);
			add_world_sprite(p->hit_point, 3, 0);
			add_world_sprite(p->hit_point, 3, 0);
			add_world_sprite(p->hit_point, 3, 0);
		}
		play_enemy_sound(e, 2, "goul", 8);
		e->ai_state = 0x401;
		e->killer = p;
		e->alpha = 255.0f;
		GOOL_FADE(e) = -13.0f;
		for(i = 0; i < 0xb; i++)
		{
			e->groups[i].flags |= 0x80;
			e->groups[i].alpha = e->alpha;
		}
		e->flags &= 0xfffd;
		blend_from_pose(e, gool_anim_blob, 8, 0x60);
		e->frame = 0x60;
		e->ai_state = 0x401;
		award_points(p->index, 500);
		return;
	}
	play_enemy_sound(e, 1, "goul", 2);
	award_points(p->index, 50);
	if(e->ai_state != 0x230 && e->ai_state != 0x231 && e->ai_state != 0x232)
	{
		e->resume_frame = e->frame;
		e->resume_state = e->ai_state;
	}
	switch(group)
	{
	case 2:
	case 6:
	case 10:
		blend_from_pose(e, gool_anim_blob, 8, 0x86);
		e->frame = 0x74;
		e->ai_state = 0x231;
		break;
	case 3:
	case 7:
	case 9:
		blend_from_pose(e, gool_anim_blob, 8, 0x98);
		e->frame = 0x86;
		e->ai_state = 0x232;
		break;
	default:
		blend_from_pose(e, gool_anim_blob, 8, 0xaa);
		e->frame = 0x98;
		e->ai_state = 0x230;
		break;
	}
}

/* 0x800eaea8: the die callback (the shot lands): the part is marked shot, the damage; at the
   highest violence an arm comes off with its hand (6 and 10, or 7 and 9) */
int gool_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;
	group_t	*g;
	float	floor;

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return(0);
	}
	group = p->hit_group;
	g = &e->groups[group];
	g->flags |= 0xf0000000;
	gool_damage(obj, p, shot_damage_of(p), e);
	floor = e->y - 20.0f;
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 2:
		case 6:
		case 10:
			detach_group(&e->groups[10], e->groups[6].pivot, floor, e->scale);
			detach_group(&e->groups[6], e->groups[6].pivot, floor, e->scale);
			break;
		case 3:
		case 7:
		case 9:
			detach_group(&e->groups[9], e->groups[7].pivot, floor, e->scale);
			detach_group(&e->groups[7], e->groups[7].pivot, floor, e->scale);
			break;
		}
	}
	return(0);
}

/* 0x800eb018, 0x800eb048: the texmaps, the model and the animation */
void load_gool_texmap(void)
{
	load_texmap(gool_texmap, 0x19, "gool");
}

void load_gool(void)
{
	load_model((void **)&gool_verts, 0x1bb, (void **)&gool_faces, 0x2e6, (void **)&gool_normals, 0x20b, "GOOL.ZM", 0xd27a169e);
	load_anim(gool_anim_blob, 0xb, 0xdf, "gool.za", 0x091684f2);
}

/* 0x800eb0d0 */
void remove_gool(void)
{
	remove_surface(gool_texmap, 0x19);
	remove_animation(gool_anim_blob, 0xb, 0xdf, "gool.za");
	remove_model((void **)&gool_verts, 0x1bb, (void **)&gool_faces, 0x2e6, (void **)&gool_normals, 0x20b, "GOOL.ZM");
}

/* 0x800eb150: the init callback: the record (the ghouls take turns between two starting
   places), the first state, the groups (the head's flagged 0x40) placed at the first frame */
int gool_init(game_object_t *obj)
{
	enemy_t	*e;
	group_t	*g;
	void	*data;
	int	i;

	data = malloc(sizeof(enemy_t));
	obj->data = data;
	if(!data)
	{
		return(1);
	}
	obj->hit = gool_hit;
	obj->die = gool_die;
	obj->remove = gool_remove;
	e = (enemy_t *)data;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0x2c;
	e->target = get_target_player();
	e->rz = 0;
	e->rx = 0;
	e->ry = 0;
	if(D_8018e034 == e->rz)
	{
		e->z = e->rz;
		e->x = 6.0f;
		e->y = 2.0f;
		D_8018e034 = 1.0f;
	}
	else
	{
		e->x = -8.0f;
		e->y = 2.0f;
		e->z = -6.0f;
		D_8018e034 = 2.0f;
	}
	e->unkcc = 0.7f;
	set_enemy_scale(e, 1.0f);
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	gool_setup(e, 0x180);
	if(D_8018e034 == 2.0f)
	{
		e->frame += 0xc;
	}
	e->vel[0] = 0;
	e->vel[1] = 0;
	e->vel[2] = 0;
	set_enemy_health(e, 3.5f);
	e->groups = malloc(0xb * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < 0xb; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[5].flags |= 0x40;
	pose_groups(e, gool_anim_blob, 0xb);
	g = e->groups;
	e->hit_group = &g[8];
	g[8].floor = -20.0f;
	e->unk1a = 0x1102;
	e->path = 0x1d5;
	e->unk90 = 0x1d5;
	e->unke0 = 0;
	e->unkd0 = 0;
	e->unk92 = 0x1fd;
	return(0);
}

/* 0x800eb36c: the remove callback */
void gool_remove(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(debug_enemy == e)
	{
		debug_enemy = 0;
	}
	if(e->unke0)
	{
		((enemy_t *)e->unke0)->unke4 = 0;
	}
	if(e->unkd0)
	{
		remove_light(e->unkd0);
		e->unkd0 = 0;
	}
	free(e->groups);
	free(e);
}

/* 0x800eb3e0: the think callback */
void gool_think(game_object_t *obj)
{
	enemy_t	*e;
	int	i;

	e = (enemy_t *)obj->data;
	animate_loose_groups(e, 0xb);
	if(debug_mode & 0x10000)
	{
		return;
	}
	gool_claw(e);
	switch(e->ai_state)
	{
	case 0x180:
		if(++e->frame >= 0xbd)
		{
			e->frame = 0xaa;
		}
		path_place(e, D_8017dfe8[0], e->path, 1.0f);
		if(++e->path > e->unk92)
		{
			gool_approach(e);
		}
		break;
	case 0x181:
		if(++e->frame >= 0xbd)
		{
			e->frame = 0xaa;
		}
		path_place(e, D_8017dfe8[0], e->path, 1.0f);
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
	case 0x120:
		if(++e->frame >= 0xbd)
		{
			e->frame = 0xaa;
		}
		face_camera(e, 0.2f);
		gool_approach(e);
		break;
	case 0x110:
		face_camera(e, 0.2f);
		if(++e->frame >= 0x60)
		{
			gool_approach(e);
		}
		break;
	case 0x100:
		if(++e->frame >= 0xdf)
		{
			gool_approach(e);
		}
		break;
	case 0x401:
		e->alpha += GOOL_FADE(e);
		if(e->alpha > 0.0f)
		{
			for(i = 0; i < 0xb; i++)
			{
				e->groups[i].flags |= 0x80;
				e->groups[i].alpha = e->alpha;
			}
		}
		if(++e->frame >= 0x74)
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
	case 0x230:
		if(++e->frame >= 0xaa)
		{
			blend_from_pose(e, gool_anim_blob, 8, e->resume_frame);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x231:
		if(++e->frame >= 0x86)
		{
			blend_from_pose(e, gool_anim_blob, 8, e->resume_frame);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x232:
		if(++e->frame >= 0x98)
		{
			blend_from_pose(e, gool_anim_blob, 8, e->resume_frame);
			e->ai_state = e->resume_state;
		}
		break;
	}
	angles_to(&camera_x, &e->x, &e->rx);
}

/* 0x800eb7a0 */
void func_800eb7a0(enemy_t *e, int state)
{
	if(state < 0)
	{
		randrng(100);
	}
	e->ai_state = 0;
}

/* 0x800eb7d4: the state set-up: the walks and the hover start on the hover loop's first frame */
void gool_setup(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	switch(state)
	{
	case 0x180:
	case 0x181:
		e->frame = 0xaa;
		break;
	case 0x120:
		e->frame = 0xaa;
		e->ai_state = 0x120;
		break;
	}
}

/* 0x800eb820 */
void func_800eb820(void)
{
}

/* 0x800eb828: the claw attack lands on fixed frames while the claw's group is still on */
void gool_claw(enemy_t *e)
{
	if(e->ai_state != 0x110)
	{
		return;
	}
	switch(e->frame)
	{
	case 7:
		if(e->groups[7].flags & 0x2000)
		{
			hurt_player(e->target, 0x5f, 1, 0.0f);
		}
		break;
	case 0x29:
		if(e->groups[7].flags & 0x2000)
		{
			hurt_player(e->target, 0x5f, 1, 0.0f);
		}
		break;
	case 0x47:
		if(e->groups[7].flags & 0x2000)
		{
			hurt_player(e->target, 0x5f, 1, 0.0f);
		}
		break;
	case 0x16:
	case 0x37:
	case 0x57:
		if(e->groups[6].flags & 0x2000)
		{
			hurt_player(e->target, 0x5f, 1, 3.1415927f);
		}
		break;
	}
}

/* 0x800eb94c: go for the player: keep a playing target, steer to a point in front of the
   camera and pick the state by how close it got (0 still far: hover, 1 there: pause, 2 in
   reach: claw) */
void gool_approach(enemy_t *e)
{
	player_stats_t	*p;
	int	r;

	if(e->target->state != 4)
	{
		p = get_target_player();
		if(p->state == 4)
		{
			e->target = p;
		}
	}
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 5.0f, -17.5f);
	r = step_toward_goal(e, 2.0f);
	switch(r)
	{
	case 1:
		if(e->ai_state == 0x100)
		{
			e->frame = 0xbd;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, gool_anim_blob, 8, 0xbd, 2);
		e->ai_state = 0x100;
		break;
	case 2:
		if(e->ai_state & 0x200)
		{
			e->frame = 0;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, gool_anim_blob, 8, 0, r);
		e->ai_state = 0x110;
		break;
	case 0:
		if(e->ai_state == 0x120)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, gool_anim_blob, 8, 0xaa, 2);
		e->ai_state = 0x120;
		break;
	}
}
