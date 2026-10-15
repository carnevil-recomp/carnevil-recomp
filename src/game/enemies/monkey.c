/*
 * monkey.c - the Freak Show's monkey (object type 0x2022, assets MONK.ZM / monk.za / monk.pth,
 * the monk*.WMS texmaps: fur, hands, face, ears, hat, teeth, tongue; sound bank "monkey"). One
 * original file, 0x8011a170-0x8011b50c (the word at 0x8011b50c is padding), its tables at
 * 0x80195a70 (texmaps) and 0x801960b0 (submeshes, monkey_data.c). The model has 568 vertices,
 * 879 faces and 580 normals in 32 submeshes, the animation 229 frames, the path 1125 points.
 *
 * It runs its path (0x801, 0x802, 0x904; 0xa03 runs it once and swipes at the end), stands and
 * plays (0, 6, 7, 5: 5 counts 21 loops before the path's end is handled), or hunts the player
 * on foot (1 walk, 3 crouch, 0x201 swipe; steered by monkey_steer): the swipes hurt the player
 * for 130 when the hand (part 1) or body (part 9) is within 15 units on the frame they land.
 * One shot at 1.5 health kills it (300 points, a cry; 30 for a hit that does not).
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

extern void	draw_effects(enemy_t *);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern int	randrng(int);
extern void	path_place(enemy_t *, void *, short, float);
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern int	face_camera(enemy_t *, float);
extern int	group_near_camera(group_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal_xz(enemy_t *, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern int	mark_die();
extern int	remove_die();
extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern char	text_buffer[];
extern enemy_t	*debug_enemy;

extern texmap_t	monk_texmap[50];			/* monkey_data.c */
extern submesh_t submesh_info_monk[32];

float	*monk_verts;			/* 0x801e5b88 */
face_t	*monk_faces;			/* 0x8021f0c8 */
float	*monk_normals;			/* 0x801e5848 */
void	*monk_path;			/* 0x802373dc */

#define MONK_GROUPS	32
#define MONK_NVERTS	0x238
#define MONK_NFACES	0x36f
#define MONK_NNORMALS	0x244
#define MONK_FRAMES	0xe5
#define MONK_PATH_LEN	0x465
#define MONK_HAND	1		/* the swipes land through the hand ... */
#define MONK_BODY	9		/* ... or the body */
#define MONK_HIT_GROUP	10		/* the submesh it is hit through */
#define MONK_SWIPE	130		/* the damage of a swipe */
#define MONK_REACH	15.0f

int	monkey_die(game_object_t *, player_stats_t *);
void	monkey_remove(game_object_t *);
void	monkey_setup(enemy_t *, unsigned short);
void	monkey_steer(enemy_t *);
void	monkey_end_path(enemy_t *);

/* 0x8011a170: the draw callback */
void monkey_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_monk, MONK_GROUPS, monk_verts, MONK_NVERTS, monk_faces, MONK_NFACES, monk_normals, MONK_NNORMALS);
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

/* 0x8011a36c: the hit test: the bounding sphere, then the submesh */
int monkey_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_monk, MONK_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)monkey_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x8011a428: the shot lands: blood and chunks; it dies at once (300 points and a cry, a
   crouching monkey drops 13 to the floor), else 30 points */
int monkey_die(game_object_t *obj, player_stats_t *p)
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
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			if(e->ai_state & 0x100)
			{
				e->y -= 13.0f;
			}
			award_points(p->index, 300);
			play_enemy_sound(e, randrng(12) + 1, "monkey", 2);
			e->killer = p;
			mark_die(obj->id, p);
			e->ai_state = 0x2001;
			e->frame = 0x23;
			e->flags &= 0xfffd;
		}
		else
		{
			award_points(p->index, 30);
		}
	}
	return(0);
}

/* 0x8011a588 */
void load_monk_texmap(void)
{
	load_texmap(monk_texmap, 50, "monk");
}

/* 0x8011a5b8 */
void load_monk(void)
{
	load_model((void **)&monk_verts, MONK_NVERTS, (void **)&monk_faces, MONK_NFACES, (void **)&monk_normals, MONK_NNORMALS, "MONK.ZM", 0x1231bb04);
	load_anim(submesh_info_monk, MONK_GROUPS, MONK_FRAMES, "monk.za", 0x87799b33);
	load_path(&monk_path, MONK_PATH_LEN, "monk.pth", 0xe085101a);
}

/* 0x8011a660: the assets go when the level is unloaded */
void remove_monk_asets(void)
{
	remove_surface(monk_texmap, 50);
	remove_path(&monk_path, MONK_PATH_LEN, "monk.pth");
	remove_animation(submesh_info_monk, MONK_GROUPS, MONK_FRAMES, "monk.za");
	remove_model((void **)&monk_verts, MONK_NVERTS, (void **)&monk_faces, MONK_NFACES, (void **)&monk_normals, MONK_NNORMALS, "MONK.ZM");
}

/* 0x8011a6f8: the init callback: the record, scale 0.5, health 1.5, the 32 groups */
int monkey_init(game_object_t *obj)
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
	obj->hit = monkey_hit;
	obj->die = monkey_die;
	obj->remove = monkey_remove;
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
	e->x = -10.0f;
	e->unkcc = 0.62f;
	set_enemy_scale(e, 0.5f);
	set_enemy_health(e, 1.5f);
	e->groups = malloc(MONK_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < MONK_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	init_groups(e, submesh_info_monk, MONK_GROUPS);
	g = e->groups;
	e->hit_group = &g[MONK_HIT_GROUP];
	g[MONK_HIT_GROUP].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	monkey_setup(e, 0);
	return(0);
}

/* 0x8011a878: the remove callback */
void monkey_remove(game_object_t *obj)
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

/* 0x8011a8d0: the think callback: the animation of each state; the runs follow the path, the
   swipes land on their frame when the hand or body is within reach */
void monkey_think(game_object_t *obj)
{
	enemy_t	*e;

	if(debug_mode & 0x10000)
	{
		return;
	}
	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, MONK_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0:
		if(++e->frame >= MONK_FRAMES)
		{
			e->frame = 0;
		}
		break;
	case 1:
		if(++e->frame >= 0xf)
		{
			e->frame = 0;
		}
		face_camera(e, 0.2f);
		monkey_steer(e);
		break;
	case 3:
		if(++e->frame >= 0x44)
		{
			monkey_steer(e);
		}
		break;
	case 4:
		if(++e->frame >= 0xd3)
		{
			if(group_near_camera(&e->groups[MONK_BODY], MONK_REACH))
			{
				hurt_player(e->target, MONK_SWIPE, 3, 0.0f);
			}
			remove_die(obj->id, 0);
		}
		break;
	case 5:
		if(++e->frame >= 0x44)
		{
			e->frame = 0x35;
		}
		if(++e->counter >= 0x15)
		{
			monkey_end_path(e);
		}
		break;
	case 6:
		if(++e->frame >= 0xf)
		{
			e->frame = 0;
		}
		break;
	case 7:
		if(++e->frame >= 0x44)
		{
			e->frame = 0x35;
		}
		break;
	case 0x801:
		if(++e->frame >= 0xf)
		{
			e->frame = 0;
		}
		path_place(e, monk_path, e->path, 1.0f);
		if(++e->path >= e->unk92)
		{
			monkey_end_path(e);
		}
		break;
	case 0x802:
		if(++e->frame >= 0x4f)
		{
			e->frame = 0x44;
		}
		path_place(e, monk_path, e->path, 1.0f);
		if(++e->path >= e->unk92)
		{
			monkey_end_path(e);
		}
		break;
	case 0xa03:
		if(++e->frame >= 0x4f)
		{
			e->frame = 0x44;
		}
		path_place(e, monk_path, e->path, 1.0f);
		if(++e->path >= e->unk92)
		{
			if(group_near_camera(&e->groups[MONK_BODY], MONK_REACH))
			{
				hurt_player(e->target, MONK_SWIPE, 3, 0.0f);
			}
			remove_die(obj->id, 0);
		}
		break;
	case 0x904:
		if(++e->frame >= 0xa8)
		{
			e->frame = 0x95;
		}
		path_place(e, monk_path, e->path, 1.0f);
		if(++e->path >= e->unk92)
		{
			monkey_end_path(e);
		}
		break;
	case 0x201:
		face_camera(e, 0.2f);
		if(++e->frame == 0x1d)
		{
			if(group_near_camera(&e->groups[MONK_HAND], MONK_REACH))
			{
				hurt_player(e->target, MONK_SWIPE, 3, 0.0f);
			}
		}
		if(e->frame >= 0x23)
		{
			monkey_steer(e);
		}
		break;
	case 0x206:
		face_camera(e, 0.2f);
		if(++e->frame == 0x1d)
		{
			if(group_near_camera(&e->groups[MONK_HAND], MONK_REACH))
			{
				hurt_player(e->target, MONK_SWIPE, 3, 0.0f);
			}
		}
		if(e->frame >= 0x23)
		{
			e->frame = 0xf;
		}
		break;
	case 0x302:
		if(++e->frame == 0xb1)
		{
			if(group_near_camera(&e->groups[MONK_HAND], MONK_REACH))
			{
				hurt_player(e->target, MONK_SWIPE, 3, 0.0f);
			}
		}
		if(e->frame >= 0xb9)
		{
			e->frame = 0xa8;
		}
		break;
	case 0x303:
		if(++e->frame == 0x80)
		{
			if(group_near_camera(&e->groups[MONK_HAND], MONK_REACH))
			{
				hurt_player(e->target, MONK_SWIPE, 3, 0.0f);
			}
		}
		if(e->frame >= 0x95)
		{
			e->ai_state = 0x304;
			e->frame = 0x4f;
		}
		break;
	case 0x304:
		if(++e->frame == 0x61)
		{
			if(group_near_camera(&e->groups[MONK_HAND], MONK_REACH))
			{
				hurt_player(e->target, MONK_SWIPE, 3, 0.0f);
			}
		}
		if(e->frame >= 0x6e)
		{
			e->frame = 0x4f;
		}
		break;
	case 0x205:
		if(++e->frame == 0xdb)
		{
			if(group_near_camera(&e->groups[MONK_BODY], MONK_REACH))
			{
				hurt_player(e->target, MONK_SWIPE, 3, 0.0f);
			}
		}
		if(e->frame >= 0xe5)
		{
			e->frame = 0xd3;
		}
		break;
	case 0x2001:
		if(++e->frame >= 0x35)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		break;
	}
}

/* 0x8011af54: the spawn table's state and its first frame; the path states start where the
   entry says */
void monkey_setup(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	switch(state)
	{
	case 0:
		e->frame = 0;
		break;
	case 4:
		e->frame = 0xb9;
		break;
	case 5:
		e->frame = 0x35;
		e->counter = 0;
		break;
	case 3:
		e->frame = 0x35;
		break;
	case 7:
		e->frame = 0x35;
		break;
	case 0x302:
		e->frame = 0xa8;
		break;
	case 0x201:
		e->frame = 0xf;
		break;
	case 0x303:
		e->frame = 0x6e;
		break;
	case 0x205:
		e->frame = 0xd3;
		break;
	case 0x802:
		e->frame = 0x44;
		path_place(e, monk_path, e->path, 1.0f);
		break;
	case 0x801:
		e->frame = 0;
		path_place(e, monk_path, e->path, 1.0f);
		break;
	case 0xa03:
		e->frame = 0x44;
		path_place(e, monk_path, e->path, 1.0f);
		break;
	case 0x904:
		e->frame = 0x95;
		path_place(e, monk_path, e->path, 1.0f);
		break;
	case 1:
	case 6:
		e->frame = 0;
		break;
	default:
		e->frame = 0;
		break;
	}
}

/* 0x8011b124: hunting on foot: steer at a point beside the player; at it, crouch (3); in reach,
   swipe (0x201); else walk (1) */
void monkey_steer(enemy_t *e)
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
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0, -12.0f);
	r = step_toward_goal_xz(e, 1.3f);
	switch(r)
	{
	case 1:
		if(e->ai_state == 3)
		{
			e->frame = 0x35;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_monk, 10, 0x35, 5);
		e->ai_state = 3;
		break;
	case 2:
		if(e->ai_state & 0x200)
		{
			e->frame = 0xf;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_monk, 10, 0xf, 5);
		e->ai_state = 0x201;
		break;
	case 0:
		if(e->ai_state == 1)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_monk, 10, 0, 5);
		e->ai_state = 1;
		break;
	}
}

/* 0x8011b2dc: the path ran out: the spawn entry (unk6) says what follows: a stand (4, 7, 0x302,
   0x206), another run (0x801, 0xa03), removal (8); anything else hunts */
void monkey_end_path(enemy_t *e)
{
	switch((unsigned short)e->unk6)
	{
	case 8:
		remove_die((unsigned short)e->id, 0);
		break;
	case 4:
		e->unk6 = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_monk, 10, 0xb9, 2);
		e->ai_state = 4;
		break;
	case 0x302:
		e->unk6 = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_monk, 10, 0xa8, 2);
		e->ai_state = 0x302;
		break;
	case 7:
		e->unk6 = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_monk, 10, 0x35, 2);
		e->ai_state = 7;
		break;
	case 0x206:
		e->unk6 = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_monk, 10, 0xf, 2);
		e->ai_state = 0x206;
		break;
	case 0x801:
		e->unk6 = 0;
		blend_to_frame(e, submesh_info_monk, 10, 0, 2);
		e->ai_state = 0x801;
		path_place(e, monk_path, e->path, 1.0f);
		break;
	case 0xa03:
		e->unk6 = 0;
		blend_to_frame(e, submesh_info_monk, 10, 0x44, 2);
		e->ai_state = 0xa03;
		path_place(e, monk_path, e->path, 1.0f);
		break;
	default:
		e->unk6 = 0;
		monkey_steer(e);
		break;
	}
}
