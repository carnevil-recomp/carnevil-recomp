/*
 * junior.c - Junior, the Freak Show's boss (object type 0x2025): the big baby (BABY.ZM, 942
 * vertices, 1706 faces, 946 normals) or, with the BIG BABY adjustment (58) off, a teddy bear
 * (BEAR.ZM), on the same 15-part animation (baby.za, 685 frames) and the same path (baby.pth,
 * 726 points); his voice is the "baby" bank, his name plate JUNIOR.WMS, the audits BABY
 * START/END. One original file, 0x8011f4e0-0x801222d0, tables in junior_data.c.
 *
 * The file opens with his puke (object type 0x202a, PUKE.ZM): six blobs from his mouth (part 13)
 * that tumble towards the player, 30 damage each if they reach him, 500 points if shot down.
 * Junior walks his path between the script's marks, comes at the player (0x100) and swipes at
 * him (0x201/0x202/0x205/0x206: 190 damage within 70 units of the swinging hand), walks back
 * to his post (0x8101), throws a heavy object (0x207), pukes (0x8203) and staggers (0x8103)
 * when the script asks; a part shot sixteen times makes him flinch (0x1001-0x1006), gory at the
 * highest violence. His speech is lip-synced: each voice line has a run of mouth shapes, one a
 * frame, that switch his face's texture. At 0 health he falls (0x2002), bursts apart in smoke
 * (0x2001, the boss music on) and is gone (0x2003, 0x4000). The script callbacks the movie
 * script calls are at the end of the file. The exported names are still the address names
 * spawn.c, wave.c and the movie script use; each says what it is.
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
#include "effects.h"
#include "weapons.h"

#ifdef PORT_HOST
#define INLINE
#else
#define INLINE inline
#endif

extern void	draw_effects(enemy_t *);			/* run the attached effects */
extern void	think_effects(enemy_t *);
extern void	free_effects(enemy_t *);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, int, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);		/* debug sphere */
extern void	func_800d14a4(float, float);				/* debug map marker */
extern void	func_800c99e4(enemy_t *, int);				/* debug: the groups' frames */
extern void	func_800d326c(void *, void *, void *);			/* a screen triangle */
extern void	set_enemy_scale(enemy_t *, float), set_enemy_health2(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern void	animate_loose_groups(enemy_t *, int);
extern void	blend_to_frame(enemy_t *, submesh_t *, int, int, int);
extern void	start_hit_reaction(enemy_t *, int, int, submesh_t *, int);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern float	shot_damage_shared(player_stats_t *);			/* player.c */
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	award_points(int, int);
extern void	add_world_sprite(float *, int, int);
extern void	add_smoke(float *, unsigned int);
extern void	point_toward_camera(float *, float, float *);
extern void	start_shake(int);					/* shake.c */
extern void	draw_life_bar(float, float, float, float, float, unsigned int);	/* draw2d.c */
extern int	face_camera(enemy_t *, float);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal_xz(enemy_t *, float);
extern int	near_camera(enemy_t *, float);
extern int	group_near_camera(group_t *, float);
extern void	path_place(enemy_t *, void *, short, float);
extern float	func_800fe8dc(float range);				/* a random float in +-range */
extern void	mat_vec_mul(float *, float *, float *);
extern int	randrng(int);
extern int	increment_audit(int);
extern void	remove_surface(texmap_t *, int);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *);
extern void	remove_path(void **, int, char *);
extern int	violence_level;
extern int	D_801e6ed0;				/* adjust.c: adjustment 58, BIG BABY (1: the baby, 0: the bear) */
extern enemy_t	*debug_enemy;
extern char	text_buffer[];

extern texmap_t	baby_texmap[42], bear_texmap[21], puke_texmap[3];	/* junior_data.c */
extern submesh_t baby_anim_blob[15], bear_anim_blob[15], puke_submesh[1];
extern float	puke_spin[3], puke_offset[3];
extern int	*junior_mouth[30];
extern int	junior_voices[14];

#define JUNIOR_GROUPS	15
#define JUNIOR_FRAMES	0x2ad
#define JUNIOR_PATH_LEN	0x2d6
#define JUNIOR_MOUTH	13		/* his face: the mouth shapes switch its texture */
#define JUNIOR_HAND	12		/* the hand his swipes measure, and holds the rattle in */
#define PUKE_NVERTS	0x1a
#define PUKE_NFACES	0x30
#define PUKE_NNORMALS	0x1a

/* blend into frame f of his model as the BIG BABY adjustment has it */
#define JUNIOR_BLEND(e, f)	if(D_801e6ed0) blend_to_frame(e, baby_anim_blob, 7, f, 2); else blend_to_frame(e, bear_anim_blob, 7, f, 2)

float	*baby_verts;			/* 0x801e5818 */
face_t	*baby_faces;			/* 0x80212ddc */
float	*baby_normals;			/* 0x801e0f3c */
float	*bear_verts;			/* 0x801e57e0 */
face_t	*bear_faces;			/* 0x80212dc0 */
float	*bear_normals;			/* 0x801e0f28 */
float	*puke_verts;			/* 0x801e5b84 */
face_t	*puke_faces;			/* 0x8021f0c0 */
float	*puke_normals;			/* 0x801e5844 */
void	*junior_path;			/* 0x8022b114 */
submesh_t *junior_submesh;		/* 0x801e7504: the model's submesh table that was loaded */

enemy_t	*junior = 0;			/* 0x80198f80 */
int	junior_mouth_shape = 0;		/* 0x80198f84: 0-3, the face texture of the frame */
int	D_80198f88 = 0;			/* his burst: smoke on the even frames */
int	junior_last_voice = 0;		/* 0x80198fc4 */
int	junior_thud = 0;		/* 0x80198fc8: his footfalls cycle through three sounds */
float	junior_full_health;		/* 0x801e0ee4 */
float	junior_post[3];			/* 0x802373e8: where he walks back to after a flinch */
int	junior_speaking;		/* 0x8023743c: a mouth run is playing */
int	*junior_mouth_at;		/* 0x801e5cd0: its next shape */
unsigned short	D_801e15a8;		/* his sounds' priority base, kept */
unsigned short	junior_thud_prio;	/* 0x801e5cc0: his footfalls' cue priority */
int	D_801e6c18;			/* the script wants him to stagger next (0x8103) */
int	D_80212d88;			/* the script's last stretch: he throws or comes at the player */
int	D_801e5da4;			/* shots do half damage while it is set and he is below 20% */
/* the shots each part has taken; every sixteenth makes him flinch */
int	D_801e6eb4, D_80237298, D_802372c8, D_8022b120, D_802371ec, D_8022b180, D_80237270;
int	D_8021f0c4, D_8022b110, D_801e0964, D_8022b118;

int	func_8011ffa0(game_object_t *, player_stats_t *);
void	func_8012098c(game_object_t *);
void	func_80121318(enemy_t *, unsigned short);
void	junior_next_move(enemy_t *);
void	func_801216b4(enemy_t *);
void	func_801218a8(enemy_t *);
void	func_8012199c(enemy_t *);
void	func_80121a88(enemy_t *);
int	func_80121b14(enemy_t *);
void	func_80121b70(float, float);
void	func_80121c80(int);
void	func_80121cac(void);
void	func_80121d2c(enemy_t *);
void	func_80121de0(enemy_t *);
void	func_8011f634(game_object_t *);
int	func_8011f6c0(game_object_t *, player_stats_t *);
int	func_8011f74c(game_object_t *, player_stats_t *);
int	func_8011f7a8(game_object_t *);
void	func_8011f948(game_object_t *);
void	func_8011f980(game_object_t *);

/* ---- the puke ---- */

/* 0x8011f4e0: six blobs from his mouth, each on its way with a little spread */
void func_8011f4e0(enemy_t *e)
{
	enemy_t	*p;
	short	id;
	int	n;
	float	dir[3], at[3];

	n = 0;
	do
	{
		id = add_object1_wrapper(0x202a, func_8011f634, func_8011f7a8, func_8011f980);
		if(!id)
		{
			return;
		}
		n++;
		p = get_object_data(id);
		mat_vec_mul(puke_offset, e->groups[JUNIOR_MOUTH].mat, at);
		p->x = e->groups[JUNIOR_MOUTH].pos[0] + at[0];
		p->y = e->groups[JUNIOR_MOUTH].pos[1] + at[1];
		p->z = e->groups[JUNIOR_MOUTH].pos[2] + at[2];
		dir[0] = puke_spin[0] + func_800fe8dc(0.02f);
		dir[1] = puke_spin[1] + func_800fe8dc(0.02f);
		dir[2] = puke_spin[2] + func_800fe8dc(0.02f);
		mat_vec_mul(dir, e->groups[JUNIOR_MOUTH].mat, &p->unkb4);
		p->counter = 0x32;
		p->target = get_target_player();
	} while(n < 6);
}

/* 0x8011f634: a blob's draw callback */
void func_8011f634(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	draw_model(e, puke_submesh, 1, puke_verts, PUKE_NVERTS, puke_faces, PUKE_NFACES, puke_normals, PUKE_NNORMALS);
	if(debug_mode & 0x100)
	{
		func_800c8938(&e->x, e->radius, 0x600000);
	}
}

/* 0x8011f6c0: a blob's hit test: its sphere */
int func_8011f6c0(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(&e->x, e->radius * e->radius, p))
	{
		p->hit = 1;
		p->hit_id = obj->id;
		p->hit_group = 0;
		p->die = (void *)func_8011f74c;
		p->hit_point[0] = p->aim[0];
		p->hit_point[1] = p->aim[1];
		p->hit_point[2] = p->aim[2];
	}
	return 0;
}

/* 0x8011f74c: a blob shot down: 500 points */
int func_8011f74c(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	e->ai_state = 0;
	e->killer = p;
	award_points(p->index, 500);
	remove_game_object(obj->id);
	play_rotating_cue(1, "generic1");
	return 0;
}

/* 0x8011f7a8: a blob's init callback: tumbling at random, one of three colours */
int func_8011f7a8(game_object_t *obj)
{
	enemy_t	*e;
	group_t	*g;
	void	*data;
	int	i;

	data = malloc(sizeof(enemy_t));
	obj->data = data;
	if(!data)
	{
		return 1;
	}
	obj->hit = func_8011f6c0;
	obj->die = func_8011f74c;
	obj->remove = func_8011f948;
	e = (enemy_t *)obj->data;
	e->counter = 0x78;
	e->unk4 = 0x14;
	e->ai_state = 0;
	e->num = 0;
	e->flags = 0xc;
	e->rx = func_800fe8dc(3.14159274f);
	e->ry = func_800fe8dc(3.14159274f);
	e->rz = func_800fe8dc(3.14159274f);
	e->x = 0.0f;
	e->y = 0.0f;
	e->z = 0.0f;
	e->unkcc = 0.112899996f;
	set_enemy_scale(e, func_800fe8dc(0.800000012f) + 1.20000005f);
	e->groups = malloc(sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return 1;
	}
	for(i = 0; i < 1; i++)
	{
		e->groups[i].flags = 0x8000;
	}
	i = randrng(100);
	if(i < 0x21)
	{
		e->groups[0].flags |= 0x10000000;
	}
	else if(i < 0x42)
	{
		e->groups[0].flags |= 0x20000000;
	}
	init_groups(e, puke_submesh, 1);
	g = e->groups;
	e->hit_group = g;
	g->floor = 0.0f;
	return 0;
}

/* 0x8011f948: a blob's remove callback */
void func_8011f948(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	free(e->groups);
	free(e);
}

/* 0x8011f980: a blob's think callback: it tumbles and falls; at the player it hurts him (30),
   otherwise it is gone after 50 frames */
void func_8011f980(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	e->rx = e->rx + 0.150000006f;
	e->ry = e->ry + 0.0700000003f;
	e->x = e->x + e->unkb4;
	e->y = e->y + e->unkb8;
	e->z = e->z + e->unkbc;
	e->unkb8 = e->unkb8 + -0.219999999f;
	if(near_camera(e, 6.0f))
	{
		remove_game_object(obj->id);
		hurt_player(e->target, 0x1e, 0xb, 0.0f);
	}
	else if(--e->counter == 0)
	{
		remove_game_object(obj->id);
	}
}

/* ---- Junior ---- */

/* 0x8011fa7c: the draw callback (junior_draw): his burst in smoke; the mouth shape on his face;
   his model; his life bar and name plate while he lives */
void func_8011fa7c(game_object_t *obj)
{
	enemy_t		*e;
	group_t		*g;
	unsigned int	face;
	int		i;
	float		pt[4];

	e = (enemy_t *)obj->data;
	if(e->ai_state == 0x2001)
	{
		if(D_80198f88 & 1)
		{
			for(i = 0; i < JUNIOR_GROUPS; i++)
			{
				e->groups[i].flags &= ~0x10;
			}
		}
		else
		{
			for(i = 0; i < JUNIOR_GROUPS; i++)
			{
				e->groups[i].flags |= 0x10;
				point_toward_camera(e->groups[i].pivot, 10.0f, pt);
				add_smoke(pt, 0x5f5f6f);
				play_level_music(0x47, "freaksho");
				start_shake(0);
			}
		}
		D_80198f88++;
	}
	if(e->ai_state == 0x2003)
	{
		for(i = 0; i < JUNIOR_GROUPS; i++)
		{
			point_toward_camera(e->groups[i].pivot, 10.0f, pt);
			add_smoke(pt, 0x5f5f6f);
		}
	}
	if(D_801e6ed0)
	{
		g = e->groups;
		face = g[JUNIOR_MOUTH].flags & 0xf000ffff;
		func_80121cac();
		switch(junior_mouth_shape)
		{
		case 0:
			g[JUNIOR_MOUTH].flags = face | 0x4000000;
			break;
		case 2:
			g[JUNIOR_MOUTH].flags = face | 0x1000000;
			break;
		case 1:
			g[JUNIOR_MOUTH].flags = face | 0x2000000;
			break;
		case 3:
			g[JUNIOR_MOUTH].flags = face;
			break;
		}
		draw_model(e, baby_anim_blob, JUNIOR_GROUPS, baby_verts, 0x3ae, baby_faces, 0x6aa, baby_normals, 0x3b2);
	}
	else
	{
		draw_model(e, bear_anim_blob, JUNIOR_GROUPS, bear_verts, 0x3dc, bear_faces, 0x6d6, bear_normals, 0x3db);
	}
	draw_effects(e);
	if(!(e->ai_state & 0x6000))
	{
		if(D_801e6ed0)
		{
			draw_life_bar(e->health, junior_full_health, 100.0f, 300.0f, 248.0f, 0xa0a0ff);
		}
		else
		{
			draw_life_bar(e->health, junior_full_health, 100.0f, 300.0f, 248.0f, 0xcc9240);
		}
		func_80121b70(200.0f, 233.0f);
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

/* 0x8011fee4: the hit test (junior_hit): the bounding sphere, then the submesh */
int func_8011fee4(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		if((group = find_submesh_hit(e, junior_submesh, JUNIOR_GROUPS, p)) != -1)
		{
			p->hit = 1;
			p->hit_id = obj->id;
			p->hit_group = group;
			p->die = (void *)func_8011ffa0;
			p->hit_point[0] = p->aim[0];
			p->hit_point[1] = p->aim[1];
			p->hit_point[2] = p->aim[2];
		}
	}
	return 0;
}

/* 0x8011ffa0: a shot lands (junior_die): a hit mark; every sixteenth shot on a part makes him
   flinch from it (gory at the highest violence; his face also twitches back); the damage (half
   while D_801e5da4 holds and he is below a fifth), 50 points a shot, 2000 for the last */
int func_8011ffa0(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;
	int	reaction, frame;
	float	before, damage;

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return 0;
	}
	add_world_sprite(p->hit_point, 0, 0);
	reaction = 0;
	g = &e->groups[p->hit_group];
	frame = 0;
	switch(p->hit_group)
	{
	case 13:
		if(e->ai_state != 0x8203)
		{
			e->unk9c = 1.04719734f;
		}
		if(!(++D_801e6eb4 & 0xf))
		{
			if(violence_level >= 3)
			{
				g->flags |= 0xf0000000;
			}
			reaction = 0x1001;
			frame = 0x1cd;
		}
		break;
	case 1:
		if(!(++D_80237298 & 0xf))
		{
			if(violence_level >= 3)
			{
				g->flags |= 0xf0000000;
			}
			reaction = 0x1004;
			frame = 0x200;
		}
		break;
	case 2:
		if(!(++D_802372c8 & 0xf))
		{
			if(violence_level >= 3)
			{
				g->flags |= 0xf0000000;
			}
			reaction = 0x1006;
			frame = 0x1e7;
		}
		break;
	case 4:
		if(!(++D_8022b120 & 0xf))
		{
			if(violence_level >= 3)
			{
				g->flags |= 0xf0000000;
			}
			reaction = 0x1004;
			frame = 0x200;
		}
		break;
	case 5:
		if(!(++D_802371ec & 0xf))
		{
			if(violence_level >= 3)
			{
				g->flags |= 0xf0000000;
			}
			reaction = 0x1006;
			frame = 0x1e7;
		}
		break;
	case 10:
		if(!(++D_8022b180 & 0xf))
		{
			if(violence_level >= 3)
			{
				g->flags |= 0xf0000000;
			}
			reaction = 0x1003;
			frame = 0x188;
		}
		break;
	case 0:
		if(!(++D_80237270 & 0xf))
		{
			if(violence_level >= 3)
			{
				g->flags |= 0xf0000000;
			}
			reaction = 0x1005;
			frame = 0x1ad;
		}
		break;
	case 11:
		if(!(++D_8021f0c4 & 0xf))
		{
			if(violence_level >= 3)
			{
				g->flags |= 0xf0000000;
			}
			reaction = 0x1003;
			frame = 0x188;
		}
		break;
	case 3:
		if(!(++D_8022b110 & 0xf))
		{
			if(violence_level >= 3)
			{
				g->flags |= 0xf0000000;
			}
			reaction = 0x1005;
			frame = 0x1ad;
		}
		break;
	case 9:
		if(!(++D_801e0964 & 0xf))
		{
			if(violence_level >= 3)
			{
				g->flags |= 0xf0000000;
			}
			reaction = 0x1002;
			frame = 0x16c;
		}
		break;
	case 7:
		if(!(++D_8022b118 & 0xf))
		{
			if(violence_level >= 3)
			{
				g->flags |= 0xf0000000;
			}
			reaction = 0x1002;
			frame = 0x16c;
		}
		break;
	}
	if(!(e->ai_state & 0x6000) && e->health > 0.0f)
	{
		before = e->health;
		damage = shot_damage_shared(p);
		if(junior->health / junior_full_health < 0.200000003f && D_801e5da4)
		{
			damage = damage * 0.5f;
		}
		e->health = e->health - damage;
		if(e->health <= 0.0f)
		{
			e->health = 0.0f;
		}
		else
		{
			award_points(p->index, 50);
		}
		if(e->health <= 0.0f && before > 0.0f)
		{
			award_points(p->index, 2000);
		}
	}
	if(e->ai_state & 0xe000)
	{
		return 0;
	}
	if(reaction)
	{
		start_hit_reaction(e, frame, reaction, junior_submesh, 7);
		func_80121d2c(e);
	}
	return 0;
}

/* 0x801203f0: his texmaps (the baby's or the bear's) and the puke's (sic: the game's name) */
void load_junoir_texmap(void)
{
	if(D_801e6ed0)
	{
		load_texmap(baby_texmap, 0x2a, "baby");
	}
	else
	{
		load_texmap(bear_texmap, 0x15, "bear");
	}
	load_texmap(puke_texmap, 3, "puke");
}

/* 0x80120460: his model, animation and path, and the puke's model */
void load_junior(void)
{
	if(D_801e6ed0)
	{
		load_model((void **)&baby_verts, 0x3ae, (void **)&baby_faces, 0x6aa, (void **)&baby_normals, 0x3b2, "BABY.ZM", 0x6f51d615);
		load_anim(baby_anim_blob, JUNIOR_GROUPS, JUNIOR_FRAMES, "baby.za", (int)0xa05090da);
		junior_submesh = baby_anim_blob;
	}
	else
	{
		load_model((void **)&bear_verts, 0x3dc, (void **)&bear_faces, 0x6d6, (void **)&bear_normals, 0x3db, "BEAR.ZM", 0x06af2364);
		load_anim(bear_anim_blob, JUNIOR_GROUPS, JUNIOR_FRAMES, "baby.za", (int)0xa05090da);
		junior_submesh = bear_anim_blob;
	}
	load_path(&junior_path, JUNIOR_PATH_LEN, "baby.pth", 0x5072ec42);
	load_model((void **)&puke_verts, PUKE_NVERTS, (void **)&puke_faces, PUKE_NFACES, (void **)&puke_normals, PUKE_NNORMALS, "PUKE.ZM", 0x7a169db2);
}

/* 0x801205cc: all his assets go when the level is unloaded */
void remove_junior_texmap(void)
{
	if(D_801e6ed0)
	{
		remove_surface(baby_texmap, 0x2a);
	}
	else
	{
		remove_surface(bear_texmap, 0x15);
	}
	remove_surface(puke_texmap, 3);
	remove_path(&junior_path, JUNIOR_PATH_LEN, "baby.pth");
	remove_animation(junior_submesh, JUNIOR_GROUPS, JUNIOR_FRAMES, "baby.za");
	if(D_801e6ed0)
	{
		remove_model((void **)&baby_verts, 0x3ae, (void **)&baby_faces, 0x6aa, (void **)&baby_normals, 0x3b2, "BABY.ZM");
	}
	else
	{
		remove_model((void **)&bear_verts, 0x3dc, (void **)&bear_faces, 0x6d6, (void **)&bear_normals, 0x3db, "BEAR.ZM");
	}
	remove_model((void **)&puke_verts, PUKE_NVERTS, (void **)&puke_faces, PUKE_NFACES, (void **)&puke_normals, PUKE_NNORMALS, "PUKE.ZM");
}

/* 0x80120714: the init callback (junior_init): the record (facing the player, twice the size,
   400 health by difficulty), fifteen groups (his face turnable), the flinch counters cleared,
   then lying in his first state 0x2002 */
int func_80120714(game_object_t *obj)
{
	enemy_t	*e;
	group_t	*g;
	int	i;

	obj->data = malloc(sizeof(enemy_t));
	if(!obj->data)
	{
		return 1;
	}
	obj->hit = func_8011fee4;
	obj->die = func_8011ffa0;
	obj->remove = func_8012098c;
	e = (enemy_t *)obj->data;
	e->unk4 = 0x14;
	junior = e;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0xc;
	e->target = get_target_player();
	e->unk54 = 0;
	e->rx = 0.0f;
	e->rz = 0.0f;
	e->x = 0.0f;
	e->y = 0.0f;
	e->ry = 3.14159274f;
	e->z = 20.0f;
	e->unkcc = 2.31999993f;
	set_enemy_scale(e, 2.0f);
	set_enemy_health2(e, 400.0f);
	junior_full_health = e->health;
	e->groups = malloc(JUNIOR_GROUPS * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return 1;
	}
	for(i = 0; i < JUNIOR_GROUPS; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	e->groups[JUNIOR_MOUTH].flags = 0x8001;
	e->unk9c = 0.0f;
	e->groups[JUNIOR_MOUTH].rot[0] = 0.0f;
	e->groups[JUNIOR_MOUTH].rot[1] = e->unk9c;
	e->groups[JUNIOR_MOUTH].rot[2] = 0.0f;
	e->groups[JUNIOR_MOUTH].flags |= 0x1000000;
	init_groups(e, junior_submesh, JUNIOR_GROUPS);
	junior_speaking = 0;
	junior_mouth_shape = 0;
	D_801e6eb4 = 0;
	D_801e0964 = 0;
	D_8022b118 = 0;
	D_80237298 = 0;
	D_802372c8 = 0;
	D_8022b120 = 0;
	D_802371ec = 0;
	D_8022b180 = 0;
	D_80237270 = 0;
	D_8021f0c4 = 0;
	D_8022b110 = 0;
	D_801e6c18 = 0;
	D_80212d88 = 0;
	D_801e5da4 = 1;
	g = e->groups;
	e->hit_group = &g[7];
	g[7].floor = 0.0f;
	e->unk98 = 0.0f;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = cue_priority(3);
	D_801e15a8 = 0x40;
	junior_thud_prio = cue_priority(4);
	func_80121318(e, 0x2002);
	return 0;
}

/* 0x8012098c: the remove callback (junior_remove) */
void func_8012098c(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	junior = 0;
	if(debug_enemy == e)
	{
		debug_enemy = 0;
	}
	free_effects(e);
	free(e->groups);
	free(e);
}

/* 0x801209ec: the think callback (junior_think): his face settles back; the states run their
   frames; the flinches end in func_8012199c, the swipes in func_801218a8, the walks and the
   throw in junior_next_move */
void func_801209ec(game_object_t *obj)
{
	enemy_t	*e;
	int	i;
	float	unused[10];		/* 40 bytes the original's frame reserves and never touches */

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		return;
	}
	e->groups[JUNIOR_MOUTH].rot[1] = e->unk9c;
	e->unk9c = e->unk9c - 0.100000001f;
	if(e->unk9c < 0.0f)
	{
		e->unk9c = 0.0f;
	}
	think_effects(e);
	animate_loose_groups(e, JUNIOR_GROUPS);
	if(e->flags & 2)
	{
		return;
	}
	switch(e->ai_state)
	{
	case 0x1002:
		if(++e->frame >= 0x188)
		{
			e->frame--;
			func_8012199c(e);
		}
		break;
	case 0x1001:
		if(++e->frame >= 0x1e7)
		{
			e->frame--;
			func_8012199c(e);
		}
		break;
	case 0x1003:
		if(++e->frame >= 0x1ad)
		{
			e->frame--;
			func_8012199c(e);
		}
		break;
	case 0x1005:
		if(++e->frame >= 0x1cd)
		{
			e->frame--;
			func_8012199c(e);
		}
		break;
	case 0x1004:
		if(++e->frame >= 0x216)
		{
			e->frame--;
			func_8012199c(e);
		}
		break;
	case 0x1006:
		if(++e->frame >= 0x200)
		{
			e->frame--;
			func_8012199c(e);
		}
		break;
	case 0x100:
		e->frame++;
		func_80121de0(e);
		if(e->frame >= 0x16)
		{
			e->frame = 0;
		}
		face_camera(e, 0.200000003f);
		set_goal_near_camera(e, &camera_x, e->goal, 0.0f, 0.0f, -75.0f);
		if(step_toward_goal_xz(e, 4.0f) == 2)
		{
			func_801216b4(e);
		}
		break;
	case 0x8101:
		e->frame++;
		func_80121de0(e);
		if(e->frame >= 0x16c)
		{
			e->frame = 0x15a;
		}
		face_camera(e, 0.200000003f);
		e->goal[0] = junior_post[0];
		e->goal[1] = junior_post[1];
		e->goal[2] = junior_post[2];
		if(step_toward_goal_xz(e, 3.0f) == 2)
		{
			junior_next_move(e);
		}
		break;
	case 0x8203:
		if(++e->frame == 0x49)
		{
			play_enemy_sound(e, 0x12, "baby", 4);
			func_80121c80(0x12);
		}
		if((unsigned short)e->frame >= 0x5b && (unsigned short)e->frame <= 0x63)
		{
			func_8011f4e0(e);
		}
		if(e->frame >= 0x8d)
		{
			func_80121a88(e);
		}
		break;
	case 0x201:
		if(++e->frame == 0x122 && group_near_camera(&e->groups[8], 70.0f))
		{
			hurt_player(e->target, 0xbe, 3, 0.0f);
		}
		if(e->frame >= 0x13b)
		{
			func_801218a8(e);
		}
		break;
	case 0x202:
		if(++e->frame == 0x147 && group_near_camera(&e->groups[JUNIOR_HAND], 70.0f))
		{
			hurt_player(e->target, 0xbe, 3, 0.0f);
		}
		if(e->frame >= 0x15a)
		{
			func_801218a8(e);
		}
		break;
	case 0x205:
		if(++e->frame == 0x220 && group_near_camera(&e->groups[JUNIOR_HAND], 70.0f))
		{
			hurt_player(e->target, 0xbe, 3, 0.0f);
		}
		if(e->frame >= 0x229)
		{
			func_801218a8(e);
		}
		break;
	case 0x206:
		if(++e->frame == 0x236 && group_near_camera(&e->groups[JUNIOR_HAND], 70.0f))
		{
			hurt_player(e->target, 0xbe, 3, 0.0f);
		}
		if(e->frame >= 0x23b)
		{
			func_801218a8(e);
		}
		break;
	case 0x104:
		func_80121de0(e);
		if(++e->frame >= 0xe1)
		{
			func_80121d2c(e);
			e->frame = 0x16;
			e->ai_state = 0x102;
		}
		break;
	case 0x8802:
		e->frame++;
		func_80121de0(e);
		if(e->frame >= 0x16)
		{
			e->frame = 0;
		}
		path_place(e, junior_path, e->path, 1.0f);
		if(++e->path >= e->unk92)
		{
			e->path = e->unk92 - 1;
			junior_post[0] = junior->x;
			junior_post[1] = junior->y;
			junior_post[2] = junior->z;
			junior_next_move(e);
		}
		break;
	case 0x8803:
		if(++e->frame >= 0x118)
		{
			e->frame = 0xe1;
		}
		goto walk;
	case 0x102:
		if(++e->frame >= 0x3d)
		{
			junior_next_move(e);
		}
		break;
	case 0x8103:
		if(++e->frame >= 0x3d)
		{
			e->frame = 0x16;
		}
		break;
	case 0x2002:
		if(++e->frame >= 0xba)
		{
			e->ai_state = 0x2001;
			e->frame = 0x23b;
			e->counter = 0;
		}
		goto walk;
	case 0x2001:
		if(++e->frame >= 0x241)
		{
			if(++e->counter >= 4)
			{
				e->ai_state = 0x2003;
				for(i = 0; i < JUNIOR_GROUPS; i++)
				{
					e->groups[i].flags |= 4;
				}
			}
			else
			{
				e->frame = 0x23b;
			}
		}
		goto walk;
	case 0x2003:
		if(++e->frame >= 0x273)
		{
			e->frame--;
			e->ai_state = 0x4000;
		}
		/* fall through */
	case 0x4000:
	case 0x8801:
	walk:
		path_place(e, junior_path, e->path, 1.0f);
		if(++e->path >= e->unk92)
		{
			e->path = e->unk92 - 1;
		}
		break;
	case 0x207:
		if(++e->frame == 0x279)
		{
			e->unkac = randrng(3) + 0x13;
			attach_effect(e, attach_held_object(&e->groups[6], e->unkac, 0.349999994f, -0.200000003f, 0.0f, 1.57079601f, 0.0f, 3.14159274f, e->scale));
			func_80121d2c(e);
		}
		if(e->frame == 0x29d)
		{
			throw_heavy(e->groups[6].pos, e->unkac, e->scale);
			detach_effect(e, &e->groups[6]);
		}
		if(e->frame >= 0x2ad)
		{
			junior_next_move(e);
		}
		break;
	case 0:
		if(++e->frame >= 0x241)
		{
			e->frame = 0x23b;
		}
		break;
	}
}

/* 0x80121318: put him in a state (junior_setup): 0x8801 walks on with the rattle in his hand
   and a line of speech (the audit BABY START) */
void func_80121318(enemy_t *e, unsigned short state)
{
	e->ai_state = state;
	switch(state)
	{
	case 0x2002:
		e->frame = 0x8d;
		break;
	case 0x8801:
		increment_audit(0x3e);
		attach_effect(e, attach_held_object(&e->groups[JUNIOR_HAND], 0x15, 0.349999994f, 0.200000003f, 0.0f, 1.57079601f, 0.0f, 3.14159274f, e->scale));
		play_enemy_sound(e, 0x1c, "baby", 4);
		func_80121c80(0x1c);
		e->frame = 0x216;
		path_place(e, junior_path, e->path, 1.0f);
		break;
	case 0x102:
		e->frame = 0x16;
		break;
	case 0:
	default:
		e->frame = 0;
		break;
	}
}

/* 0x8012142c: his next move (the game's chose_junoir_animation?): a stagger the script asked
   for; in the last stretch a throw (65%) or a walk at the player; otherwise a walk at the
   player (85%) or a stand */
void junior_next_move(enemy_t *e)
{
	if(D_801e6c18)
	{
		D_801e6c18 = 0;
		func_80121d2c(e);
		if(e->frame > 0)
		{
			e->frame--;
		}
		JUNIOR_BLEND(e, 0x16);
		e->ai_state = 0x8103;
	}
	else if(D_80212d88)
	{
		D_801e5da4 = 0;
		if(randrng(100) < 0x41)
		{
			e->target = get_target_player();
			if(e->frame > 0)
			{
				e->frame--;
			}
			JUNIOR_BLEND(e, 0x273);
			e->ai_state = 0x207;
		}
		else
		{
			func_80121d2c(e);
			e->target = get_target_player();
			if(e->frame > 0)
			{
				e->frame--;
			}
			JUNIOR_BLEND(e, 0);
			e->ai_state = 0x100;
		}
		e->target = get_target_player();
		face_camera(e, 0.200000003f);
	}
	else if(randrng(100) < 0x55)
	{
		func_80121d2c(e);
		e->target = get_target_player();
		if(e->frame > 0)
		{
			e->frame--;
		}
		JUNIOR_BLEND(e, 0);
		e->ai_state = 0x100;
		e->target = get_target_player();
		face_camera(e, 0.200000003f);
	}
	else
	{
		func_80121d2c(e);
		if(e->frame > 0)
		{
			e->frame--;
		}
		JUNIOR_BLEND(e, 0x16);
		e->ai_state = 0x102;
	}
}

/* 0x801216b4: at the player he swipes, one of four at random */
void func_801216b4(enemy_t *e)
{
	int	r, state;

	r = randrng(100);
	if(r < 25)
	{
		func_80121d2c(e);
		if(e->frame > 0)
		{
			e->frame--;
		}
		JUNIOR_BLEND(e, 0x118);
		state = 0x201;
	}
	else if(r < 50)
	{
		func_80121d2c(e);
		if(e->frame > 0)
		{
			e->frame--;
		}
		JUNIOR_BLEND(e, 0x13b);
		state = 0x202;
	}
	else if(r < 75)
	{
		func_80121d2c(e);
		if(e->frame > 0)
		{
			e->frame--;
		}
		JUNIOR_BLEND(e, 0x217);
		state = 0x205;
	}
	else
	{
		func_80121d2c(e);
		if(e->frame > 0)
		{
			e->frame--;
		}
		JUNIOR_BLEND(e, 0x229);
		state = 0x206;
	}
	e->ai_state = state;
	e->target = get_target_player();
	face_camera(e, 0.200000003f);
}

INLINE void func_80121908(enemy_t *);

/* 0x801218a8: after a swipe: back to his post (35%, or when the script wants a stagger), else
   another swipe */
void func_801218a8(enemy_t *e)
{
	if(randrng(100) < 0x23 || D_801e6c18)
	{
		func_80121908(e);
	}
	else
	{
		func_801216b4(e);
	}
}

/* 0x80121908: walk back to his post */
INLINE void func_80121908(enemy_t *e)
{
	func_80121d2c(e);
	if(e->frame > 0)
	{
		e->frame--;
	}
	JUNIOR_BLEND(e, 0x15a);
	e->ai_state = 0x8101;
	face_camera(e, 0.200000003f);
}

/* 0x8012199c: a flinch is over: at his post he takes up what he was doing, else he walks back */
void func_8012199c(enemy_t *e)
{
	if(func_80121b14(e))
	{
		JUNIOR_BLEND(e, e->resume_frame);
		e->ai_state = e->resume_state;
	}
	else
	{
		func_80121908(e);
	}
}

/* 0x80121a88: after the puke, a stagger */
void func_80121a88(enemy_t *e)
{
	D_801e6c18 = 0;
	func_80121d2c(e);
	if(e->frame > 0)
	{
		e->frame--;
	}
	JUNIOR_BLEND(e, 0x16);
	e->ai_state = 0x8103;
}

/* 0x80121b14: whether he stands at his post */
int func_80121b14(enemy_t *e)
{
	return junior_post[0] == e->x && junior_post[1] == e->y && junior_post[2] == e->z;
}

/* 0x80121b70: his name plate beside the life bar (texture 0x7084, JUNIOR.WMS), 128 by 32
   around x, y */
void func_80121b70(float x, float y)
{
	struct { float x, y, oow, sow, tow, pad; } v[4];

	guTexSource(find_surface(0x7084)->handle);
	guColorCombineFunction(4);
	guAlphaSource(2);
	grAlphaTestReferenceValue(4);
	v[0].x = x - 64.0f;
	v[0].y = y - 16.0f;
	v[0].oow = 0.909090877f;
	v[0].sow = 0;
	v[0].tow = 57.2727242f;
	v[1].x = x + 64.0f;
	v[1].y = y - 16.0f;
	v[1].oow = 0.909090877f;
	v[1].sow = 231.818176f;
	v[1].tow = 57.2727242f;
	v[2].x = x - 64.0f;
	v[2].y = y + 16.0f;
	v[2].oow = 0.909090877f;
	v[2].sow = 0;
	v[2].tow = 0;
	v[3].x = x + 64.0f;
	v[3].y = y + 16.0f;
	v[3].oow = 0.909090877f;
	v[3].sow = 231.818176f;
	v[3].tow = 0;
	func_800d326c(&v[0], &v[3], &v[1]);
	func_800d326c(&v[0], &v[2], &v[3]);
}

/* 0x80121c80: start the mouth run of "baby" call n */
INLINE void func_80121c80(int call)
{
	junior_speaking = 1;
	junior_mouth_at = junior_mouth[call];
}

/* 0x80121cac: a frame of the mouth run (from his draw): its shape, until the -1 */
void func_80121cac(void)
{
	int	shape;

	if(junior_speaking)
	{
		shape = *junior_mouth_at++;
		if(shape == -1)
		{
			junior_speaking = 0;
		}
		else
		{
			junior_mouth_shape = shape;
		}
	}
}

/* 0x80121d00: whether he still has health (nothing calls it) */
int func_80121d00(void)
{
	return junior->health > 0.0f;
}

/* 0x80121d2c: a line of speech: a voice line other than the last one at random (after a line
   the next call is 0, a silent one), lip-synced */
void func_80121d2c(enemy_t *e)
{
	int	call;

	call = 0;
	if(!junior_last_voice)
	{
		do
		{
			call = junior_voices[randrng(14)];
		} while(call == junior_last_voice);
	}
	junior_last_voice = call;
	play_enemy_sound(e, call, "baby", 0);
	func_80121c80(call);
}

/* 0x80121de0: his footfalls on the walk and swipe frames: a thud (three in turn) and a shake */
void func_80121de0(enemy_t *e)
{
	switch(e->frame)
	{
	case 5:
	case 0xb:
	case 0xf:
	case 0x14:
	case 0xbf:
	case 0xd5:
	case 0xe0:
	case 0x161:
	case 0x16c:
		play_cue_flags_noint(junior_thud + 2, "freaksho", junior_thud_prio);
		start_shake(0);
		if(++junior_thud >= 3)
		{
			junior_thud = 0;
		}
		break;
	}
}

/* ---- the Freak Show movie script's callbacks for him ---- */

/* 0x80121ed4: his entrance: on his path at point 60, then at the stage front in 0x104, his
   post there */
void sub_80121ed4(void)
{
	path_place(junior, junior_path, 0x3c, 1.0f);
	junior->x = 483.799988f;
	junior->frame = 0xba;
	junior->ai_state = 0x104;
	junior->y = -67.3700027f;
	junior->z = 2413.13501f;
	junior_post[0] = junior->x;
	junior_post[1] = junior->y;
	junior_post[2] = junior->z;
}

/* 0x80121f68: a gate: false while he staggers; once below 82% the next move is a stagger */
int sub_80121f68(void)
{
	if(junior->ai_state == 0x8103)
	{
		return 0;
	}
	if(junior->health / junior_full_health < 0.819999993f)
	{
		D_801e6c18 = 1;
	}
	return 1;
}

/* 0x80121fbc: walk path points 0x3d-0x95 */
void sub_80121fbc(void)
{
	junior->ai_state = 0x8802;
	junior->unk90 = 0x3d;
	junior->unk92 = 0x95;
	junior->path = 0x3d;
	junior->frame = 0;
}

/* 0x80121fe8: a line (call 13), then walk path points 0x96-0xd8 in 0x8803 */
void sub_80121fe8(void)
{
	play_enemy_sound(junior, 0xd, "baby", 4);
	func_80121c80(0xd);
	junior->ai_state = 0x8803;
	junior->unk90 = 0x96;
	junior->unk92 = 0xd8;
	junior->path = 0x96;
	junior->frame = 0xe1;
}

/* 0x80122060: walk on */
void sub_80122060(void)
{
	junior->ai_state = 0x8802;
	junior->frame = 0;
}

/* 0x80122078: a gate: false while he staggers; once below 60% the next move is a stagger */
int sub_80122078(void)
{
	if(junior->ai_state == 0x8103)
	{
		return 0;
	}
	if(junior->health / junior_full_health < 0.600000024f)
	{
		D_801e6c18 = 1;
	}
	return 1;
}

/* 0x801220cc: walk path points 0xd9-0x16b */
void sub_801220cc(void)
{
	junior->ai_state = 0x8802;
	junior->unk90 = 0xd9;
	junior->unk92 = 0x16b;
	junior->path = 0xd9;
	junior->frame = 0;
}

/* 0x801220f8: his post where he stands */
void sub_801220f8(void)
{
	junior_post[0] = junior->x;
	junior_post[1] = junior->y;
	junior_post[2] = junior->z;
}

/* 0x8012212c: his post where he stands, and the puke */
void sub_8012212c(void)
{
	junior_post[0] = junior->x;
	junior_post[1] = junior->y;
	junior_post[2] = junior->z;
	junior->ai_state = 0x8203;
	junior->frame = 0x3d;
}

/* 0x8012216c: a gate: while he is not staggering */
int sub_8012216c(void)
{
	return junior->ai_state != 0x8103;
}

/* 0x80122184: walk path points 0x16c-0x1d1, his post where he stands */
void sub_80122184(void)
{
	junior->ai_state = 0x8802;
	junior->unk90 = 0x16c;
	junior->unk92 = 0x1d1;
	junior->path = 0x16c;
	junior->frame = 0;
	junior_post[0] = junior->x;
	junior_post[1] = junior->y;
	junior_post[2] = junior->z;
}

/* 0x801221d8: an empty callback */
void sub_801221d8(void)
{
}

/* 0x801221e0: the last stretch: walk path points 0x1d2-0x2d5 */
void sub_801221e0(void)
{
	D_80212d88 = 1;
	junior->ai_state = 0x8802;
	junior->unk90 = 0x1d2;
	junior->unk92 = 0x2d5;
	junior->path = 0x1d2;
	junior->frame = 0;
}

/* 0x80122218: a gate: false while he staggers; at 0 health the next move is a stagger */
int sub_80122218(void)
{
	if(junior->ai_state == 0x8103)
	{
		return 0;
	}
	if(junior->health <= 0.0f)
	{
		D_801e6c18 = 1;
	}
	return 1;
}

/* 0x8012225c: he is beaten (the audit BABY END): he falls (0x2002) with his last line (call 9) */
void sub_8012225c(void)
{
	increment_audit(0x3f);
	junior->ai_state = 0x2002;
	junior->frame = 0x8d;
	junior->flags &= ~2;
	play_enemy_sound(junior, 9, "baby", 8);
	func_80121c80(9);
}
