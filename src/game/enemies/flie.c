/*
 * flie.c - the Freak Show's fly man (object type 0x2029, the level's first spawn; assets
 * FLIE.ZM / flie.za / flie.pth and the flie*.WMS texmaps: wings, fur, head, hat, a fly swatter
 * and its handle; sound bank "fly"). One original file, 0x80122e40-0x80123f84, its tables at
 * 0x801994e0 (texmaps) and 0x80199aa0 (submeshes, flie_data.c), rodata 0x80173b88-0x80173c1c.
 * The model has 1004 vertices, 1593 faces and 1087 normals in 23 submeshes, the animation 127
 * frames, the path 1263 points.
 *
 * It flies its path (0x801 / 0x803), then closes on the camera: it hovers towards a point beside
 * and in front of the player (0x100), stops when near (0x102) and swats twice (0x201: frames
 * 110 and 121 hurt the player for 105 when the swatter's submeshes 21 and 22 are within 15).
 * 0x802 flies the path to its end and swats once there (submesh 7). Shots knock it about at
 * violence 3 (parts swapped for their damaged versions); it dies at 1.5 health (500 points).
 */
#include <stdlib.h>
#include <glide/glide.h>
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "texmap.h"
#include "credits.h"			/* debug_mode, player_stats_t */
#include "levelsnd.h"
#include "camera.h"			/* camera_x */

extern void	draw_effects(enemy_t *), think_effects(enemy_t *), free_effects(enemy_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, int, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern void	path_place(enemy_t *, void *, short, float);
extern int	group_near_camera(group_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern int	remove_die();
extern void	mark_die(short, player_stats_t *);
extern void	replace_group(enemy_t *, group_t *, group_t *);
extern void	*attach_flame(float, float, group_t *);
extern void	attach_effect(enemy_t *, void *);
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern int	randrng(int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal(enemy_t *, float);
extern int	turn_toward_camera(enemy_t *, float);		/* enemy.c: turn towards the camera on all three axes */
extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern int	violence_level;
extern enemy_t	*debug_enemy;
extern char	text_buffer[];

extern texmap_t	flie_texmap[46];		/* flie_data.c */
extern submesh_t submesh_info_flie[23];

float	*flie_verts;			/* 0x801e5840 */
face_t	*flie_faces;			/* 0x80212dec */
float	*flie_normals;			/* 0x801e0f54 */
void	*flie_path;			/* 0x8022b178 */

#define FLIE_GROUPS	0x17
#define FLIE_NVERTS	0x3ec
#define FLIE_NFACES	0x639
#define FLIE_NNORMALS	0x43f
#define FLIE_FRAMES	0x7f
#define FLIE_PATH_LEN	0x4ef
#define FLIE_TEXMAPS	0x2e
#define FLIE_BODY	9		/* the submesh it is hit through and measured by */
#define FLIE_SWAT	0x69		/* the damage of a swat */
#define FLIE_FLY_FRAMES	0x13		/* the wing-beat loop */

/* the states */
#define FLIE_IDLE	0		/* hovers in place (frames 0-126) */
#define FLIE_PATH	0x801		/* flies its path, then comes for the camera */
#define FLIE_PATH_SWAT	0x802		/* flies its path and swats at its end */
#define FLIE_CIRCLE	0x803		/* one wing-beat loop, then the path or the camera */
#define FLIE_APPROACH	0x100		/* turns and hovers towards the camera */
#define FLIE_STOP	0x102		/* near enough: settles */
#define FLIE_ATTACK	0x201		/* the swats (frames 104-126) */
#define FLIE_HIT	0x1001		/* the hit reaction */
#define FLIE_DYING	0x2001
#define FLIE_DEAD	0x4000
#define FLIE_FALL	0x8103		/* loops frames 94-103 */

int	flie_die(game_object_t *, player_stats_t *);
void	flie_remove(game_object_t *);
void	flie_setup(enemy_t *, unsigned short);
void	flie_approach(enemy_t *);
void	func_80123f24(enemy_t *);

/* 0x80122e40: the draw callback */
void flie_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_flie, FLIE_GROUPS, flie_verts, FLIE_NVERTS, flie_faces, FLIE_NFACES, flie_normals, FLIE_NNORMALS);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[FLIE_BODY].pos, e->radius, 0x600000);
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
			draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xff0000);
		}
		else
		{
			draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xffffff);
		}
	}
	if(debug_mode & 0x1000)
	{
		func_800c99e4(e, 0xff);
	}
}

/* 0x8012303c: the hit test: the bounding sphere, then the submesh */
int flie_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_flie, FLIE_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)flie_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x801230f8: the shot lands: blood and a chunk; the flame thrower sets the part alight; at
   violence 3 the part hit is swapped for a damaged one (the wings 7, 19 and 20, the head 9;
   anything else turns gory, a second time swaps the body 8 for 17); then damage (dead at 0:
   500 points and its death cry, else 50 and a hurt cry) and the hit reaction */
int flie_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;
	int	group;

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return(0);
	}
	burst_blood(p->hit_point, e->unk98, 0x800000);
	add_chunks(e->hit_group->pos, e->unk98);
	add_world_sprite(p->hit_point, 0, 0);
	group = p->hit_group;
	g = &e->groups[group];
	if(p->shot_weapon == 0x1000004)
	{
		if(!(g->flags & 2))
		{
			g->flags |= 2;
			attach_effect(e, attach_flame(0.0f, (submesh_info_flie[group].radius + submesh_info_flie[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 7:
			if(randrng(100) < 50)
			{
				replace_group(e, &e->groups[7], &e->groups[20]);
			}
			else
			{
				replace_group(e, &e->groups[7], &e->groups[19]);
			}
			break;
		case 20:
			replace_group(e, &e->groups[20], &e->groups[18]);
			break;
		case 19:
			replace_group(e, &e->groups[19], &e->groups[18]);
			break;
		case 9:
			g->flags |= 0xf0000000;
			break;
		case 18:
			break;
		default:
			if(g->flags & 0xf0000000)
			{
				replace_group(e, &e->groups[8], &e->groups[17]);
				break;
			}
			g->flags |= 0xf0000000;
			break;
		}
	}
	if(!(e->ai_state & 0x6000))
	{
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			award_points(p->index, 500);
			play_enemy_sound(e, 8, "fly", 8);
			e->killer = p;
			mark_die(obj->id, p);
			e->ai_state = FLIE_DYING;
			e->frame = 0x35;
			e->flags &= 0xfffd;
		}
		else
		{
			award_points(p->index, 50);
			play_enemy_sound(e, randrng(6) + 2, "fly", 2);
		}
	}
	if(!(e->ai_state & 0xe000))
	{
		start_hit_reaction(e, 0x13, FLIE_HIT, submesh_info_flie, 9);
	}
	return(0);
}

/* 0x801233e4 */
void load_flie_texmap(void)
{
	load_texmap(flie_texmap, FLIE_TEXMAPS, "flie");
}

/* 0x80123414: the model, animation and path; four submeshes (the damaged wings and body) take
   their animation from the whole ones instead of their own */
void load_flie(void)
{
	load_model((void **)&flie_verts, FLIE_NVERTS, (void **)&flie_faces, FLIE_NFACES, (void **)&flie_normals, FLIE_NNORMALS, "FLIE.ZM", 0x72fb9192);
	load_anim(submesh_info_flie, FLIE_GROUPS, FLIE_FRAMES, "flie.za", 0xdb2b585d);
	load_path(&flie_path, FLIE_PATH_LEN, "flie.pth", 0x37443326);
	free(submesh_info_flie[17].anim);
	free(submesh_info_flie[20].anim);
	free(submesh_info_flie[19].anim);
	free(submesh_info_flie[18].anim);
	submesh_info_flie[17].anim = submesh_info_flie[8].anim;
	submesh_info_flie[20].anim = submesh_info_flie[7].anim;
	submesh_info_flie[19].anim = submesh_info_flie[7].anim;
	submesh_info_flie[18].anim = submesh_info_flie[7].anim;
}

/* 0x8012352c: the assets go when the level is unloaded (the shared animations are let go first) */
void remove_flie_assets(void)
{
	submesh_info_flie[17].anim = 0;
	submesh_info_flie[20].anim = 0;
	submesh_info_flie[19].anim = 0;
	submesh_info_flie[18].anim = 0;
	remove_surface(flie_texmap, FLIE_TEXMAPS);
	remove_path(&flie_path, FLIE_PATH_LEN, "flie.pth");
	remove_animation(submesh_info_flie, FLIE_GROUPS, FLIE_FRAMES, "flie.za");
	remove_model((void **)&flie_verts, FLIE_NVERTS, (void **)&flie_faces, FLIE_NFACES, (void **)&flie_normals, FLIE_NNORMALS, "FLIE.ZM");
}

/* 0x801235ec: the init callback: the record, health 1.5, the 23 groups (the wings 2 and 11
   translucent, the damaged parts hidden, the hat 10 nine times in ten) */
int flie_init(game_object_t *obj)
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
	obj->hit = flie_hit;
	obj->die = flie_die;
	obj->remove = flie_remove;
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
	e->ry = 3.1415927f;
	e->x = 11.0f;
	e->z = 30.0f;
	e->unkcc = 1.0f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 1.5f);
	e->groups = malloc(FLIE_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < FLIE_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[11].flags |= 8;
	e->groups[11].flags |= 0x80;
	e->groups[11].alpha = 128.0f;
	e->groups[2].flags |= 8;
	e->groups[2].flags |= 0x80;
	e->groups[2].alpha = 128.0f;
	e->groups[20].flags = 0;
	e->groups[19].flags = 0;
	e->groups[18].flags = 0;
	e->groups[17].flags = 0;
	e->groups[3].flags = 0;
	if(randrng(100) < 90)
	{
		e->groups[10].flags = 0;
	}
	init_groups(e, submesh_info_flie, FLIE_GROUPS);
	g = e->groups;
	e->hit_group = &g[FLIE_BODY];
	g[FLIE_BODY].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	flie_setup(e, 0);
	play_enemy_sound(e, 1, "fly", -2);
	return(0);
}

/* 0x80123824: the remove callback */
void flie_remove(game_object_t *obj)
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

/* 0x8012387c: the think callback: the effects, loose parts, then the state */
void flie_think(game_object_t *obj)
{
	enemy_t	*e;

	if(debug_mode & 0x10000)
	{
		return;
	}
	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, FLIE_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case FLIE_PATH:
		if(++e->frame >= FLIE_FLY_FRAMES)
		{
			e->frame = 0;
		}
		if(++e->path < e->unk92)
		{
			path_place(e, flie_path, e->path, 1.0f);
			break;
		}
		flie_approach(e);
		break;
	case FLIE_CIRCLE:
		if(++e->frame >= FLIE_FLY_FRAMES)
		{
			func_80123f24(e);
		}
		break;
	case FLIE_PATH_SWAT:
		if(++e->frame >= FLIE_FLY_FRAMES)
		{
			e->frame = 0;
		}
		path_place(e, flie_path, e->path, 1.0f);
		if(++e->path >= e->unk92)
		{
			if(group_near_camera(&e->groups[7], 15.0f))
			{
				hurt_player(e->target, FLIE_SWAT, 3, 0.0f);
			}
			remove_die(obj->id, 0);
		}
		break;
	case FLIE_HIT:
		if(++e->frame >= 0x24)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_flie, 9, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case FLIE_APPROACH:
		if(++e->frame >= FLIE_FLY_FRAMES)
		{
			e->frame = 0;
		}
		turn_toward_camera(e, 0.6f);
		flie_approach(e);
		break;
	case FLIE_ATTACK:
		turn_toward_camera(e, 0.6f);
		if(++e->frame == 0x6e && group_near_camera(&e->groups[21], 15.0f))
		{
			hurt_player(e->target, FLIE_SWAT, 1, 0.0f);
		}
		if(e->frame == 0x79 && group_near_camera(&e->groups[22], 15.0f))
		{
			hurt_player(e->target, FLIE_SWAT, 1, 3.1415927f);
		}
		if(e->frame >= 0x7f)
		{
			flie_approach(e);
		}
		break;
	case FLIE_STOP:
		e->frame++;
		turn_toward_camera(e, 0.6f);
		if(e->frame >= FLIE_FLY_FRAMES)
		{
			flie_approach(e);
		}
		break;
	case FLIE_IDLE:
		if(++e->frame >= FLIE_FRAMES)
		{
			e->frame = 0;
		}
		break;
	case FLIE_FALL:
		if(++e->frame >= 0x68)
		{
			e->frame = 0x5e;
		}
		break;
	case 1:
	case 0x104:
		if(++e->frame >= FLIE_FLY_FRAMES)
		{
			e->frame = 0;
		}
		break;
	case FLIE_DYING:
		if(++e->frame >= 0x46)
		{
			e->frame--;
			e->ai_state = FLIE_DEAD;
		}
		break;
	case FLIE_DEAD:
		break;
	}
}

/* 0x80123cd4: the spawn table's state; the path states start where the entry says, 0x8103
   lies at frame 94 turned a quarter */
void flie_setup(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	switch(state)
	{
	case FLIE_PATH:
		path_place(e, flie_path, e->path, 1.0f);
		e->frame = 0;
		break;
	case FLIE_PATH_SWAT:
	case FLIE_CIRCLE:
		path_place(e, flie_path, e->path, 1.0f);
		e->frame = 0;
		break;
	case FLIE_FALL:
		e->frame = 0x5e;
		e->rx = 1.570796f;
		break;
	default:
		e->frame = 0;
		break;
	}
}

/* 0x80123d68: come for the camera: the target becomes a playing player if it was not, the goal
   a point 1 to the side, 2 up and 11 in front of the camera; the step towards it picks the state:
   still flying (0x100), arrived (0x102) or within reach (0x201, the swats) */
void flie_approach(enemy_t *e)
{
	player_stats_t	*p;

	if(e->target->state != 4)
	{
		p = get_target_player();
		if(p->state == 4)
		{
			e->target = p;
		}
	}
	set_goal_near_camera(e, &camera_x, e->goal, 1.0f, 2.0f, -11.0f);
	switch(step_toward_goal(e, 2.2f))
	{
	case 1:
		if(e->ai_state == FLIE_STOP)
		{
			e->frame = 0;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_flie, 9, 0, 2);
		e->ai_state = FLIE_STOP;
		break;
	case 2:
		if(e->ai_state & 0x200)
		{
			e->frame = 0x68;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_flie, 9, 0x68, 2);
		e->ai_state = FLIE_ATTACK;
		break;
	case 0:
		if(e->ai_state == FLIE_APPROACH)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_flie, 9, 0, 2);
		e->ai_state = FLIE_APPROACH;
		break;
	}
}

/* 0x80123f24: after a loop on the spot: back onto the path if the entry said 0x801 (once),
   else come for the camera */
void func_80123f24(enemy_t *e)
{
	if((unsigned short)e->unk6 == FLIE_PATH)
	{
		e->ai_state = FLIE_PATH;
		path_place(e, flie_path, e->path, 1.0f);
		e->frame = 0;
		e->unk6 = 0;
		return;
	}
	flie_approach(e);
}
