/*
 * krinkle.c - Krinkle, the Rickety Town clown (object type 0x2041; 0x80126410-0x80127e40 and
 * its tables at 0x8019a788). The model KRIK.ZM has 729 vertices, 858 normals and 1196 faces in
 * 17 submeshes, the animation krik.za 837 frames, the path krik.pth 219 points. The draw, the
 * init, the removal, the loaders and the state set-up are in C; the think (0x80126fd8) and the
 * hit handling (0x8012660c, 0x801266c8) are still assembly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <glide/glide.h>
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "texmap.h"
#include "credits.h"			/* debug_mode */
#include "levelsnd.h"
#include "camera.h"
#include "weapons.h"

extern void	draw_effects(enemy_t *);			/* run the attached effects */
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, int, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);		/* objects.c */
extern void	set_enemy_health(enemy_t *, float);		/* health by difficulty */
extern player_stats_t *get_target_player(void);		/* objects.c */
extern int	randrng(int);
extern void	path_place(enemy_t *, void *, short, float);	/* start along a path */
extern void	free_effects(enemy_t *);
int	krinkle_hit(game_object_t *, player_stats_t *);
int	krinkle_die(game_object_t *, player_stats_t *);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *), remove_path(void **, int, char *);
extern enemy_t	*debug_enemy;
extern char	text_buffer[];

float	*krik_verts;			/* 0x801e5b38 */
face_t	*krik_faces;			/* 0x8021efd8 */
float	*krik_normals;			/* 0x801e57fc */
void	*krik_path;			/* 0x802372e0 */
int	krinkle_sounds[4] = { 1, 2, 3, 4 };	/* 0x8019b0a0 */

extern texmap_t	krinkle_texmap[55];		/* krinkle_data.c */
extern submesh_t submesh_info_krik[17];
void	krinkle_remove(game_object_t *);

/* 0x80126bd0, 0x80126c00: the texmaps, the model, the animation and the path. Submeshes 16
   and 17 (0x8019b04c, 0x8019b06c) share the animation of 10 and 14 */
void load_krinkle_texmap(void)
{
	load_texmap(krinkle_texmap, 0x37, "krik");
}

int load_krinkle(void)
{
	load_model((void **)&krik_verts, 0x2d9, (void **)&krik_faces, 0x4ac, (void **)&krik_normals, 0x35a, "KRIK.ZM", 0x27b51fc0);
	load_anim(submesh_info_krik, 0x11, 0x345, "krik.za", 0xa5cef503);
	load_path(&krik_path, 0xdb, "krik.pth", 0x03b5830d);
	free(submesh_info_krik[16].anim);
	free(submesh_info_krik[15].anim);
	submesh_info_krik[16].anim = submesh_info_krik[1].anim;
	submesh_info_krik[15].anim = submesh_info_krik[5].anim;
	return(0);
}

/* 0x80126ce8 */
void remove_krinkle_assets(void)
{
	submesh_info_krik[16].anim = 0;
	submesh_info_krik[15].anim = 0;
	remove_surface(krinkle_texmap, 0x37);
	remove_path(&krik_path, 0xdb, "krik.pth");
	remove_animation(submesh_info_krik, 0x11, 0x345, "krik.za");
	remove_model((void **)&krik_verts, 0x2d9, (void **)&krik_faces, 0x4ac, (void **)&krik_normals, 0x35a, "KRIK.ZM");
}

/* 0x80126410: the draw callback */
void krinkle_draw(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, submesh_info_krik, 0x11, krik_verts, 0x2d9, krik_faces, 0x4ac, krik_normals, 0x35a);
	draw_effects(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->groups[5].pos, e->radius, 0x60b060);
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

/* 0x80127c04: the state set-up: the high state bits move to flags2 (0x40 attaches an effect
   to submesh 11), then the state picks the animation frame */
void krinkle_setup(enemy_t *e, unsigned short state)
{
	effect_t *p;

	e->ai_state = state;
	if(state & 0x100)
	{
		e->ai_state = state & 0xfeff;
		e->flags2 |= 0x100;
	}
	if(e->ai_state & 0x80)
	{
		e->ai_state &= 0xff7f;
		e->flags2 |= 0x80;
	}
	if(e->ai_state & 0x20)
	{
		e->ai_state &= 0xffdf;
		e->flags2 |= 0x20;
	}
	if(e->ai_state & 0x40)
	{
		e->ai_state &= 0xffbf;
		e->flags2 |= 0x40;
		e->groups[0].flags = 0;
		p = attach_held_object(&e->groups[11], WEAPON_PRESENT, 0.06f, 0.15f, 0.16f, 4.2f, -2.9f, 0.0f, e->scale);
		attach_effect(e, p);
	}
	switch(e->ai_state)
	{
	case 0:
		e->frame = 0;
		break;
	case 1:
		e->frame = 0x5f;
		break;
	case 2:
		e->frame = 0x5f;
		break;
	case 4:
		e->frame = 0x1c5;
		break;
	case 0x202:
		e->frame = 0x2a9;
		break;
	case 0x203:
		e->unkac = 1;
		e->frame = 0x179;
		break;
	case 0x207:
		e->frame = 0x1da;
		break;
	case 0x208:
		e->frame = 0x225;
		break;
	case 0x20a:
		e->frame = 0x272;
		break;
	case 0x20b:
		e->ai_state = 0x203;
		e->unkac = 0;
		e->frame = 0x179;
		break;
	case 0x801:
		path_place(e, krik_path, e->path, 1.0f);
		e->frame = 0x5f;
		break;
	default:
		e->frame = 0;
		break;
	}
}

/* 0x80126d98: the init callback: the record, the submesh states, the first state, a greeting */
int krinkle_init(game_object_t *obj)
{
	enemy_t	*e;
	int	i;

	e = malloc(sizeof(enemy_t));
	obj->data = (short *)e;
	if(!e)
	{
		return(1);
	}
	obj->hit = krinkle_hit;
	obj->die = krinkle_die;
	obj->remove = krinkle_remove;
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
	e->x = 0;
	e->y = 0;
	e->z = 0;
	e->ry = 3.1415927f;
	e->unkcc = 0.92f;
	set_enemy_scale(e, 1.0f);
	set_enemy_health(e, 5.5f);
	e->groups = malloc(0x11 * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < 0x11; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[16].flags = 0;
	e->groups[15].flags = 0;
	e->unka8 = 0;
	e->unkac = 0;
	e->unkb0 = 0;
	init_groups(e, submesh_info_krik, 0x11);
	e->hit_group = &e->groups[5];
	e->groups[5].floor = 0.0f;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	krinkle_setup(e, 0);
	play_enemy_sound(e, krinkle_sounds[randrng(2)], "krinkle", -2);
	return(0);
}

/* 0x80126f5c: the remove callback: a last sound unless already dying, the effects, the record */
void krinkle_remove(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(!(e->ai_state & 0x4000))
	{
		play_enemy_sound(e, 0xb, "krinkle", 8);
	}
	if(debug_enemy == e)
	{
		debug_enemy = 0;
	}
	free_effects(e);
	free(e->groups);
	free(e);
}

extern void	think_effects(enemy_t *);
extern void	animate_loose_groups(enemy_t *, int);
extern int	group_near_camera(group_t *, float);		/* enemy.c */
extern int	face_camera(enemy_t *, float);			/* enemy.c */
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);	/* blend to a frame */
extern void	detach_effect(enemy_t *, group_t *);		/* drop the effect on a group */
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal_xz(enemy_t *, float);
extern int	remove_die();
int	krinkle_attack_sounds[6] = { 5, 6, 8, 12, 13, 14 };	/* 0x8019b0bc */
void	krinkle_steer(enemy_t *);
void	krinkle_end_path(enemy_t *);

/* 0x80126fd8: the think callback. States: 0 idle, 1 walk, 2 stand, 3 wait, 4 wave, 0x201 strike
   (the cane at frames 10/34/57/74/89 and the fist at 22/47/66 hit a player in reach), 0x202
   and 0x209 throw the cane and the head, 0x203-0x206 the other strikes, 0x207/0x208/0x20a the
   taunts, 0x801 the path, 0x1001-0x1005 hit reactions, 0x2001-0x2004 deaths, 0x4000 gone */
void krinkle_think(game_object_t *obj)
{
	enemy_t	*e;
	short	frame;
	effect_t *p;

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
	animate_loose_groups(e, 0x11);
	if(e->flags2 & 0x100)
	{
		path_place(e, krik_path, e->path, 1.0f);
		e->path++;
		if(e->path >= e->unk92)
		{
			if(e->flags & 2)
			{
				e->flags &= 0xfffd;
			}
			krinkle_end_path(e);
		}
	}
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1003:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0xe9)
		{
			break;
		}
		e->frame = frame;
		blend_to_frame(e, submesh_info_krik, 5, e->resume_frame, 2);
		e->ai_state = e->resume_state;
		break;
	case 0x1004:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x10d)
		{
			break;
		}
		e->frame = frame;
		blend_to_frame(e, submesh_info_krik, 5, e->resume_frame, 2);
		e->ai_state = e->resume_state;
		break;
	case 0x1005:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x12a)
		{
			break;
		}
		e->frame = frame;
		blend_to_frame(e, submesh_info_krik, 5, e->resume_frame, 2);
		e->ai_state = e->resume_state;
		break;
	case 0x1001:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0xd3)
		{
			break;
		}
		e->frame = frame;
		blend_to_frame(e, submesh_info_krik, 5, e->resume_frame, 2);
		e->ai_state = e->resume_state;
		break;
	case 0x1002:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0xb9)
		{
			break;
		}
		e->frame = frame;
		blend_to_frame(e, submesh_info_krik, 5, e->resume_frame, 2);
		e->ai_state = e->resume_state;
		break;
	case 1:
		e->frame++;
		if(e->frame >= 0x6f)
		{
			e->frame = 0x5f;
		}
		face_camera(e, 0.2f);
		krinkle_steer(e);
		break;
	case 0x201:
		face_camera(e, 0.2f);
		frame = e->frame;
		e->frame = frame + 1;
		switch(frame)
		{
		case 10:
			if(group_near_camera(&e->groups[11], 25.0f))
			{
				hurt_player(e->target, 0x50, 7, 3.1415927f);
			}
			break;
		case 22:
			if(group_near_camera(&e->groups[14], 25.0f))
			{
				hurt_player(e->target, 0x50, 3, 0.0f);
			}
			break;
		case 34:
			if(group_near_camera(&e->groups[11], 25.0f))
			{
				hurt_player(e->target, 0x50, 7, 0.0f);
			}
			break;
		case 47:
			if(group_near_camera(&e->groups[14], 25.0f))
			{
				hurt_player(e->target, 0x50, 3, 0.0f);
			}
			break;
		case 57:
			if(group_near_camera(&e->groups[11], 25.0f))
			{
				hurt_player(e->target, 0x50, 7, 0.0f);
			}
			break;
		case 66:
			if(group_near_camera(&e->groups[14], 25.0f))
			{
				hurt_player(e->target, 0x50, 7, 3.1415927f);
			}
			break;
		case 74:
			if(group_near_camera(&e->groups[11], 25.0f))
			{
				hurt_player(e->target, 0x50, 7, 0.0f);
			}
			break;
		case 89:
			if(group_near_camera(&e->groups[14], 25.0f))
			{
				hurt_player(e->target, 0x50, 7, 0.0f);
			}
			break;
		}
		if(e->frame < 0x5f)
		{
			break;
		}
		krinkle_steer(e);
		break;
	case 0x206:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame == 0x207)
		{
			if(group_near_camera(&e->groups[14], 25.0f))
			{
				hurt_player(e->target, 0x50, 3, 0.0f);
			}
		}
		if(e->frame == 0x212)
		{
			if(group_near_camera(&e->groups[14], 25.0f))
			{
				hurt_player(e->target, 0x50, 3, 0.0f);
			}
		}
		if(e->frame == 0x21c)
		{
			if(group_near_camera(&e->groups[14], 25.0f))
			{
				hurt_player(e->target, 0x50, 3, 0.0f);
			}
		}
		if(e->frame < 0x225)
		{
			break;
		}
		krinkle_steer(e);
		break;
	case 0x204:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame == 0x1ad)
		{
			if(group_near_camera(&e->groups[8], 25.0f))
			{
				hurt_player(e->target, 0x50, 3, 0.0f);
			}
		}
		if(e->frame < 0x1b9)
		{
			break;
		}
		krinkle_steer(e);
		break;
	case 0x205:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame == 0x1c0)
		{
			if(group_near_camera(&e->groups[4], 25.0f))
			{
				hurt_player(e->target, 0x50, 3, 0.0f);
			}
		}
		if(e->frame < 0x1c5)
		{
			break;
		}
		krinkle_steer(e);
		break;
	case 0x203:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame == 0x190 || e->frame == 0x19b)
		{
			if(group_near_camera(&e->groups[11], 25.0f))
			{
				hurt_player(e->target, 0x50, 3, 0.0f);
			}
		}
		if(e->frame < 0x1a7)
		{
			break;
		}
		krinkle_end_path(e);
		break;
	case 4:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame < 0x1da)
		{
			break;
		}
		krinkle_end_path(e);
		break;
	case 0x202:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame == 0x2bf)
		{
			if(e->groups[11].flags & 0x2000)
			{
				throw_at_camera(e->groups[11].pos, WEAPON_PRESENT, e->scale);
				detach_effect(e, &e->groups[11]);
			}
		}
		if(e->frame == 0x2c9)
		{
			if(e->groups[14].flags & 0x2000)
			{
				p = attach_held_object(&e->groups[14], WEAPON_PRESENT, 0.06f, 0.15f, 0.16f, 4.2f, -2.9f, 0.0f, e->scale);
				attach_effect(e, p);
			}
		}
		if(e->frame == 0x2df)
		{
			if(e->groups[14].flags & 0x2000)
			{
				throw_at_camera(e->groups[14].pos, WEAPON_PRESENT, e->scale);
				detach_effect(e, &e->groups[14]);
			}
		}
		if(e->frame < 0x2e5)
		{
			break;
		}
		if(e->groups[11].flags & 0x2000)
		{
			p = attach_held_object(&e->groups[11], WEAPON_PRESENT, 0.06f, 0.15f, 0.16f, 4.2f, -2.9f, 0.0f, e->scale);
			attach_effect(e, p);
		}
		e->ai_state = 6;
		e->counter = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_krik, 5, 0x308, 2);
		break;
	case 6:
		face_camera(e, 0.2f);
		e->frame++;
		if(e->frame < 0x31d)
		{
			break;
		}
		e->counter++;
		if((unsigned short)e->counter < 2)
		{
			e->frame = 0x308;
			break;
		}
		e->ai_state = 0x202;
		e->counter = 0;
		e->frame--;
		blend_to_frame(e, submesh_info_krik, 5, 0x2a9, 2);
		break;
	case 0x207:
		e->frame++;
		if(e->frame == 0x1e8 || e->frame == 0x1f8)
		{
			if(group_near_camera(&e->groups[11], 25.0f))
			{
				hurt_player(e->target, 0x50, 3, 0.0f);
			}
		}
		if(e->frame < 0x1fd)
		{
			break;
		}
		e->frame = 0x1da;
		break;
	case 0x208:
		e->frame++;
		if(e->frame == 0x244 || e->frame == 0x232 || e->frame == 0x256 || e->frame == 0x263)
		{
			if(group_near_camera(&e->groups[11], 25.0f))
			{
				hurt_player(e->target, 0x50, 3, 0.0f);
			}
		}
		if(e->frame < 0x272)
		{
			break;
		}
		e->frame = 0x225;
		break;
	case 0x20a:
		e->frame++;
		if(e->frame == 0x28d)
		{
			if(group_near_camera(&e->groups[11], 25.0f))
			{
				hurt_player(e->target, 0x50, 7, 3.1415927f);
			}
		}
		else if(e->frame == 0x286 || e->frame == 0x296)
		{
			if(group_near_camera(&e->groups[11], 25.0f))
			{
				hurt_player(e->target, 0x50, 0xa, 0.0f);
			}
		}
		if(e->frame < 0x2a9)
		{
			break;
		}
		e->frame = 0x27c;
		break;
	case 3:
		e->frame++;
		if(e->frame < 0x31d)
		{
			break;
		}
		krinkle_steer(e);
		break;
	case 2:
		e->frame++;
		if(e->frame >= 0x6f)
		{
			e->frame = 0x5f;
		}
		break;
	case 0x2001:
		e->frame++;
		if(e->frame >= 0x168)
		{
			e->frame = 0x12a;
		}
		break;
	case 0x2002:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x179)
		{
			break;
		}
		e->frame = frame;
		e->ai_state = 0x4000;
		break;
	case 0x2003:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x308)
		{
			break;
		}
		e->frame = frame;
		e->ai_state = 0x4000;
		break;
	case 0x2004:
		frame = e->frame;
		e->frame = frame + 1;
		if(e->frame < 0x345)
		{
			break;
		}
		e->frame = frame;
		e->ai_state = 0x4000;
		break;
	case 0:
		e->frame++;
		if(e->frame >= 0x345)
		{
			e->frame = 0;
		}
		break;
	case 0x801:
		e->frame++;
		if(e->frame >= 0x6f)
		{
			e->frame = 0x5f;
		}
		path_place(e, krik_path, e->path, 1.0f);
		e->path++;
		if(e->path >= e->unk92)
		{
			krinkle_end_path(e);
		}
		break;
	}
}

/* 0x80127e4c: off the path: keep a playing target, steer at the camera and pick the next
   move from the steering result: 1 wait (3), 2 a strike or a taunt by chance, 0 walk (1) */
void krinkle_steer(enemy_t *e)
{
	player_stats_t *p;
	int	r;
	int	roll;
	short	frame;

	if(e->target->state != 4)
	{
		p = get_target_player();
		if(p->state == 4)
		{
			e->target = p;
		}
	}
	set_goal_near_camera(e, &camera_x, e->goal, 2.0f, 0, -18.0f);
	if(e->unkac)
	{
		r = step_toward_goal_xz(e, 0.0f);
	}
	else
	{
		r = step_toward_goal_xz(e, 1.1f);
	}
	switch(r)
	{
	case 1:
		if(e->ai_state == 3)
		{
			e->frame = 0x308;
			break;
		}
		frame = e->frame;
		if(frame > 0)
		{
			e->frame = frame - 1;
		}
		blend_to_frame(e, submesh_info_krik, 5, 0x308, 2);
		e->ai_state = 3;
		break;
	case 2:
		frame = e->frame;
		if(frame > 0)
		{
			e->frame = frame - 1;
		}
		roll = randrng(100);
		if(roll < 35)
		{
			play_enemy_sound(e, krinkle_attack_sounds[randrng(6)], "krinkle", 0);
			blend_to_frame(e, submesh_info_krik, 5, 0, r);
			e->ai_state = 0x201;
		}
		else if(roll < 70)
		{
			play_enemy_sound(e, krinkle_attack_sounds[randrng(6)], "krinkle", 0);
			blend_to_frame(e, submesh_info_krik, 5, 0x1fd, r);
			e->ai_state = 0x206;
		}
		else if(roll < 85)
		{
			play_enemy_sound(e, krinkle_attack_sounds[randrng(6)], "krinkle", 0);
			blend_to_frame(e, submesh_info_krik, 5, 0x1a7, r);
			e->ai_state = 0x204;
		}
		else
		{
			play_enemy_sound(e, krinkle_attack_sounds[randrng(6)], "krinkle", 0);
			blend_to_frame(e, submesh_info_krik, 5, 0x1b9, r);
			e->ai_state = 0x205;
		}
		break;
	case 0:
		if(e->ai_state == 1)
		{
			break;
		}
		frame = e->frame;
		if(frame > 0)
		{
			e->frame = frame - 1;
		}
		blend_to_frame(e, submesh_info_krik, 5, 0x5f, 2);
		e->ai_state = 1;
		break;
	}
}

/* 0x80128140: the path ran out: the spawn entry's note picks what happens next (5 vanish,
   2 walk, 3 wait, 0x202 and 0x209 the cane throw), anything else steers */
void krinkle_end_path(enemy_t *e)
{
	short	frame;

	switch(e->unk6)
	{
	case 5:
		remove_die(e->id, 0);
		break;
	case 2:
		frame = e->frame;
		e->unk6 = 0;
		e->ai_state = 2;
		if(frame > 0)
		{
			e->frame = frame - 1;
		}
		blend_to_frame(e, submesh_info_krik, 5, 0x5f, 2);
		break;
	case 3:
		frame = e->frame;
		e->unk6 = 0;
		e->ai_state = 3;
		if(frame > 0)
		{
			e->frame = frame - 1;
		}
		blend_to_frame(e, submesh_info_krik, 5, 0x308, 2);
		break;
	case 0x202:
		frame = e->frame;
		e->unk6 = 0;
		e->ai_state = 0x202;
		if(frame > 0)
		{
			e->frame = frame - 1;
		}
		blend_to_frame(e, submesh_info_krik, 5, 0x2a9, 2);
		break;
	case 0x209:
		frame = e->frame;
		e->unk6 = 2;
		e->ai_state = 0x202;
		if(frame > 0)
		{
			e->frame = frame - 1;
		}
		blend_to_frame(e, submesh_info_krik, 5, 0x2a9, 2);
		break;
	default:
		e->unk6 = 0;
		krinkle_steer(e);
		break;
	}
}

extern int	shot_hits_sphere(float *, float, player_stats_t *);	/* objects.c */
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);
extern void	burst_blood(float *, float);
extern void	add_chunks(float *, float);
extern void	add_world_sprite(float *, int, int);	/* sprites.c */
extern void	add_smoke(float *, unsigned int);
extern float	shot_damage_of(player_stats_t *);
extern void	award_points(int, int);
extern int	mark_die();
extern void	detach_group(group_t *, float *, float, float *);	/* model.c: a part flies off */
extern void	replace_group(enemy_t *, group_t *, group_t *);	/* a group takes over another's place */
extern int	violence_level;
int	krinkle_hurt_sounds[5] = { 3, 4, 7, 9, 10 };	/* 0x8019b0a8 */
float	D_8019b088[3] = { 0, 0.05f, 0 };		/* the neck spray's two vectors */
float	D_8019b094[3] = { 0, 0.15f, 0 };

/* 0x8012660c: the hit test */
int krinkle_hit(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		group = find_submesh_hit(e, submesh_info_krik, 0x11, p);
		if(group != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)krinkle_die;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return(0);
}

/* 0x80128290, 0x8012831c: the cane arm's groups and the head's groups fly off */
void krinkle_drop_arm_parts(enemy_t *e)
{
	detach_group(&e->groups[11], e->groups[10].pos, e->y, e->scale);
	detach_group(&e->groups[10], e->groups[10].pos, e->y, e->scale);
	detach_group(&e->groups[9], e->groups[10].pos, e->y, e->scale);
	detach_group(&e->groups[0], e->groups[10].pos, e->y, e->scale);
}

void krinkle_drop_head_parts(enemy_t *e)
{
	detach_group(&e->groups[14], e->groups[13].pos, e->y, e->scale);
	detach_group(&e->groups[13], e->groups[13].pos, e->y, e->scale);
	detach_group(&e->groups[12], e->groups[12].pos, e->y, e->scale);
}

/* 0x801266c8: the shot lands: blood, the heavy weapon tears him apart, damage, points, a
   death (plain, the cane throw's, the head-loss one with the parts flying) or a reaction */
int krinkle_die(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;
	int	group;
	int	i;
	int	gib;
	int	roll;

	e = (enemy_t *)obj->data;
	gib = 0;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return(0);
	}
	burst_blood(p->hit_point, e->unk98);
	add_chunks(e->hit_group->pos, e->unk98);
	add_world_sprite(p->hit_point, 0, 0);
	group = p->hit_group;
	g = &e->groups[group];
	if(p->shot_weapon == 0x1000005)
	{
		for(i = 0; i < 0x11; i++)
		{
			e->groups[i].flags |= 0x20;
			add_smoke(e->groups[i].pivot, 0xd0ff00);
		}
		e->health = 0.01f;
		gib = 1;
	}
	if(p->shot_weapon == 0x1000004)
	{
		if(!(g->flags & 2))
		{
			g->flags |= 2;
			attach_effect(e, attach_flame(0.0f, (submesh_info_krik[group].radius + submesh_info_krik[group].radius) * e->scale[0], g));
		}
	}
	if(violence_level >= 3)
	{
		if(group == 1)
		{
			e->unka8++;
			if(e->unka8 == group)
			{
				e->groups[1].flags |= 0xf0000000;
			}
			else if(e->unka8 < 2)
			{
				if(e->unka8 == 0)
				{
					printf("This should never happen! Zomb head damage at 0\n");
				}
			}
			else if(e->unka8 == 2)
			{
				replace_group(e, &e->groups[1], &e->groups[16]);
			}
		}
		else
		{
			g->flags |= 0xf0000000;
		}
	}
	if(!(e->ai_state & 0x6000))
	{
		e->health -= shot_damage_of(p);
#ifdef PORT_HOST
		{ extern int port_trace; if(port_trace) fprintf(stderr, "[krinkle] hit: health now %.1f state %#x\n", e->health, e->ai_state); }
#endif
		if(e->health <= 0.0f)
		{
			award_points(p->index, 500);
			play_enemy_sound(e, 0xb, "krinkle", 8);
			e->killer = p;
			mark_die(obj->id, p);
			roll = randrng(100);
			if(e->flags2 & 0x40)
			{
				roll = 50;
			}
			if(gib)
			{
				e->ai_state = 0x2004;
				e->frame = 0x31d;
			}
			else if(e->flags2 & 0x20)
			{
				e->ai_state = 0x2003;
				e->frame = 0x2e5;
			}
			else if(roll < 30 && group == 5 && violence_level >= 3)
			{
				krinkle_drop_head_parts(e);
				krinkle_drop_arm_parts(e);
				if(e->groups[1].flags & 0x8000)
				{
					detach_group(&e->groups[1], e->groups[1].pos, e->y, e->scale);
				}
				else
				{
					detach_group(&e->groups[16], e->groups[16].pos, e->y, e->scale);
				}
				replace_group(e, &e->groups[5], &e->groups[15]);
				attach_effect(e, attach_spray(D_8019b094, D_8019b088, &e->groups[15]));
				e->ai_state = 0x2001;
				e->frame = 0x12a;
			}
			else
			{
				e->ai_state = 0x2002;
				e->frame = 0x168;
			}
			e->flags &= 0xfffd;
		}
		else
		{
			award_points(p->index, 50);
			play_enemy_sound(e, krinkle_hurt_sounds[randrng(5)], "krinkle", 2);
		}
	}
	if(e->ai_state & 0xe000)
	{
		return(0);
	}
	if(!(e->flags2 & 0x80) || !(e->unkb0 & 1))
	{
		switch(group)
		{
		case 1:
			start_hit_reaction(e, 0xd3, 0x1003, submesh_info_krik, 5);
			break;
		case 5:
			if(randrng(100) < 50)
			{
				start_hit_reaction(e, 0xb9, 0x1001, submesh_info_krik, 5);
			}
			else
			{
				start_hit_reaction(e, 0x9c, 0x1002, submesh_info_krik, 5);
			}
			break;
		case 6:
		case 7:
		case 8:
			start_hit_reaction(e, 0xe9, 0x1004, submesh_info_krik, 5);
			break;
		case 2:
		case 3:
		case 4:
			start_hit_reaction(e, 0x10d, 0x1005, submesh_info_krik, 5);
			break;
		default:
			start_hit_reaction(e, 0xb9, 0x1001, submesh_info_krik, 5);
			break;
		}
	}
	e->unkb0++;
	return(0);
}
