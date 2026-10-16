/*
 * doctor_klot.c - the Big Top's Doctor Klot (object type 0x2060; assets DOCT.ZM / doct.za /
 * doct.pth and the doct*.WMS texmaps: coat, saw, drill, pump, case ...; his voice is the "klott"
 * bank). One original file, 0x8013b9c8-0x8013cfb0 (toke's file comes before it, the skeleton's
 * after); its tables are in doctor_klot_data.c (0x801a11b0-0x801a1bc0). The model has 905
 * vertices, 1497 faces and 974 normals in 22 submeshes, the animation 339 frames, the path 205
 * points.
 *
 * Each Klot carries one of three instruments, in turn from Klot to Klot (part 20, parts 6 and 7,
 * or part 21; parts 18 and 19 are his damaged face and head, hidden at first). He walks his path
 * (0x8801), or stands (2) or idles (0); then he closes on a point 18 units in front of the
 * camera, playing a taunt when he gets there, and attacks with his instrument: 0x201 two blows,
 * 0x202 two blows, 0x203 one, each 180 damage when his part 15 is within 20 units of the camera.
 * A shot at the top violence level marks the part hit (two hits on the head swap it for its
 * damaged version and spray blood); 7.5 health, 500 points for the kill and a death call, 50 and
 * a hurt call for a hit; he falls (0x2001 or, three times in four, 0x2002) and is gone (0x4000).
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
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	attach_effect(enemy_t *, void *);
extern void	*attach_flame(float, float, group_t *);
extern void	*attach_spray(float *, float *, group_t *);
extern void	detach_group(group_t *, float *, float, float *);
extern void	replace_group(enemy_t *, group_t *, group_t *);
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
extern void	path_follow_turned(enemy_t *, float *, int, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern int	mark_die();
extern int	load_model(void **, int, void **, int, void **, int, char *, int);
extern int	load_anim(submesh_t *, int, int, char *, int);
extern int	load_path(void **, int, char *, int);
extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern enemy_t	*debug_enemy;
extern char	text_buffer[];
extern int	violence_level;

extern texmap_t	doctor_texmap[50];		/* doctor_klot_data.c */
extern submesh_t submesh_info_doctor[22];
extern float	klot_head_spray_dir[3], klot_head_spray_base[3];
extern int	klot_taunts[32], klot_hurt_calls[15], klot_death_calls[2];
extern int	klot_instrument_count;

float	*doctor_verts;			/* 0x801e593c */
face_t	*doctor_faces;			/* 0x80212e78 */
float	*doctor_normals;		/* 0x801e4398 */
float	*doctor_path;			/* 0x802372a0 */

#define KLOT_GROUPS	0x16
#define KLOT_NVERTS	0x389
#define KLOT_NFACES	0x5d9
#define KLOT_NNORMALS	0x3ce
#define KLOT_FRAMES	0x153
#define KLOT_PATH_LEN	0xcd
#define KLOT_TEXMAPS	0x32
#define KLOT_HIT_GROUP	1		/* the submesh his record is measured from */
#define KLOT_FACE	12		/* the face; 18 is its damaged version */
#define KLOT_HEAD	17		/* the head; 19 is its damaged version */
#define KLOT_HAND	15		/* the part his blows land with */
#define KLOT_REACH	20.0f
#define KLOT_BLOW	180		/* the damage of a blow */
#define HURT_STAB	3		/* hurt_player's plain mark */

#define KLOT_IDLE	0
#define KLOT_STAND	2
#define KLOT_READY	1		/* in front of the camera, between blows */
#define KLOT_CLOSE	3		/* at his goal, turning to it */
#define KLOT_PATH	0x8801

/* 0x8013b9c8: the draw callback (Binary Ninja's render_enetity_debug_info) */
void render_enetity_debug_info(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_doctor, KLOT_GROUPS, doctor_verts, KLOT_NVERTS, doctor_faces, KLOT_NFACES, doctor_normals, KLOT_NNORMALS);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[12].pos, e->radius, 0x600000);
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

int	klot_die(game_object_t *, player_stats_t *);

/* 0x8013bbc4: the hit test: the bounding sphere, then the submesh */
int klot_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_doctor, KLOT_GROUPS, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)klot_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x8013bc80: the shot lands: blood; the flame thrower sets the part alight; at the top violence
   level the part hit is marked (the face is swapped on a second hit, the head marked on the
   first and swapped with a spray of blood on the second); then the damage, points and a call,
   death or a hit reaction by the part hit */
int klot_die(game_object_t *obj, player_stats_t *p)
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
			attach_effect(e, attach_flame(0.0f, (submesh_info_doctor[group].radius + submesh_info_doctor[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		switch(group)
		{
		case 0:
		case 17:
			e->unka8++;
			switch(e->unka8)
			{
			case 0:
				printf("This should never happen! Zomb head damage at 0\n");
				break;
			case 1:
				e->groups[17].flags |= 0xf0000000;
				e->groups[0].flags |= 0xf0000000;
				break;
			case 2:
				replace_group(e, &e->groups[17], &e->groups[19]);
				attach_effect(e, attach_spray(klot_head_spray_base, klot_head_spray_dir, &e->groups[19]));
				break;
			}
			break;
		case 12:
			if(g->flags & 0xf0000000)
			{
				replace_group(e, &e->groups[12], &e->groups[18]);
			}
			else
			{
				g->flags |= 0xf0000000;
			}
			break;
		case 11:
		case 13:
		case 15:
			e->groups[11].flags |= 0xf0000000;
			break;
		case 2:
		case 14:
		case 16:
			e->groups[2].flags |= 0xf0000000;
			break;
		case 1:
			e->groups[1].flags |= 0xf0000000;
			break;
		case 3:
			e->groups[3].flags |= 0xf0000000;
			break;
		case 4:
		case 5:
			e->groups[4].flags |= 0xf0000000;
			break;
		case 8:
			e->groups[8].flags |= 0xf0000000;
			break;
		case 9:
		case 10:
			e->groups[9].flags |= 0xf0000000;
			break;
		}
	}
	if(!(e->ai_state & 0x6000))
	{
		e->health -= shot_damage_of(p);
		if(e->health <= 0.0f)
		{
			award_points(p->index, 500);
			play_enemy_sound(e, klot_death_calls[randrng(2)], "klott", 8);
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
			e->frame = 0x95;
			e->flags &= 0xfffd;
		}
		else
		{
			award_points(p->index, 50);
			play_enemy_sound(e, klot_hurt_calls[randrng(15)], "klott", 2);
		}
	}
	if(e->ai_state & 0xe000)
	{
		return(0);
	}
	switch(group)
	{
	case 8:
	case 9:
	case 10:
		start_hit_reaction(e, 0x84, 0x1005, submesh_info_doctor, 1);
		break;
	case 3:
	case 4:
	case 5:
		start_hit_reaction(e, 0x75, 0x1006, submesh_info_doctor, 1);
		break;
	case 2:
	case 14:
	case 16:
		start_hit_reaction(e, 0x63, 0x1007, submesh_info_doctor, 1);
		break;
	case 11:
	case 13:
	case 15:
		start_hit_reaction(e, 0x4c, 0x1008, submesh_info_doctor, 1);
		break;
	case 17:
	case 19:
		if(randrng(100) < 50)
		{
			start_hit_reaction(e, 0x38, 0x1003, submesh_info_doctor, 1);
		}
		else
		{
			start_hit_reaction(e, 0x2e, 0x1004, submesh_info_doctor, 1);
		}
		break;
	case 1:
	case 12:
	case 18:
		if(randrng(100) < 50)
		{
			start_hit_reaction(e, 0x19, 0x1001, submesh_info_doctor, 1);
		}
		else
		{
			start_hit_reaction(e, 0, 0x1002, submesh_info_doctor, 1);
		}
		break;
	}
	return(0);
}

/* 0x8013c16c */
void load_doctorklot_texmap(void)
{
	load_texmap(doctor_texmap, KLOT_TEXMAPS, "doct");
}

/* 0x8013c19c: the model, animation and path; the damaged face, head and part 20 take their
   animation from the whole face (12), head (17) and part 15 instead of their own */
void load_doctor_klot(void)
{
	load_model((void **)&doctor_verts, KLOT_NVERTS, (void **)&doctor_faces, KLOT_NFACES, (void **)&doctor_normals, KLOT_NNORMALS, "DOCT.ZM", 0x1dae4ee5);
	load_anim(submesh_info_doctor, KLOT_GROUPS, KLOT_FRAMES, "doct.za", 0xb3ff103a);
	load_path((void **)&doctor_path, KLOT_PATH_LEN, "doct.pth", 0x5d43ad41);
	free(submesh_info_doctor[18].anim);
	free(submesh_info_doctor[19].anim);
	free(submesh_info_doctor[20].anim);
	submesh_info_doctor[18].anim = submesh_info_doctor[12].anim;
	submesh_info_doctor[19].anim = submesh_info_doctor[17].anim;
	submesh_info_doctor[20].anim = submesh_info_doctor[15].anim;
}

/* 0x8013c2a4: the assets go when the level is unloaded (the shared animations are let go first) */
void func_8013c2a4(void)
{
	submesh_info_doctor[18].anim = 0;
	submesh_info_doctor[19].anim = 0;
	submesh_info_doctor[20].anim = 0;
	remove_surface(doctor_texmap, KLOT_TEXMAPS);
	remove_path((void **)&doctor_path, KLOT_PATH_LEN, "doct.pth");
	remove_animation(submesh_info_doctor, KLOT_GROUPS, KLOT_FRAMES, "doct.za");
	remove_model((void **)&doctor_verts, KLOT_NVERTS, (void **)&doctor_faces, KLOT_NFACES, (void **)&doctor_normals, KLOT_NNORMALS, "DOCT.ZM");
}

void	klot_remove(game_object_t *);
void	func_8013cad4(enemy_t *, unsigned short);

/* 0x8013c35c: the init callback: the record, health 7.5, the 22 groups (the face and head 8,
   the damaged face and head hidden), the set-up */
int func_8013c35c(game_object_t *obj)
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
	obj->hit = klot_hit;
	obj->die = klot_die;
	obj->remove = klot_remove;
	e = (enemy_t *)data;
	e->unk4 = 0x14;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0xc;
	e->flags2 = 0;
	e->target = get_target_player();
	e->unk54 = 0;
	e->rx = 0;
	e->rz = 0;
	e->y = 0;
	e->z = 0;
	e->ry = 3.1415927f;
	e->x = -8.0f;
	e->unkcc = 1.14f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 7.5f);
	e->groups = malloc(KLOT_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < KLOT_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[KLOT_FACE].flags |= 8;
	e->groups[KLOT_HEAD].flags |= 8;
	e->groups[18].flags = 0;
	e->groups[19].flags = 0;
	e->unka8 = 0;
	init_groups(e, submesh_info_doctor, KLOT_GROUPS);
	g = e->groups;
	e->hit_group = &g[KLOT_HIT_GROUP];
	g[KLOT_HIT_GROUP].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	func_8013cad4(e, 0);
	return(0);
}

/* 0x8013c514: the remove callback */
void klot_remove(game_object_t *obj)
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

/* a hit reaction plays to its last frame, then he blends back to what he was doing */
#define HIT_REACTION(last) \
	frame = e->frame; \
	e->frame = frame + 1; \
	if(e->frame < last) \
	{ \
		break; \
	} \
	e->frame = frame; \
	blend_to_frame(e, submesh_info_doctor, 1, e->resume_frame, 1); \
	e->ai_state = e->resume_state; \
	break;

/* a fall plays to its last frame, then he is gone */
#define FALL(last) \
	frame = e->frame; \
	e->frame = frame + 1; \
	if(e->frame < last) \
	{ \
		break; \
	} \
	e->frame = frame; \
	e->ai_state = 0x4000; \
	break;

void	klot_approach(enemy_t *);
void	klot_end_path(enemy_t *);

/* 0x8013c56c: the think callback */
void func_8013c56c(game_object_t *obj)
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
	think_effects(e);
	animate_loose_groups(e, KLOT_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1003:
		HIT_REACTION(0x4c)
	case 0x1004:
		HIT_REACTION(0x38)
	case 0x1001:
		HIT_REACTION(0x2e)
	case 0x1002:
		HIT_REACTION(0x19)
	case 0x1005:
		HIT_REACTION(0x95)
	case 0x1006:
		HIT_REACTION(0x84)
	case 0x1007:
		HIT_REACTION(0x75)
	case 0x1008:
		HIT_REACTION(0x63)
	case KLOT_PATH:
		path_follow_turned(e, doctor_path, e->path, 1.0f);
		if(e->path >= e->unk92)
		{
			klot_end_path(e);
		}
		if(++e->frame >= 0xec)
		{
			e->frame = 0xde;
		}
		break;
	case KLOT_READY:
		if(++e->frame >= 0xec)
		{
			e->frame = 0xde;
		}
		face_camera(e, 0.2f);
		klot_approach(e);
		break;
	case 0x201:
		face_camera(e, 0.2f);
		if(++e->frame == 0x10e && group_near_camera(&e->groups[KLOT_HAND], KLOT_REACH))
		{
			hurt_player(e->target, KLOT_BLOW, HURT_STAB, 0.0f);
		}
		if(e->frame == 0x118 && group_near_camera(&e->groups[KLOT_HAND], KLOT_REACH))
		{
			hurt_player(e->target, KLOT_BLOW, HURT_STAB, 0.0f);
		}
		if(e->frame < 0x12e)
		{
			break;
		}
		klot_approach(e);
		break;
	case 0x202:
		face_camera(e, 0.2f);
		if(++e->frame == 0x13a && group_near_camera(&e->groups[KLOT_HAND], KLOT_REACH))
		{
			hurt_player(e->target, KLOT_BLOW, HURT_STAB, 0.0f);
		}
		if(e->frame == 0x144 && group_near_camera(&e->groups[KLOT_HAND], KLOT_REACH))
		{
			hurt_player(e->target, KLOT_BLOW, HURT_STAB, 0.0f);
		}
		if(e->frame < 0x153)
		{
			break;
		}
		klot_approach(e);
		break;
	case 0x203:
		face_camera(e, 0.2f);
		if(++e->frame == 0xf7 && group_near_camera(&e->groups[KLOT_HAND], KLOT_REACH))
		{
			hurt_player(e->target, KLOT_BLOW, HURT_STAB, 0.0f);
		}
		if(e->frame < 0x102)
		{
			break;
		}
		klot_approach(e);
		break;
	case KLOT_CLOSE:
		if(++e->frame < 0xde)
		{
			break;
		}
		klot_approach(e);
		break;
	case KLOT_STAND:
		if(++e->frame >= 0xde)
		{
			e->frame = 0xc7;
		}
		break;
	case 0x2001:
		FALL(0xc7)
	case 0x2002:
		FALL(0xc7)
	case KLOT_IDLE:
		if(++e->frame >= 0x153)
		{
			e->frame = 0;
		}
		break;
	}
}

/* 0x8013cad4: the set-up: the next instrument in turn (part 20, parts 6 and 7, or part 21; the
   choice goes into flags2 as 0x8000, 0x4000 or 0x2000), then the state's first frame */
void func_8013cad4(enemy_t *e, unsigned short state)
{
	klot_instrument_count++;
	e->ai_state = state;
	e->flags2 &= 0x1fff;
	if(klot_instrument_count % 3 == 0)
	{
		e->groups[21].flags = 0;
		e->groups[6].flags = 0;
		e->groups[7].flags = 0;
		e->groups[20].flags = 0xa000;
		e->flags2 |= 0x8000;
	}
	else if(klot_instrument_count % 3 == 1)
	{
		e->groups[21].flags = 0;
		e->groups[20].flags = 0;
		e->groups[7].flags = 0xa000;
		e->groups[6].flags = 0xa000;
		e->flags2 |= 0x4000;
	}
	else
	{
		e->groups[20].flags = 0;
		e->groups[6].flags = 0;
		e->groups[7].flags = 0;
		e->groups[21].flags = 0xa000;
		e->flags2 |= 0x2000;
	}
	switch(e->ai_state)
	{
	case KLOT_STAND:
		e->frame = 0xc7;
		break;
	case KLOT_READY:
		e->frame = 0xde;
		break;
	case KLOT_PATH:
		e->frame = 0xde;
		path_follow_turned(e, doctor_path, e->path, 1.0f);
		break;
	case KLOT_IDLE:
	default:
		e->frame = 0;
		break;
	}
}

/* 0x8013cc3c: go for the player: keep a playing target, steer to a point 18 in front of the
   camera and pick the state by how close he got (0 still far: walk, 1 there: turn to it,
   2 in reach: a taunt and the blow his instrument makes) */
void klot_approach(enemy_t *e)
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
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0.0f, -18.0f);
	r = step_toward_goal_xz(e, 1.3f);
	switch(r)
	{
	case 1:
		if(e->ai_state == KLOT_CLOSE)
		{
			e->frame = 0xc7;
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_doctor, 1, 0xc7, 1);
		e->ai_state = KLOT_CLOSE;
		break;
	case 2:
		if(e->frame > 0)
		{
			e->frame--;
		}
		play_enemy_sound(e, klot_taunts[randrng(32)], "klott", 0);
		if(e->flags2 & 0x8000)
		{
			blend_to_frame(e, submesh_info_doctor, 1, 0x102, 1);
			e->ai_state = 0x201;
		}
		else if(e->flags2 & 0x4000)
		{
			blend_to_frame(e, submesh_info_doctor, 1, 0x12e, 1);
			e->ai_state = 0x202;
		}
		else
		{
			blend_to_frame(e, submesh_info_doctor, 1, 0xec, 1);
			e->ai_state = 0x203;
		}
		break;
	case 0:
		if(e->ai_state == KLOT_READY)
		{
			break;
		}
		if(e->frame > 0)
		{
			e->frame--;
		}
		blend_to_frame(e, submesh_info_doctor, 1, 0xde, 1);
		e->ai_state = KLOT_READY;
		break;
	}
}

/* 0x8013ce50: at the end of his path: a taunt; the spawn entry's 2 makes him stand, else he goes
   for the player */
void klot_end_path(enemy_t *e)
{
	play_enemy_sound(e, klot_taunts[randrng(32)], "klott", 0);
	if((unsigned short)e->unk6 == 2)
	{
		e->unk6 = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_doctor, 1, 0xc7, 2);
		e->ai_state = KLOT_STAND;
		return;
	}
	e->unk6 = 0;
	klot_approach(e);
}

/* 0x8013cef0, 0x8013cf50: parts 15 and 13, or 16 and 14, come loose (no caller in the game) */
void klot_drop_parts_15_13(enemy_t *e)
{
	detach_group(&e->groups[15], e->groups[13].pos, e->y, e->scale);
	detach_group(&e->groups[13], e->groups[13].pos, e->y, e->scale);
}

void klot_drop_parts_16_14(enemy_t *e)
{
	detach_group(&e->groups[16], e->groups[14].pos, e->y, e->scale);
	detach_group(&e->groups[14], e->groups[14].pos, e->y, e->scale);
}
