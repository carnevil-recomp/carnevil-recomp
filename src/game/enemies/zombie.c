/*
 * zombie.c - the Haunted House zombie (object type 0x200c; one original file, 0x800f7988-0x800fa71c,
 * its tables at 0x80190068-0x80190914). ZOMB.ZM: 668 vertices, 747 normals, 1144 faces in 21
 * submeshes; zomb.za 1477 frames; zomb.pth 829 points. A zombie walks at the player and claws
 * or bites, carries one of five held objects when the level asks for them, can walk in carrying
 * a victim (the Haunted House innocent 0x2011 or 0x2012) that it lets go of when hurt, comes
 * apart under the heavy weapons, and dies in one of three ways.
 *
 * The groups the code names: 0, 1 (an arm and its upper part; 4 is the stump that replaces
 * 1), 12 and 13 (the hands that claw and hold), 14, 15 (the other arm; 5 is its stump), 16, 17,
 * 18 (the head pieces), 19 (the neck stump), 20 (the hit group, the body); 2 and 3 are stumps
 * shown when the head or the body go.
 */
#include <stdio.h>
#include <stdlib.h>
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "texmap.h"
#include "credits.h"			/* debug_mode, player_stats_t, violence_level */
#include "levelsnd.h"
#include "camera.h"			/* camera_x */

#ifdef PORT_HOST
#define INLINE
#else
#define INLINE inline
#endif

extern void	draw_effects(enemy_t *);
extern void	*attach_held_object(group_t *, int, float, float, float, float, float, float, float *);
extern void	*attach_spray(float *, float *, group_t *);
extern void	*attach_flame(float, float, group_t *);
extern void	attach_effect(enemy_t *, void *);
extern void	detach_effect(enemy_t *, group_t *);
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	throw_at_camera(float *, int, float *);
extern void	detach_group(group_t *, float *, float, float *);
extern void	hide_group(enemy_t *, group_t *);
extern void	replace_group(enemy_t *, group_t *, group_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern void	pose_groups(enemy_t *, submesh_t *, int);	/* model.c: pose the groups at the current frame */
extern void	pose_group(enemy_t *, submesh_t *, int);	/* model.c: pose one group */
extern void	fling_group(group_t *, float *, float *, float, float *);	/* model.c: knock a group loose with a fling */
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern void	clear_enemy_fields(enemy_t *);
extern void	unlink_enemy(enemy_t *);
extern int	randrng(int);
extern int	face_camera(enemy_t *, float);
extern int	face_point(enemy_t *, float *, float);	/* enemy.c: turn towards a point */
extern int	step_straight_to_goal(enemy_t *, float);		/* enemy.c: step straight at the goal */
extern int	group_near_camera(group_t *, float);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	blend_from_pose(enemy_t *, submesh_t *, int, int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal_xz(enemy_t *, float);
extern void	path_place(enemy_t *, void *, int, float);
extern void	angles_to(float *, float *, float *);
extern void	rot_matrix_zyx(float *, float *), mat_vec_mul(float *, float *, float *);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_chunks_n(float *, float, int);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern int	mark_die();
extern void	add_die_like(short, short);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern int	violence_level;
extern enemy_t	*debug_enemy;
extern int	D_801e5cd4;			/* wave.c sets it for one level's load: the zombies carry things */
/* the Haunted House innocent's objects (0x2011 and 0x2012) and their state set-ups */
extern void	inng_draw(game_object_t *), inng_think(game_object_t *);
extern int	inng_init(game_object_t *);
extern void	vict_draw(game_object_t *), vict_think(game_object_t *);
extern int	vict_init(game_object_t *);
extern void	inng_setup(enemy_t *, int), vict_setup(enemy_t *, int);

extern texmap_t	zombie_texmap[45];		/* zombie_data.c */
extern submesh_t submesh_info_zomb[21];
extern float	zomb_spray_neck[3], zomb_spray_neck_dir[3], zomb_spray_arm5[3], zomb_spray_arm5_dir[3];
extern float	zomb_spray_arm4[3], zomb_spray_arm4_dir[3], zomb_spray_body[3], zomb_spray_body_dir[3];
extern int	zomb_hurt_sounds[3];

float	*zomb_verts;			/* 0x801e5bb0 */
face_t	*zomb_faces;			/* 0x8021f0f8 */
float	*zomb_normals;			/* 0x801e5874 */
void	*zomb_path;			/* 0x802373f8 */

/* the victim a zombie carries in (enemy_t.unkdc) */
#define HELD(e)		((enemy_t *)(e)->unkdc)

int	zombie_die(game_object_t *, player_stats_t *);
void	zombie_damage(game_object_t *, player_stats_t *, float, enemy_t *);
void	func_800f9e8c(enemy_t *), func_800f9f40(enemy_t *), zombie_next_action(enemy_t *), zombie_walk(enemy_t *);
void	zombie_pick_held_object(enemy_t *), func_800fa5b4(enemy_t *), zombie_release_victim(enemy_t *);
void	zombie_setup(enemy_t *, unsigned short);

/* 0x800f7988: the draw callback */
void zombie_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_zomb, 0x15, zomb_verts, 0x29c, zomb_faces, 0x478, zomb_normals, 0x2eb);
	draw_effects(e);
}

/* 0x800f7a00: the hit callback: the shot against the body's sphere, then the submeshes */
int zombie_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pivot, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_zomb, 0x15, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)zombie_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x800f7abc: killed, falling (state 0x402 from frame 852): it drops its victim, 500 points,
   its death cry, the die list. Inlined into zombie_die; this copy has no caller */
INLINE void func_800f7abc(game_object_t *obj, player_stats_t *p, enemy_t *e)
{
	if(!(e->ai_state & 0x400))
	{
		zombie_release_victim(e);
		e->health = 0.0f;
		award_points(p->index, 500);
		play_enemy_sound(e, 0x11, "zombie", 8);
		e->killer = p;
		if(!(debug_mode & 1))
		{
			mark_die(obj->id, p);
		}
		e->ai_state = 0x402;
		e->frame = 0x354;
		e->flags &= ~2;
	}
}

/* 0x800f7b74: killed by the shotgun's head shot (state 0x404 from frame 600): the same, and at
   violence 3 the head goes (group 19 takes group 2's place, func_800f9e8c). Inlined into
   zombie_die; this copy has no caller */
INLINE void func_800f7b74(game_object_t *obj, player_stats_t *p, enemy_t *e)
{
	if(!(e->ai_state & 0x400))
	{
		zombie_release_victim(e);
		e->health = 0.0f;
		award_points(p->index, 500);
		play_enemy_sound(e, 0x11, "zombie", 8);
		if(violence_level >= 3)
		{
			replace_group(e, &e->groups[19], &e->groups[2]);
			func_800f9e8c(e);
		}
		e->killer = p;
		if(!(debug_mode & 1))
		{
			mark_die(obj->id, p);
		}
		e->ai_state = 0x404;
		e->frame = 0x258;
		e->flags &= ~2;
	}
}

/* 0x800f7c5c: damage: when the health runs out it drops its victim, 500 points, its death cry,
   the die list, and one of three deaths: blown apart (one in four at violence 3, state 0x401:
   the body groups go, chunks and a spray from the neck), falling backwards (0x404, also when
   shot while kneeling, state 0x10e) or falling (0x402, also from state 0x103); 50 points
   otherwise */
void zombie_damage(game_object_t *obj, player_stats_t *p, float damage, enemy_t *e)
{
	int	kind;

	if(!(e->ai_state & 0x400))
	{
		e->health = e->health - damage;
		if(e->health <= 0.0f)
		{
			zombie_release_victim(e);
			award_points(p->index, 500);
			play_enemy_sound(e, 0x11, "zombie", 8);
			e->killer = p;
			if(!(debug_mode & 1))
			{
				mark_die(obj->id, p);
			}
			if(randrng(100) < 25)
			{
				kind = 1;
			}
			else
			{
				kind = 2;
			}
			if(e->ai_state == 0x103)
			{
				kind = 2;
			}
			if(e->ai_state == 0x10e)
			{
				kind = 3;
			}
			if(violence_level < 3 && kind == 1)
			{
				kind = 2;
			}
			switch(kind)
			{
			case 1:
				e->ai_state = 0x401;
				e->frame = 0x1cf;
				e->flags &= ~2;
				hide_group(e, &e->groups[5]);
				hide_group(e, &e->groups[15]);
				hide_group(e, &e->groups[14]);
				hide_group(e, &e->groups[13]);
				hide_group(e, &e->groups[4]);
				hide_group(e, &e->groups[1]);
				hide_group(e, &e->groups[0]);
				hide_group(e, &e->groups[12]);
				hide_group(e, &e->groups[16]);
				hide_group(e, &e->groups[17]);
				hide_group(e, &e->groups[18]);
				hide_group(e, &e->groups[19]);
				replace_group(e, &e->groups[20], &e->groups[3]);
				add_chunks_n(e->groups[19].pos, e->y, 10);
				pose_group(e, submesh_info_zomb, 3);
				attach_effect(e, attach_spray(zomb_spray_body, zomb_spray_body_dir, &e->groups[3]));
				break;
			case 3:
				e->ai_state = 0x404;
				e->frame = 0x258;
				e->flags &= ~2;
				break;
			default:
				e->ai_state = 0x402;
				e->frame = 0x354;
				e->flags &= ~2;
				break;
			}
		}
		else
		{
			award_points(p->index, 50);
		}
	}
}

/* 0x800f7f00: the die callback: blood, a hit mark and chunks; a zombie carrying its victim
   (state 0x2000) lets go; at violence 3 the shot takes off the part it hit (an arm, the head
   in up to three shots, the hands), the damage, then unless it is dying a hurt
   cry and a hit reaction chosen by the part */
int zombie_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	enemy_t	*held;
	group_t	*g;
	int	group;
	int	kind;
	int	r;
	unsigned short cur;
	int	state;
	int	frame;

	e = (enemy_t *)obj->data;
	kind = 0;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return(0);
	}
	burst_blood(p->hit_point, 0.0f, 0x800000);
	add_world_sprite(p->hit_point, 0, 0);
	add_chunks(p->hit_point, e->y);
	group = p->hit_group;
	if(e->ai_state == 0x2000)
	{
		held = HELD(e);
		if(held)
		{
			inng_setup(held, 0x400);
			e->unkdc = 0;
			held->unkd8 = 0;
			e->ai_state = 0x101;
			e->frame = 0;
		}
	}
	if(e->ai_state == 0x10b)
	{
		e->ai_state = 0x101;
		e->frame = 0;
	}
	if(violence_level >= 3)
	{
		if(e->ai_state & 0x400)
		{
			goto react;
		}
		switch(group)
		{
		case 20:
			g = &e->groups[group];
			g->flags |= 0xf0000000;
			break;
		case 19:
			if(p->shot_weapon == 2)
			{
				func_800f7b74(obj, p, e);
				break;
			}
			g = &e->groups[group];
			g->flags |= 0xf0000000;
			e->groups[18].flags |= 0xf0000000;
			r = randrng(100);
			if(r >= 1 && r <= 10)
			{
				kind = 1;
				func_800f9f40(e);
			}
			else if(r >= 11 && r <= 20)
			{
				kind = 2;
				func_800f9e8c(e);
			}
			break;
		case 16:
		case 17:
		case 18:
			if(group == 18)
			{
				e->unka8++;
			}
			else
			{
				e->unka8 += randrng(3) + 1;
			}
			switch(e->unka8)
			{
			case 0:
				printf("This should never happen! Zomb head damage = 0.\n");
				break;
			case 1:
				g = &e->groups[group];
				g->flags = (g->flags | 0xf0000000) & ~0x1000000;
				e->groups[18].flags |= 0xf0000000;
				break;
			case 2:
				replace_group(e, &e->groups[16], &e->groups[17]);
				e->groups[18].flags |= 0xf0000000;
				break;
			default:
				e->groups[18].flags |= 0xf0000000;
				if(e->groups[16].flags & GROUP_VISIBLE)
				{
					detach_group(&e->groups[16], e->groups[16].pos, e->y, e->scale);
				}
				else
				{
					detach_group(&e->groups[17], e->groups[17].pos, e->y, e->scale);
				}
				pose_group(e, submesh_info_zomb, 18);
				attach_effect(e, attach_spray(zomb_spray_neck, zomb_spray_neck_dir, &e->groups[18]));
				func_800f7abc(obj, p, e);
				break;
			}
			/* fall through: the head pieces' neighbours go with them */
		case 6:
		case 7:
		case 8:
			e->groups[8].flags |= 0xf0000000;
			e->groups[7].flags |= 0xf0000000;
			break;
		case 9:
		case 10:
		case 11:
			e->groups[11].flags |= 0xf0000000;
			e->groups[10].flags |= 0xf0000000;
			break;
		case 0:
		case 1:
		case 12:
			if(e->groups[1].flags & 0xf0000000)
			{
				func_800f9e8c(e);
			}
			else
			{
				e->groups[1].flags |= 0xf0000000;
				e->groups[0].flags |= 0xf0000000;
			}
			break;
		case 13:
		case 14:
		case 15:
			if(e->groups[15].flags & 0xf0000000)
			{
				func_800f9f40(e);
			}
			else
			{
				e->groups[15].flags |= 0xf0000000;
				e->groups[14].flags |= 0xf0000000;
			}
			break;
		}
	}
	if(!(e->ai_state & 0x400))
	{
		zombie_damage(obj, p, shot_damage_of(p), e);
	}
react:
	cur = e->ai_state;
	if(cur == 0x103)
	{
		return(0);
	}
	if(cur & 0x1000)
	{
		return(0);
	}
	if(cur & 0x400)
	{
		return(0);
	}
	play_enemy_sound(e, zomb_hurt_sounds[randrng(3)], "zombie", 2);
	switch(group)
	{
	case 16:
		state = 0x205;
		frame = 0x124;
		break;
	case 17:
		state = 0x206;
		frame = 0x124;
		break;
	case 20:
		state = 0x207;
		frame = 0x436;
		break;
	case 5:
	case 13:
	case 14:
	case 15:
		state = 0x203;
		frame = 0x14e;
		break;
	case 0:
	case 1:
	case 4:
	case 12:
		state = 0x204;
		frame = 0x169;
		break;
	default:
		if(kind == 1)
		{
			state = 0x203;
			frame = 0x14e;
		}
		else if(kind == 2)
		{
			state = 0x204;
			frame = 0x169;
		}
		else if(e->unk9c == 0.0f)
		{
			state = 0x201;
			e->unk9c = 1.0f;
			frame = 0x137;
		}
		else
		{
			state = 0x202;
			frame = 0x137;
			e->unk9c = 0.0f;
		}
		break;
	}
	e->unk4 = 0;
	if(e->flags & 2)
	{
		e->flags &= ~2;
		e->resume_state = e->ai_state;
		e->resume_frame = e->frame;
		e->frame = frame;
		e->ai_state = state;
	}
	else if(!(e->ai_state & 0x200))
	{
		e->resume_state = e->ai_state;
		e->resume_frame = e->frame;
		blend_from_pose(e, submesh_info_zomb, 0x14, frame);
		e->ai_state = state;
	}
	else
	{
		if(state == e->ai_state)
		{
			e->frame = frame;
		}
		else
		{
			blend_from_pose(e, submesh_info_zomb, 0x14, frame);
		}
		e->ai_state = state;
	}
	return(0);
}

/* 0x800f8620, 0x800f8650, 0x800f8798: the texmaps; the model, the animation and the path
   (submeshes 5, 4, 17, 3 and 2 share the animations of 15, 1, 16, 20 and 19); their removal */
void load_zombie_texmap(void)
{
	load_texmap(zombie_texmap, 0x2d, "zomb");
}

void load_zomb(void)
{
	load_model((void **)&zomb_verts, 0x29c, (void **)&zomb_faces, 0x478, (void **)&zomb_normals, 0x2eb, "ZOMB.ZM", 0xc5b9f6a3);
	load_anim(submesh_info_zomb, 0x15, 0x5c5, "zomb.za", 0xc8e92942);
	load_path(&zomb_path, 0x33d, "zomb.pth", 0x1652ea8b);
	free(submesh_info_zomb[5].anim);
	free(submesh_info_zomb[4].anim);
	free(submesh_info_zomb[17].anim);
	free(submesh_info_zomb[2].anim);
	free(submesh_info_zomb[3].anim);
	submesh_info_zomb[5].anim = submesh_info_zomb[15].anim;
	submesh_info_zomb[4].anim = submesh_info_zomb[1].anim;
	submesh_info_zomb[17].anim = submesh_info_zomb[16].anim;
	submesh_info_zomb[3].anim = submesh_info_zomb[20].anim;
	submesh_info_zomb[2].anim = submesh_info_zomb[19].anim;
}

void remove_zombie_assets(void)
{
	submesh_info_zomb[5].anim = 0;
	submesh_info_zomb[4].anim = 0;
	submesh_info_zomb[17].anim = 0;
	submesh_info_zomb[3].anim = 0;
	submesh_info_zomb[2].anim = 0;
	remove_surface(zombie_texmap, 0x2d);
	remove_path(&zomb_path, 0x33d, "zomb.pth");
	remove_animation(submesh_info_zomb, 0x15, 0x5c5, "zomb.za");
	remove_model((void **)&zomb_verts, 0x29c, (void **)&zomb_faces, 0x478, (void **)&zomb_normals, 0x2eb, "ZOMB.ZM");
}

void	zombie_remove(game_object_t *);

/* 0x800f8860: the init callback: a random start frame, its goal 10 in front of the player, the
   stumps hidden, half the zombies without group 16's 0x1000000 bit, the body as the hit
   group, its first state; with D_801e5cd4 set one of five held objects on group 16 */
int zombie_init(game_object_t *obj)
{
	enemy_t	*e;
	short	*p;
	group_t	*g;
	int	i;

	p = malloc(sizeof(enemy_t));
	obj->data = p;
	if(!p)
	{
		return(1);
	}
	obj->hit = zombie_hit;
	obj->die = zombie_die;
	obj->remove = zombie_remove;
	e = (enemy_t *)p;
	e->counter = 0;
	e->unk4 = 0x14;
	e->frame = randrng(100);
	e->num = 0;
	e->flags = 0xc;
	e->unka8 = 0;
	e->target = get_target_player();
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0.0f, -10.0f);
	e->rx = 0;
	e->rz = 0;
	e->x = 0;
	e->y = 0;
	e->z = 0;
	e->ry = 3.1415927f;
	e->unkcc = 1.0f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 7.5f);
	e->groups = malloc(0x15 * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < 0x15; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[5].flags = 0;
	e->groups[4].flags = 0;
	e->groups[17].flags = 0;
	e->groups[2].flags = 0;
	e->groups[3].flags = 0;
	if(randrng(100) < 50)
	{
		e->groups[16].flags |= 0x1000000;
	}
	init_groups(e, submesh_info_zomb, 0x15);
	e->hit_group = &e->groups[20];
	e->hit_group->floor = 0.0f;
	e->unk1a = 0x200c;
	clear_enemy_fields(e);
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	zombie_setup(e, 0);
	if(D_801e5cd4)
	{
		g = &e->groups[16];
		switch(randrng(5))
		{
		case 0:
			attach_effect(e, attach_held_object(g, 0x40, 0.06f, 0.02f, -0.02f, 1.6f, 2.0f, 0.0f, e->scale));
			break;
		case 1:
			attach_effect(e, attach_held_object(g, 0x41, 0.16f, 0.0f, 0.04f, 4.68f, -1.78f, 0.0f, e->scale));
			break;
		case 2:
			attach_effect(e, attach_held_object(g, 0x42, 0.14f, 0.0f, 0.04f, -0.32f, 0.0f, 4.68f, e->scale));
			break;
		case 3:
			attach_effect(e, attach_held_object(g, 0x43, 0.18f, 0.0f, 0.02f, -0.6f, 1.42f, -1.54f, e->scale));
			break;
		default:
			attach_effect(e, attach_held_object(g, 0x44, 0.14f, 0.0f, 0.06f, 0.0f, 0.0f, -1.5f, e->scale));
			break;
		}
	}
	return(0);
}

/* 0x800f8c24: the remove callback */
void zombie_remove(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(debug_enemy == e)
	{
		debug_enemy = 0;
	}
	unlink_enemy(e);
	free_effects(e);
	free(e->groups);
	free(e);
}

/* 0x800f8c84: the think callback: the effects, the loose groups, then the state: the walks
   and the idle loops, the claws and the bite (hurting the player when the hand is within 30 at
   the frame of the blow), the throw (state 0x110 throws the object in group 12 at frame
   1141), carrying the victim (0x1000, 0x2000), the hit reactions blending back, the deaths, and
   the body coming apart (0x403, gone after 30 frames) */
void zombie_think(game_object_t *obj)
{
	enemy_t	*e;
	enemy_t	*held;

	e = (enemy_t *)obj->data;
	held = HELD(e);
	think_effects(e);
	if(debug_mode & 0x10000)
	{
		return;
	}
	animate_loose_groups(e, 0x15);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x10d:
		if(++e->frame >= 0x511)
		{
			zombie_next_action(e);
		}
		break;
	case 0x10c:
		if(++e->frame >= 0x4cb)
		{
			e->frame = 0x497;
		}
		break;
	case 0x140:
		face_point(e, e->goal, 0.2f);
		step_straight_to_goal(e, 0.4f);
		if(++e->frame >= 0x7a)
		{
			e->frame = 0;
		}
		break;
	case 0x10b:
		if(++e->frame >= 0x436)
		{
			e->frame = 0x36d;
		}
		break;
	case 0x110:
		if(++e->frame == 0x475 && (e->groups[12].flags & GROUP_ROOT))
		{
			throw_at_camera(e->groups[12].pos, 1, e->scale);
			detach_effect(e, &e->groups[12]);
		}
		if(e->frame >= 0x497)
		{
			zombie_next_action(e);
		}
		break;
	case 0x1001:
		if(++e->frame >= 0x5c5)
		{
			e->frame = 0x465;
		}
		break;
	case 0x600:
		angles_to(&camera_x, &e->x, &e->rx);
		/* fall through */
	case 0x108:
		if(++e->frame >= 0x2f9)
		{
			zombie_walk(e);
		}
		break;
	case 0x1000:
		e->frame++;
		if(held)
		{
			if(e->frame < 0x30e)
			{
				held->frame++;
			}
			else if(e->frame == 0x30e)
			{
				vict_setup(held, 0x100);
				held->x = held->hit_group->pos[0];
				held->y = held->hit_group->pos[1];
				held->z = held->hit_group->pos[2];
				e->unkdc = 0;
				held->unkd8 = 0;
			}
		}
		if(e->frame >= 0x329)
		{
			zombie_next_action(e);
		}
		break;
	case 0x2000:
		e->frame++;
		if(held)
		{
			held->frame++;
		}
		if(e->frame >= 0x354)
		{
			e->frame -= 0x2b;
			if(held)
			{
				held->frame -= 0x2b;
			}
		}
		break;
	case 0x105:
	case 0x106:
		if(++e->frame >= 0x2d4)
		{
			e->frame = 0x27b;
		}
		break;
	case 0x180:
		if(++e->frame >= 0x7a)
		{
			e->frame = 0;
		}
		path_place(e, zomb_path, e->path, 1.0f);
		if(++e->path > e->unk92)
		{
			if(e->unk6)
			{
				zombie_next_action(e);
			}
			else
			{
				zombie_walk(e);
			}
		}
		break;
	case 0x10e:
		if(++e->frame >= 0x575)
		{
			e->frame -= 0x32;
		}
		break;
	case 0x181:
		if(++e->frame >= 0x7a)
		{
			e->frame = 0;
		}
		break;
	case 0x102:
		if(e->frame == 0x1b6 && group_near_camera(&e->groups[12], 30.0f))
		{
			hurt_player(e->target, 0xfa, 7, 0.0f);
		}
		if(++e->frame >= 0x1bf)
		{
			zombie_walk(e);
		}
		break;
	case 0x103:
		if(++e->frame >= 0x124)
		{
			zombie_walk(e);
		}
		break;
	case 0x107:
		if(++e->frame >= 0x124)
		{
			zombie_next_action(e);
		}
		break;
	case 0x104:
		face_camera(e, 0.2f);
		func_800fa5b4(e);
		if(++e->frame >= 0x5c5)
		{
			zombie_walk(e);
		}
		break;
	case 0x800:
		face_camera(e, 0.2f);
		if(e->frame == 0x80)
		{
			if(group_near_camera(&e->groups[12], 30.0f))
			{
				hurt_player(e->target, 0xfa, 7, 3.1415927f);
			}
		}
		else if(e->frame == 0x92)
		{
			if(group_near_camera(&e->groups[13], 30.0f))
			{
				hurt_player(e->target, 0xfa, 7, 0.0f);
			}
		}
		if(++e->frame >= 0x96)
		{
			zombie_walk(e);
		}
		break;
	case 0x801:
		face_camera(e, 0.2f);
		if(e->frame == 0x9f && (group_near_camera(&e->groups[16], 30.0f) || group_near_camera(&e->groups[17], 30.0f)))
		{
			hurt_player(e->target, 0xfa, 3, 0.0f);
		}
		if(++e->frame >= 0xaf)
		{
			zombie_walk(e);
		}
		break;
	case 0x109:
		if(++e->frame >= 0x1a1)
		{
			if(e->unk6)
			{
				zombie_next_action(e);
			}
			else
			{
				e->frame = 0x17f;
			}
		}
		break;
	case 0x10a:
		if(++e->frame >= 0x1a1)
		{
			zombie_walk(e);
		}
		break;
	case 0x111:
		if(++e->frame >= 0x2a7)
		{
			zombie_walk(e);
		}
		break;
	case 0x101:
		if(++e->frame >= 0x7a)
		{
			e->frame = 0;
		}
		face_camera(e, 0.2f);
		zombie_walk(e);
		break;
	case 0x201:
		if((e->frame += 2) >= 0x14e)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_zomb, 0x14, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x202:
		if((e->frame += 2) >= 0x14e)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_zomb, 0x14, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x203:
		if((e->frame += 2) >= 0x169)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_zomb, 0x14, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x204:
		if((e->frame += 2) >= 0x17f)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_zomb, 0x14, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x205:
		if((e->frame += 2) >= 0x137)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_zomb, 0x14, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x206:
		if((e->frame += 2) >= 0x137)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_zomb, 0x14, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x207:
		if((e->frame += 2) >= 0x465)
		{
			e->frame--;
			blend_to_frame(e, submesh_info_zomb, 0x14, e->resume_frame, 2);
			e->ai_state = e->resume_state;
		}
		break;
	case 0x401:
		if(++e->frame >= 0x258)
		{
			e->frame--;
			e->ai_state = 0x405;
		}
		break;
	case 0x402:
		if(++e->frame >= 0x36d)
		{
			e->frame--;
			e->ai_state = 0x405;
		}
		break;
	case 0x404:
		if(++e->frame >= 0x27b)
		{
			e->frame--;
			e->ai_state = 0x405;
		}
		break;
	case 0x403:
		if(--e->counter == 0)
		{
			remove_game_object(obj->id);
		}
		break;
	case 0x405:
		break;
	}
}

/* 0x800f96a8: the state set-up: the start frame of each state; the held objects some states
   bring (0x104, 0x10c, 0x110: one in group 12; 0x10d, 0x1001: two or three); flames on nine
   groups (0x107); the path (0x180, 0x181); the body coming apart (0x403: blood, chunks, every
   group flung loose, 30 frames to go); walking in with a victim (0x1000 the hanging
   innocent 0x2012, 0x2000 the carried one 0x2011, placed at the zombie) */
void zombie_setup(enemy_t *e, unsigned short state)
{
	game_object_t *o;
	short	id;
	int	i;

	e->ai_state = state;
	switch(state)
	{
	case 0x10c:
		e->frame = 0x497;
		attach_effect(e, attach_held_object(&e->groups[12], 1, 0.1f, 0.0f, 0.1f, 1.570796f, 0.0f, 3.1415927f, e->scale));
		break;
	case 0x10d:
		e->frame = 0x4cb;
		attach_effect(e, attach_held_object(&e->groups[12], 0, 0.1f, 0.0f, 0.1f, 1.570796f, 0.0f, 3.1415927f, e->scale));
		attach_effect(e, attach_held_object(&e->groups[16], 2, 0.141f, 0.0f, 0.0f, 0.0f, 0.0f, 4.712388f, e->scale));
		break;
	case 0x110:
		e->frame = 0x465;
		attach_effect(e, attach_held_object(&e->groups[12], 1, 0.1f, 0.0f, 0.1f, 1.570796f, 0.0f, 3.1415927f, e->scale));
		break;
	case 0x1001:
		e->frame = 0x7d0;
		attach_effect(e, attach_held_object(&e->groups[12], 1, 0.1f, 0.0f, 0.1f, 1.570796f, 0.0f, 3.1415927f, e->scale));
		attach_effect(e, attach_held_object(&e->groups[20], 0, 0.0f, -0.05f, 0.0f, 1.570796f, 0.0f, 0.0f, e->scale));
		attach_effect(e, attach_held_object(&e->groups[16], 2, 0.23f, 0.0f, 0.0f, 1.570796f, 1.570796f, 0.0f, e->scale));
		break;
	case 0x101:
		e->frame = randrng(0x78);
		zombie_pick_held_object(e);
		break;
	case 0x105:
		e->frame = randrng(0x57) + 0x27b;
		break;
	case 0x106:
		e->frame = randrng(0x57) + 0x27b;
		break;
	case 0x180:
		e->frame = 0;
		path_place(e, zomb_path, e->path, 1.0f);
		e->counter = 0;
		break;
	case 0x181:
		e->frame = 0;
		path_place(e, zomb_path, e->path, 1.0f);
		break;
	case 0x102:
		e->frame = 0x1a1;
		zombie_pick_held_object(e);
		break;
	case 0x103:
		e->frame = 0xaf;
		break;
	case 0x2000:
		e->frame = 0x329;
		id = add_object1_wrapper(0x2011, inng_draw, inng_init, inng_think);
		if(id)
		{
			add_die_like(id, e->id);
			e->unkdc = (int)get_object_data(id);
			inng_setup(HELD(e), 0x2000);
			goto place;
		}
		break;
	case 0x1000:
		e->frame = 0x2f9;
		id = add_object1_wrapper(0x2012, vict_draw, vict_init, vict_think);
		if(id)
		{
			add_die_like(id, e->id);
			e->unkdc = (int)get_object_data(id);
			vict_setup(HELD(e), 1);
		place:
			HELD(e)->x = e->x;
			HELD(e)->y = e->y;
			HELD(e)->z = e->z;
			HELD(e)->rx = e->rx;
			HELD(e)->ry = e->ry;
			HELD(e)->rz = e->rz;
			HELD(e)->unkd8 = (int)e;
		}
		break;
	case 0x107:
		e->frame = 0xaf;
		attach_effect(e, attach_flame(0.0f, (submesh_info_zomb[15].radius + submesh_info_zomb[15].radius) * e->scale[0], &e->groups[15]));
		attach_effect(e, attach_flame(0.0f, (submesh_info_zomb[14].radius + submesh_info_zomb[14].radius) * e->scale[0], &e->groups[14]));
		attach_effect(e, attach_flame(0.0f, (submesh_info_zomb[13].radius + submesh_info_zomb[13].radius) * e->scale[0], &e->groups[13]));
		attach_effect(e, attach_flame(0.0f, (submesh_info_zomb[1].radius + submesh_info_zomb[1].radius) * e->scale[0], &e->groups[1]));
		attach_effect(e, attach_flame(0.0f, (submesh_info_zomb[0].radius + submesh_info_zomb[0].radius) * e->scale[0], &e->groups[0]));
		attach_effect(e, attach_flame(0.0f, (submesh_info_zomb[12].radius + submesh_info_zomb[12].radius) * e->scale[0], &e->groups[12]));
		attach_effect(e, attach_flame(0.0f, (submesh_info_zomb[16].radius + submesh_info_zomb[16].radius) * e->scale[0], &e->groups[16]));
		attach_effect(e, attach_flame(0.0f, (submesh_info_zomb[18].radius + submesh_info_zomb[18].radius) * e->scale[0], &e->groups[18]));
		attach_effect(e, attach_flame(0.0f, (submesh_info_zomb[19].radius + submesh_info_zomb[19].radius) * e->scale[0], &e->groups[19]));
		break;
	case 0x104:
		e->frame = 0x575;
		attach_effect(e, attach_held_object(&e->groups[12], 1, 0.1f, 0.0f, 0.1f, 1.570796f, 0.0f, 3.1415927f, e->scale));
		break;
	case 0x403:
		e->flags &= ~2;
		burst_blood(e->groups[19].pos, 0.0f, 0x800000);
		burst_blood(e->groups[19].pos, 0.0f, 0x800000);
		add_chunks_n(e->groups[19].pos, e->y, 10);
		for(i = 0; i < 0x15; i++)
		{
			fling_group(&e->groups[i], submesh_info_zomb[i].pivot, e->groups[i].pos, e->y, e->scale);
		}
		e->counter = 0x1e;
		mark_die(e->id, e->killer);
		break;
	case 0x109:
		e->frame = 0x17f;
		break;
	case 0x10f:
		e->ai_state = 0x109;
		e->frame = 0x18e;
		break;
	case 0x10a:
		e->frame = 0x17f;
		break;
	case 0x600:
		e->frame = 0;
		break;
	case 0x108:
		e->frame = 0x2d8;
		break;
	case 0x10b:
		e->frame = 0x36d;
		break;
	case 0x10e:
		e->frame = 0x511;
		break;
	case 0x140:
		e->frame = 0;
		break;
	}
}

/* 0x800f9e8c: group 1's arm comes off: its stump (group 4) takes its place with a spray, and
   groups 12 and 0 fall loose (once: not when the stump already shows) */
void func_800f9e8c(enemy_t *e)
{
	if(!(e->groups[4].flags & GROUP_VISIBLE))
	{
		replace_group(e, &e->groups[1], &e->groups[4]);
		pose_group(e, submesh_info_zomb, 4);
		attach_effect(e, attach_spray(zomb_spray_arm4, zomb_spray_arm4_dir, &e->groups[4]));
		detach_group(&e->groups[12], e->groups[0].pos, e->y, e->scale);
		detach_group(&e->groups[0], e->groups[0].pos, e->y, e->scale);
	}
}

/* 0x800f9f40: the same for group 15's arm: stump 5, groups 13 and 14 fall loose */
void func_800f9f40(enemy_t *e)
{
	if(!(e->groups[5].flags & GROUP_VISIBLE))
	{
		replace_group(e, &e->groups[15], &e->groups[5]);
		pose_group(e, submesh_info_zomb, 5);
		attach_effect(e, attach_spray(zomb_spray_arm5, zomb_spray_arm5_dir, &e->groups[5]));
		detach_group(&e->groups[13], e->groups[14].pivot, e->y, e->scale);
		detach_group(&e->groups[14], e->groups[14].pivot, e->y, e->scale);
	}
}

/* 0x800f9ff8: puts e in state 0x101; nothing calls it */
void func_800f9ff8(int unused, enemy_t *e)
{
	e->ai_state = 0x101;
}

/* 0x800fa004: at the end of a walk or an action the spawn entry's next state (enemy_t.unk6):
   walk again, kneel (0x10e), throw (0x110), turn and walk 500 away (0x140, the goal behind it),
   follow the path (0x180); anything else stands up into the idle (0x109) */
void zombie_next_action(enemy_t *e)
{
	float	v[3];
	float	m[9];
	float	a[3];

	switch((unsigned short)e->unk6)
	{
	case 0x10e:
		e->unk6 = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_zomb, 0x14, 0x511, 2);
		e->ai_state = 0x10e;
		break;
	case 0x101:
		e->frame = randrng(0x78);
		e->ai_state = 0x101;
		break;
	case 0x110:
		zombie_setup(e, 0x110);
		e->unk6 = 0;
		return;
	case 0x180:
		zombie_setup(e, 0x180);
		e->unk6 = 0;
		return;
	case 0x140:
		zombie_setup(e, 0x140);
		e->frame = 0;
		v[0] = 0.0f;
		v[1] = 0.0f;
		v[2] = -500.0f;
		a[0] = e->rx;
		a[1] = e->ry + 3.1415927f;
		a[2] = e->rz;
		rot_matrix_zyx(m, a);
		mat_vec_mul(v, m, v);
		e->goal[0] = e->x + v[0];
		e->goal[1] = e->y + v[1];
		e->goal[2] = e->z + v[2];
		break;
	default:
		e->unk6 = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_zomb, 0x14, 0x17f, 2);
		e->ai_state = 0x109;
		break;
	}
	e->unk6 = 0;
}

/* 0x800fa1c4: walk at the player (keeping to a player still in the game): the goal 20 in front
   of the view; standing there it idles (0x101); still walking (0x10a); within reach a claw
   (0x800, or a bite 0x801 one time in ten or when both arms are gone), or after one of those the
   recovery (0x111) */
void zombie_walk(enemy_t *e)
{
	player_stats_t *p;
	int	r;

	if(e->target->state != 4)
	{
		p = get_target_player();
		if(p->state == 4)
		{
			e->target = p;
		}
	}
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0.0f, -20.0f);
	r = step_toward_goal_xz(e, 0.4f);
	switch(r)
	{
	case 1:
		if(e->ai_state == 0x10a)
		{
			e->frame = 0x17f;
		}
		else
		{
			if(e->frame > 0)
			{
				e->frame--;
			}
			blend_to_frame(e, submesh_info_zomb, 0x14, 0x17f, 2);
			e->ai_state = 0x10a;
		}
		break;
	case 2:
		if(randrng(100) < 10 || ((e->groups[5].flags & GROUP_VISIBLE) && (e->groups[4].flags & GROUP_VISIBLE)))
		{
			if(e->ai_state == 0x801)
			{
				if(e->frame > 0)
				{
					e->frame--;
				}
				blend_to_frame(e, submesh_info_zomb, 0x14, 0x27b, r);
				e->ai_state = 0x111;
			}
			else
			{
				play_enemy_sound(e, 1, "zombie", 0);
				if(e->frame > 0)
				{
					e->frame--;
				}
				blend_to_frame(e, submesh_info_zomb, 0x14, 0x96, r);
				e->ai_state = 0x801;
			}
		}
		else if(e->ai_state == 0x800)
		{
			if(e->frame > 0)
			{
				e->frame--;
			}
			blend_to_frame(e, submesh_info_zomb, 0x14, 0x27b, 2);
			e->ai_state = 0x111;
		}
		else
		{
			play_enemy_sound(e, 1, "zombie", 0);
			if(e->frame > 0)
			{
				e->frame--;
			}
			blend_to_frame(e, submesh_info_zomb, 0x14, 0x7a, 2);
			e->ai_state = 0x800;
		}
		break;
	case 0:
		if(e->ai_state != 0x101)
		{
			if(e->frame > 0)
			{
				e->frame--;
			}
			blend_to_frame(e, submesh_info_zomb, 0x14, 0, 2);
			e->ai_state = 0x101;
		}
		break;
	}
}

/* 0x800fa490: the walk's held object: one time in ten each, group 12 or 13 gets kind 0 or 1 */
void zombie_pick_held_object(enemy_t *e)
{
	int	r;

	r = randrng(0xfa);
	if(r < 25)
	{
		attach_effect(e, attach_held_object(&e->groups[12], 0, 0.1f, 0.0f, 0.1f, 1.570796f, 0.0f, 3.1415927f, e->scale));
	}
	else if(r < 50)
	{
		attach_effect(e, attach_held_object(&e->groups[12], 1, 0.1f, 0.0f, 0.1f, 1.570796f, 0.0f, 3.1415927f, e->scale));
	}
	else if(r < 75)
	{
		attach_effect(e, attach_held_object(&e->groups[13], 0, 0.1f, 0.0f, 0.1f, 1.570796f, 0.0f, 3.1415927f, e->scale));
	}
	else if(r < 100)
	{
		attach_effect(e, attach_held_object(&e->groups[13], 1, 0.1f, 0.0f, 0.1f, 1.570796f, 0.0f, 3.1415927f, e->scale));
	}
}

/* 0x800fa5b4: state 0x104's blows: at frames 159 and 185, and 1424, 1449 and 1472 (these knock
   the view sideways), group 12 within 30 hurts the player 250 */
void func_800fa5b4(enemy_t *e)
{
	switch(e->frame)
	{
	case 0x9f:
		if(group_near_camera(&e->groups[12], 30.0f))
		{
			hurt_player(e->target, 0xfa, 7, 0.0f);
		}
		break;
	case 0xb9:
		if(group_near_camera(&e->groups[12], 30.0f))
		{
			hurt_player(e->target, 0xfa, 7, 0.0f);
		}
		break;
	case 0x590:
	case 0x5a9:
	case 0x5c0:
		if(group_near_camera(&e->groups[12], 30.0f))
		{
			hurt_player(e->target, 0xfa, 7, 3.1415927f);
		}
		break;
	}
}

/* 0x800fa6a4: let go of the victim (the hanging innocent, 0x2012): it falls from where its
   hit group is */
void zombie_release_victim(enemy_t *e)
{
	enemy_t	*held;

	held = HELD(e);
	if(held && (unsigned short)held->unk1a == 0x2012)
	{
		vict_setup(held, 0x100);
		held->x = held->hit_group->pos[0];
		held->y = held->hit_group->pos[1];
		held->z = held->hit_group->pos[2];
		e->unkdc = 0;
		held->unkd8 = 0;
	}
}
