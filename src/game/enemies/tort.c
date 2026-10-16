/*
 * tort.c - the Freak Show's "tort" (object type 0x2024, assets TORT.ZM / tort.za / tort.pth,
 * the tort*.WMS texmaps and the "tort" sound bank). One original file, 0x8011bf38-0x8011dd7c
 * (the word after it is padding), its tables at 0x80196670 (texmaps), 0x80196bb0 (submeshes) and
 * 0x80196e50 (the spray vectors, tort_data.c). The model has 0x2c6 vertices, 0x48f faces and
 * 0x2cd normals in 21 submeshes, the animation 0x231 frames; four submeshes (15, 17-20) reuse
 * other parts' animation (load_tort).
 *
 * A humanoid freak that walks at the player and fights with its hands, often holding a sword,
 * hook or blade (init: 45%), which it can throw (0x203). Spawned with 0x100 or 0x80 it slides
 * along a fixed heading each frame, the 0x80 one bleeding and calling out ("freaksho" cue
 * 0x2d) on the 0x5e'th. Shot, it plays a hurt cue, takes the damage and reacts by the part hit;
 * at the highest violence its arms come off (tort_lose_right_arm, tort_lose_left_arm) and the parts in the
 * damage table turn gory. Dead it falls (0x2001, or 0x2002 when spawned with 0x100), 500 points.
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

/* weapons.h is not included: it brings effects.h, whose burst_blood takes two arguments, and
   this file passes the original's third (0x800000), as teen.c does */
#define WEAPON_SWORD	0x10			/* weapons.h: Freak Show SWRD.ZM ... */
#define WEAPON_HOOK	0x11			/* HOOK.ZM */
#define WEAPON_BLADE	0x12			/* BLAD.ZM */

extern void	draw_effects(enemy_t *);
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	attach_effect(enemy_t *, void *);
extern void	*attach_flame(float, float, group_t *);
extern void	*attach_spray(float *, float *, group_t *);
extern void	detach_effect(enemy_t *, group_t *);
extern void	remove_effects_of_type(enemy_t *, int);
extern void	*attach_held_object(group_t *, int, float, float, float, float, float, float, float *);
extern void	throw_at_camera(float *, int, float *);
extern void	detach_group(group_t *, float *, float, float *);
extern void	replace_group(enemy_t *, group_t *, group_t *);
extern void	pose_group(enemy_t *, submesh_t *, int);	/* model.c: put a part back at its frame pose */
extern void	animate_loose_groups(enemy_t *, int);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, float *, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern int	randrng(int);
extern int	face_camera(enemy_t *, float);
extern int	group_near_camera(group_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal_xz(enemy_t *, float);
extern void	path_place(enemy_t *, void *, short, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern int	mark_die();
extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern enemy_t	*debug_enemy;
extern char	text_buffer[];
extern int	violence_level;
extern player_stats_t player_stats[2];

extern texmap_t	tort_texmap[42];		/* tort_data.c */
extern submesh_t submesh_info_tort[21];
extern float	tort_neck_spray_dir[3], tort_neck_spray_base[3], tort_larm_spray_dir[3], tort_larm_spray_base[3], tort_rarm_spray_dir[3], tort_rarm_spray_base[3];

float	*tort_verts;			/* 0x801e5bf4 */
face_t	*tort_faces;			/* 0x802372b4 */
float	*tort_normals;			/* 0x801e5b70 */
void	*tort_path;			/* 0x80237440 */
extern int	tort_hits;			/* tort_data.c: shots taken by every tort, its parity picks the reaction */

#define TORT_GROUPS	21
#define TORT_NVERTS	0x2c6
#define TORT_NFACES	0x48f
#define TORT_NNORMALS	0x2cd
#define TORT_FRAMES	0x231
#define TORT_PATH_LEN	0x144
#define TORT_HAND	3		/* the right hand: the held weapon and most blows */
#define TORT_HIT	6		/* the submesh it is hit through and measured by */
#define TORT_LHAND	12		/* the left hand: the other held weapon */
#define TORT_REACH	15.0f		/* a blow lands when the hand is this near the player */
#define TORT_BLOW	0x104		/* the damage of a blow */
#define HURT_STAB	3		/* hurt_player kinds: the plain mark ... */
#define HURT_SLASH	7		/* ... and the slash, mirrored when param is set */

/* the states */
#define TORT_LOOP	0		/* plays the whole animation round */
#define TORT_STAND	1		/* frames 0x19-0x31 on the spot */
#define TORT_APPROACH	2		/* frames 0x19-0x31, steering at the player */
#define TORT_WAIT	3		/* frames 0x19-0x31 facing the player for 0xa0 frames, then goes */
#define TORT_LOOP4	4		/* loops 0x1f8-0x201 */
#define TORT_RUN	5		/* frames 0x32-0x81, steering at the player */
#define TORT_STRIKE	0x201		/* both hands, 0x171-0x189 (a hit at 0x17b) */
#define TORT_STAB	0x202		/* 0x1ec-0x1f7 (a hit at 0x1fa) */
#define TORT_THROW	0x203		/* throws its sword at 0x20e, 0x202-0x217 */
#define TORT_SWIPE	0x204		/* 0-0x18 (a slash at 9) */
#define TORT_COMBO	0x205		/* 0x127-0x152, four slashes */
#define TORT_COMBO2	0x206		/* 0x1a3-0x1bb, a slash with each hand */
#define TORT_PATH	0x801		/* runs its path (frames 0x32-0x81) */
#define TORT_DIE1	0x2001		/* falls, 0xde-0xf7 */
#define TORT_DIE2	0x2002		/* falls, 0x153-0x170 */
#define TORT_GONE	0x4000
#define TORT_LOOP8401	0x8401		/* loops 0x115-0x125 */
#define TORT_LOOP8402	0x8402		/* loops 0x18a-0x1a2 */

int	tort_die(game_object_t *, player_stats_t *);
void	tort_remove(game_object_t *);
void	tort_setup(enemy_t *, int);
void	tort_approach(enemy_t *);
void	tort_end_run(enemy_t *);
void	tort_lose_right_arm(enemy_t *);
void	tort_lose_left_arm(enemy_t *);

/* 0x8011bf38: the draw callback */
void tort_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(0x20);
	draw_model(e, submesh_info_tort, TORT_GROUPS, tort_verts, TORT_NVERTS, tort_faces, TORT_NFACES, tort_normals, TORT_NNORMALS);
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

/* 0x8011c144: the hit test: the bounding sphere, then the submesh */
int tort_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_tort, TORT_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)tort_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x8011c200: the shot lands: blood; the flame thrower sets the part alight; at the highest
   violence the arms come off (shoulders 1-3 the right arm, 0/11/12 the left), the neck (14, 19)
   turns gory then is swapped, part 8 is swapped for 20 on its second shot, and the rest turn
   gory; then a hurt cue and the damage (death 500 points, else 50) and a hit reaction by part */
int tort_die(game_object_t *obj, player_stats_t *p)
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
	burst_blood(p->hit_point, e->y, 0x800000);
	add_chunks(e->hit_group->pos, e->y);
	add_world_sprite(p->hit_point, 0, 0);
	group = p->hit_group;
	g = &e->groups[group];
	if(p->shot_weapon == 0x1000004)
	{
		if(!e->unkac)
		{
			remove_effects_of_type(e, 1);
		}
		e->unkac = 1;
		if(!(g->flags & 2))
		{
			g->flags |= 2;
			attach_effect(e, attach_flame(0.0f, (submesh_info_tort[group].radius + submesh_info_tort[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 1:
		case 2:
		case 3:
			tort_lose_right_arm(e);
			break;
		case 0:
		case 11:
		case 12:
			tort_lose_left_arm(e);
			break;
		case 14:
		case 19:
			e->unka8++;
			switch(e->unka8)
			{
			case 0:
				printf("This should never happen! Zomb head damage at 0\n");
				break;
			case 1:
				g = &e->groups[group];
				g->flags |= 0xf0000000;
				e->groups[7].flags |= 0xf0000000;
				break;
			case 2:
				replace_group(e, &e->groups[14], &e->groups[19]);
				if(!e->unkac)
				{
					attach_effect(e, attach_spray(tort_neck_spray_base, tort_neck_spray_dir, &e->groups[19]));
				}
				break;
			}
			break;
		case 8:
			if(g->flags & 0xf0000000)
			{
				replace_group(e, &e->groups[8], &e->groups[20]);
				break;
			}
			g->flags |= 0xf0000000;
			break;
		default:
			g->flags |= 0xf0000000;
			break;
		}
	}
	if(!(e->ai_state & 0x6000))
	{
		play_enemy_sound(e, randrng(10) + 1, "tort", 2);
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			award_points(p->index, 500);
			e->killer = p;
			mark_die(obj->id, p);
			if(!(e->ai_state & 0x400))
			{
				if(e->flags2 & 0x100)
				{
					e->ai_state = TORT_DIE2;
					e->frame = 0x153;
					e->flags &= 0xfffd;
				}
				else
				{
					e->ai_state = TORT_DIE1;
					e->frame = 0xde;
					e->flags &= 0xfffd;
				}
			}
		}
		else
		{
			award_points(p->index, 50);
		}
	}
	if(e->ai_state & 0xe000)
	{
		return(0);
	}
	tort_hits++;
	switch(group)
	{
	case 6:
		start_hit_reaction(e, 0xf8, 0x1007, submesh_info_tort, 6);
		break;
	case 7:
	case 8:
	case 14:
	case 15:
	case 19:
	case 20:
		if(tort_hits & 1)
		{
			start_hit_reaction(e, 0x1bc, 0x1001, submesh_info_tort, 6);
		}
		else
		{
			start_hit_reaction(e, 0x1cf, 0x1002, submesh_info_tort, 6);
		}
		break;
	case 0:
	case 10:
	case 11:
	case 12:
	case 13:
	case 16:
	case 17:
		if(tort_hits & 1)
		{
			start_hit_reaction(e, 0xb4, 0x1003, submesh_info_tort, 6);
		}
		else
		{
			start_hit_reaction(e, 0xc9, 0x1004, submesh_info_tort, 6);
		}
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 9:
	case 18:
		if(tort_hits & 1)
		{
			start_hit_reaction(e, 0x82, 0x1005, submesh_info_tort, 6);
		}
		else
		{
			start_hit_reaction(e, 0x9d, 0x1006, submesh_info_tort, 6);
		}
		break;
	}
	return(0);
}

/* 0x8011c69c */
void load_tort_texmap(void)
{
	load_texmap(tort_texmap, 0x2a, "tort");
}

/* 0x8011c6cc: the model, the animation and the path; parts 15 and 17-20 (the damaged and
   swapped-in versions) are moved by the animation of parts 8, 0, 1, 14 and 8 */
void load_tort(void)
{
	load_model((void **)&tort_verts, TORT_NVERTS, (void **)&tort_faces, TORT_NFACES, (void **)&tort_normals, TORT_NNORMALS, "TORT.ZM", 0x3f877f5f);
	load_anim(submesh_info_tort, TORT_GROUPS, TORT_FRAMES, "tort.za", (int)0xa24f4ebf);
	load_path(&tort_path, TORT_PATH_LEN, "tort.pth", 0x3d621da8);
	free(submesh_info_tort[15].anim);
	free(submesh_info_tort[17].anim);
	free(submesh_info_tort[18].anim);
	free(submesh_info_tort[19].anim);
	free(submesh_info_tort[20].anim);
	submesh_info_tort[15].anim = submesh_info_tort[8].anim;
	submesh_info_tort[17].anim = submesh_info_tort[0].anim;
	submesh_info_tort[18].anim = submesh_info_tort[1].anim;
	submesh_info_tort[19].anim = submesh_info_tort[14].anim;
	submesh_info_tort[20].anim = submesh_info_tort[8].anim;
}

/* 0x8011c80c: the assets go when the level is unloaded */
void remove_tort_assets(void)
{
	submesh_info_tort[15].anim = 0;
	submesh_info_tort[17].anim = 0;
	submesh_info_tort[18].anim = 0;
	submesh_info_tort[19].anim = 0;
	submesh_info_tort[20].anim = 0;
	remove_surface(tort_texmap, 0x2a);
	remove_path(&tort_path, TORT_PATH_LEN, "tort.pth");
	remove_animation(submesh_info_tort, TORT_GROUPS, TORT_FRAMES, "tort.za");
	remove_model((void **)&tort_verts, TORT_NVERTS, (void **)&tort_faces, TORT_NFACES, (void **)&tort_normals, TORT_NNORMALS, "TORT.ZM");
}

/* 0x8011c8d4: the init callback: 7.5 health at 0.9 scale, the four spare parts (17-20)
   hidden, and by chance a weapon in a hand: a hook (10% right, 5% left), a blade (10% right,
   5% left) or a sword (10% right, 5% left) */
int tort_init(game_object_t *obj)
{
	enemy_t	*e;
	group_t	*g;
	void	*data;
	int	i;
	int	r;

	data = malloc(sizeof(enemy_t));
	obj->data = data;
	if(!data)
	{
		return(1);
	}
	obj->hit = tort_hit;
	obj->die = tort_die;
	obj->remove = tort_remove;
	e = (enemy_t *)data;
	e->unk4 = 0x14;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0x2c;
	e->target = get_target_player();
	e->unk54 = 0;
	e->rx = 0;
	e->rz = 0;
	e->x = 0;
	e->y = 0;
	e->z = 0;
	e->ry = 3.1415927f;
	e->unkcc = 1.0f;
	set_enemy_scale(e, 0.9f);
	set_enemy_health(e, 7.5f);
	e->groups = malloc(TORT_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < TORT_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[17].flags = 0;
	e->groups[18].flags = 0;
	e->groups[19].flags = 0;
	e->groups[20].flags = 0;
	e->groups[6].flags |= 0x40;
	e->groups[20].flags |= 0x40;
	e->groups[17].flags |= 0x40;
	e->groups[18].flags |= 0x40;
	e->unka8 = 0;
	e->unkac = 0;
	init_groups(e, submesh_info_tort, TORT_GROUPS);
	g = e->groups;
	e->hit_group = &g[TORT_HIT];
	g[TORT_HIT].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	e->flags2 = 0;
	r = randrng(100);
	if(r < 10)
	{
		attach_effect(e, attach_held_object(&e->groups[TORT_HAND], WEAPON_HOOK, 0.32f, 0.0f, 0.0f, 1.5f, -1.5f, 0.0f, e->scale));
	}
	else if(r < 15)
	{
		attach_effect(e, attach_held_object(&e->groups[TORT_LHAND], WEAPON_HOOK, 0.3f, 0.05f, -0.06f, -1.6f, 1.7f, 0.0f, e->scale));
	}
	else if(r < 25)
	{
		attach_effect(e, attach_held_object(&e->groups[TORT_HAND], WEAPON_BLADE, 0.24f, -0.01f, 0.0f, 1.9f, -4.7f, 0.0f, e->scale));
	}
	else if(r < 30)
	{
		attach_effect(e, attach_held_object(&e->groups[TORT_LHAND], WEAPON_BLADE, 0.22f, 0.01f, -0.06f, 1.5f, 1.9f, 0.0f, e->scale));
	}
	else if(r < 40)
	{
		attach_effect(e, attach_held_object(&e->groups[TORT_HAND], WEAPON_SWORD, 0.14f, 0.03f, 0.0f, -1.7f, -3.0f, 0.0f, e->scale));
	}
	else if(r < 45)
	{
		attach_effect(e, attach_held_object(&e->groups[TORT_LHAND], WEAPON_SWORD, 0.1f, 0.0f, -0.03f, -1.8f, 2.9f, 0.0f, e->scale));
	}
	tort_setup(e, 0);
	return(0);
}

/* 0x8011ccc0: the remove callback */
void tort_remove(game_object_t *obj)
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

/* a hit reaction plays to its last frame, then it blends back to what it was doing */
#define HIT_REACTION(last) \
	frame = e->frame; \
	e->frame = frame + 1; \
	if(e->frame < last) \
	{ \
		break; \
	} \
	e->frame = frame; \
	blend_to_frame(e, submesh_info_tort, 6, e->resume_frame, 5); \
	e->ai_state = e->resume_state; \
	break;

/* a fall plays to its last frame, then it is gone */
#define FALL(last) \
	frame = e->frame; \
	e->frame = frame + 1; \
	if(e->frame < last) \
	{ \
		break; \
	} \
	e->frame = frame; \
	e->ai_state = TORT_GONE; \
	break;

/* 0x8011cd18: the think callback */
void tort_think(game_object_t *obj)
{
	enemy_t	*e;
	short	frame;

	if(debug_mode & 0x10000)
	{
		return;
	}
	e = (enemy_t *)obj->data;
	think_effects(e);
	animate_loose_groups(e, TORT_GROUPS);
	if(player_stats[0].state == 4 || player_stats[1].state == 4)
	{
		if(e->flags2 & 0x100)
		{
			e->x += -0.84433f;
			e->z += 0.53582f;
		}
		else if(e->flags2 & 0x80)
		{
			e->x += 0.53582f;
			e->z += 0.84433f;
			if(++e->unkb0 == 0x5e)
			{
				play_rotating_cue_flags(0x2d, "freaksho", 1);
				play_enemy_sound(e, randrng(10) + 1, "tort", 0xc);
			}
			if(e->unkb0 >= 0x5f && (e->unkb0 & 3))
			{
				burst_blood(e->hit_group->pos, e->y, 0x800000);
				add_chunks(e->hit_group->pos, e->y);
				add_world_sprite(e->hit_group->pos, 0, 0);
			}
		}
	}
	if(e->flags & 2)
	{
		return;
	}
	if(debug_mode & 0x20000)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1001:
		HIT_REACTION(0x1cf)
	case 0x1002:
		HIT_REACTION(0x1ec)
	case 0x1005:
		HIT_REACTION(0x9d)
	case 0x1006:
		HIT_REACTION(0xb4)
	case 0x1003:
		HIT_REACTION(0xc9)
	case 0x1004:
		HIT_REACTION(0xde)
	case 0x1007:
		HIT_REACTION(0x115)
	case TORT_RUN:
		if(++e->frame >= 0x82)
		{
			e->frame = 0x32;
		}
		face_camera(e, 0.2f);
		tort_approach(e);
		break;
	case TORT_SWIPE:
		face_camera(e, 0.2f);
		if(++e->frame == 9 && group_near_camera(&e->groups[TORT_HAND], TORT_REACH))
		{
			hurt_player(e->target, TORT_BLOW, HURT_SLASH, 3.1415927f);
		}
		if(e->frame < 0x19)
		{
			break;
		}
		tort_approach(e);
		break;
	case TORT_COMBO:
		face_camera(e, 0.2f);
		if(++e->frame == 0x134 && group_near_camera(&e->groups[TORT_HAND], TORT_REACH))
		{
			hurt_player(e->target, TORT_BLOW, HURT_SLASH, 3.1415927f);
		}
		if(e->frame == 0x13c && group_near_camera(&e->groups[TORT_HAND], TORT_REACH))
		{
			hurt_player(e->target, TORT_BLOW, HURT_SLASH, 0.0f);
		}
		if(e->frame == 0x146 && group_near_camera(&e->groups[TORT_HAND], TORT_REACH))
		{
			hurt_player(e->target, TORT_BLOW, HURT_SLASH, 3.1415927f);
		}
		if(e->frame == 0x14d && group_near_camera(&e->groups[TORT_HAND], TORT_REACH))
		{
			hurt_player(e->target, TORT_BLOW, HURT_SLASH, 0.0f);
		}
		if(e->frame < 0x153)
		{
			break;
		}
		tort_approach(e);
		break;
	case TORT_COMBO2:
		face_camera(e, 0.2f);
		if(++e->frame == 0x1ae && group_near_camera(&e->groups[TORT_HAND], TORT_REACH))
		{
			hurt_player(e->target, TORT_BLOW, HURT_SLASH, 3.1415927f);
		}
		if(e->frame == 0x1b8 && group_near_camera(&e->groups[TORT_LHAND], TORT_REACH))
		{
			hurt_player(e->target, TORT_BLOW, HURT_SLASH, 0.0f);
		}
		if(e->frame < 0x1bc)
		{
			break;
		}
		tort_approach(e);
		break;
	case TORT_STRIKE:
		face_camera(e, 0.2f);
		if(++e->frame == 0x17b)
		{
			if(group_near_camera(&e->groups[14], TORT_REACH) || group_near_camera(&e->groups[19], TORT_REACH))
			{
				hurt_player(e->target, TORT_BLOW, HURT_STAB, 0.0f);
			}
		}
		if(e->frame < 0x18a)
		{
			break;
		}
		tort_approach(e);
		break;
	case TORT_APPROACH:
		if(++e->frame >= 0x32)
		{
			e->frame = 0x19;
		}
		face_camera(e, 0.2f);
		tort_approach(e);
		break;
	case TORT_THROW:
		face_camera(e, 0.2f);
		if(++e->frame == 0x20e && (e->groups[TORT_HAND].flags & 0x2000))
		{
			throw_at_camera(e->groups[TORT_HAND].pos, WEAPON_SWORD, e->scale);
			detach_effect(e, &e->groups[TORT_HAND]);
		}
		if(e->frame < 0x218)
		{
			break;
		}
		tort_end_run(e);
		break;
	case TORT_STAB:
		face_camera(e, 0.2f);
		if(++e->frame == 0x1fa && group_near_camera(&e->groups[TORT_HAND], TORT_REACH))
		{
			hurt_player(e->target, TORT_BLOW, HURT_STAB, 0.0f);
		}
		if(e->frame < 0x1f8)
		{
			break;
		}
		tort_end_run(e);
		break;
	case TORT_PATH:
		if(++e->frame >= 0x82)
		{
			e->frame = 0x32;
		}
		path_place(e, tort_path, e->path, 1.0f);
		if(++e->path <= e->unk92)
		{
			break;
		}
		tort_end_run(e);
		break;
	case TORT_WAIT:
		if(++e->frame >= 0x32)
		{
			e->frame = 0x19;
		}
		face_camera(e, 0.2f);
		if(++e->counter <= 0xa0)
		{
			break;
		}
		tort_end_run(e);
		break;
	case TORT_STAND:
		if(++e->frame >= 0x32)
		{
			e->frame = 0x19;
		}
		break;
	case TORT_LOOP8401:
		if(++e->frame >= 0x126)
		{
			e->frame = 0x115;
		}
		break;
	case TORT_LOOP4:
		if(++e->frame >= 0x202)
		{
			e->frame = 0x1f8;
		}
		break;
	case TORT_LOOP8402:
		if(++e->frame >= 0x1a3)
		{
			e->frame = 0x18a;
		}
		break;
	case TORT_DIE1:
		FALL(0xf8)
	case TORT_DIE2:
		FALL(0x171)
	case TORT_LOOP:
		if(++e->frame >= TORT_FRAMES)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x8011d704: the set-up: the state bits 0x100 (slide one way), 0x80 (slide the other, bleeding)
   and 0x40 (a sword in the right hand) go into flags2, then the state's first frame */
void tort_setup(enemy_t *e, int state)
{
	e->ai_state = state;
	if(state & 0x100)
	{
		e->ai_state = state & 0xfeff;
		e->flags2 |= 0x100;
	}
	if(e->ai_state & 0x80)
	{
		e->ai_state &= 0xff7f;
		e->unkb0 = 0;
		e->flags2 |= 0x80;
	}
	if(e->ai_state & 0x40)
	{
		e->ai_state &= 0xffbf;
		e->flags2 |= 0x40;
		detach_effect(e, &e->groups[TORT_HAND]);
		attach_effect(e, attach_held_object(&e->groups[TORT_HAND], WEAPON_SWORD, 0.14f, 0.03f, 0.0f, -1.7f, -3.0f, 0.0f, e->scale));
	}
	switch(e->ai_state)
	{
	case TORT_THROW:
		e->frame = 0x202;
		break;
	case TORT_STAB:
		e->frame = 0x1ec;
		break;
	case TORT_PATH:
		e->frame = 0x32;
		path_place(e, tort_path, e->path, 1.0f);
		break;
	case TORT_LOOP8401:
		e->frame = 0x115;
		detach_effect(e, &e->groups[TORT_LHAND]);
		detach_effect(e, &e->groups[TORT_HAND]);
		break;
	case TORT_LOOP4:
		e->frame = 0x1f8;
		break;
	case TORT_LOOP8402:
		e->frame = 0x18a;
		detach_effect(e, &e->groups[TORT_LHAND]);
		detach_effect(e, &e->groups[TORT_HAND]);
		break;
	case TORT_RUN:
		e->frame = 0x32;
		break;
	case TORT_WAIT:
		e->frame = 0x19;
		e->counter = 100;
		break;
	case TORT_APPROACH:
	default:
		e->frame = 0;
		break;
	}
}

/* 0x8011d910: go for the player: keep a playing target, steer to a point in front of the
   camera and pick the state by how close it got (0 still far: run, 1 there: approach on the
   spot, 2 in reach: a strike, or with a weapon in hand a swipe, combo or two-handed combo) */
void tort_approach(enemy_t *e)
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
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0.0f, -12.0f);
	r = step_toward_goal_xz(e, 0.5f);
	switch(r)
	{
	case 1:
		if(e->ai_state == TORT_APPROACH)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_tort, 6, 0x19, 5);
		e->ai_state = TORT_APPROACH;
		break;
	case 2:
		if(e->frame > 0)
		{
			e->frame--;
		}
		if(!(e->groups[TORT_HAND].flags & 0x2000))
		{
			blend_to_frame(e, submesh_info_tort, 6, 0x171, 5);
			e->ai_state = TORT_STRIKE;
			break;
		}
		r = randrng(100);
		if(r < 25)
		{
			blend_to_frame(e, submesh_info_tort, 6, 0, 5);
			e->ai_state = TORT_SWIPE;
		}
		else if(r < 50)
		{
			blend_to_frame(e, submesh_info_tort, 6, 0x127, 5);
			e->ai_state = TORT_COMBO;
		}
		else if(r < 75)
		{
			blend_to_frame(e, submesh_info_tort, 6, 0x1a3, 5);
			e->ai_state = TORT_COMBO2;
			e->counter = 0;
		}
		else
		{
			blend_to_frame(e, submesh_info_tort, 6, 0x171, 5);
			e->ai_state = TORT_STRIKE;
		}
		break;
	case 0:
		if(e->ai_state == TORT_RUN)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_tort, 6, 0x32, 5);
		e->ai_state = TORT_RUN;
		break;
	}
}

/* 0x8011db74: at the end of a run or a throw: the spawn entry's 1 makes it stand, else it goes
   for the player again */
void tort_end_run(enemy_t *e)
{
	if((unsigned short)e->unk6 == 1)
	{
		e->unk6 = 0;
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_tort, 6, 0x19, 5);
		e->ai_state = TORT_STAND;
		return;
	}
	e->unk6 = 0;
	tort_approach(e);
}

/* 0x8011dbf4: the right arm comes off (once): the upper arm (1) is swapped for its stump (18),
   which is put at its frame pose and sprays blood; the hand (3) and forearm (2) fall */
void tort_lose_right_arm(enemy_t *e)
{
	if(e->groups[18].flags & GROUP_VISIBLE)
	{
		return;
	}
	replace_group(e, &e->groups[1], &e->groups[18]);
	pose_group(e, submesh_info_tort, 18);
	if(!e->unkac)
	{
		attach_effect(e, attach_spray(tort_rarm_spray_base, tort_rarm_spray_dir, &e->groups[18]));
	}
	detach_group(&e->groups[3], e->groups[2].pos, e->y, e->scale);
	detach_group(&e->groups[2], e->groups[2].pos, e->y, e->scale);
}

/* 0x8011dcb8: the left arm comes off (once): the upper arm (0) is swapped for its stump (17),
   the hand (12) and forearm (11) fall */
void tort_lose_left_arm(enemy_t *e)
{
	if(e->groups[17].flags & GROUP_VISIBLE)
	{
		return;
	}
	replace_group(e, &e->groups[0], &e->groups[17]);
	pose_group(e, submesh_info_tort, 18);
	if(!e->unkac)
	{
		attach_effect(e, attach_spray(tort_larm_spray_base, tort_larm_spray_dir, &e->groups[17]));
	}
	detach_group(&e->groups[12], e->groups[11].pos, e->y, e->scale);
	detach_group(&e->groups[11], e->groups[11].pos, e->y, e->scale);
}
