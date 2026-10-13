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

extern void	func_800fd9ac(enemy_t *);			/* run the attached effects */
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, int, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);		/* objects.c */
extern void	func_800fb4a0(enemy_t *, float);		/* health by difficulty */
extern player_stats_t *get_target_player(void);		/* objects.c */
extern int	randrng(int);
extern void	*func_8010eefc(group_t *, int, float, float, float, float, float, int, float *);
extern void	func_800fd990(enemy_t *, void *);
extern void	func_800fb70c(enemy_t *, void *, short, float);	/* start along a path */
extern void	func_800fda88(enemy_t *);
extern int	func_8012660c(game_object_t *, enemy_t *);	/* hit */
extern void	func_801266c8(enemy_t *);			/* die */
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
	func_800fd9ac(e);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, (int)e->hit_group->pos[0] + 8, 0xffffff);
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
			draw_string_at_color(0, text_buffer, (int)e->hit_group->pos[0] + 8, 0xff0000);
		}
		else
		{
			draw_string_at_color(0, text_buffer, (int)e->hit_group->pos[0] + 8, 0xffffff);
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
	void	*p;

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
		p = func_8010eefc(&e->groups[11], 0x24, 0.06f, 0.15f, 0.16f, 4.2f, -2.9f, 0, e->scale);
		func_800fd990(e, p);
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
		func_800fb70c(e, krik_path, e->path, 1.0f);
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

	e = malloc(0xec);
	obj->data = (short *)e;
	if(!e)
	{
		return(1);
	}
	obj->unused18 = (int)(long)func_8012660c;
	obj->unused1c = (int)(long)func_801266c8;
	obj->remove = krinkle_remove;
	e->unk4 = 0x14;
	e->unk2 = 0;
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
	func_800fb4a0(e, 5.5f);
	e->groups = malloc(0xbb0);
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
	e->groups[5].unk74 = 0;
	e->unk98 = 0;
	e->effects = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->prio_base = 0x40;
	e->prio_flags = next_cue_priority();
	krinkle_setup(e, 0);
	play_enemy_sound((snd_object_t *)e, krinkle_sounds[randrng(2)], "krinkle", -2);
	return(0);
}

/* 0x80126f5c: the remove callback: a last sound unless already dying, the effects, the record */
void krinkle_remove(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(!(e->ai_state & 0x4000))
	{
		play_enemy_sound((snd_object_t *)e, 0xb, "krinkle", 8);
	}
	if(debug_enemy == e)
	{
		debug_enemy = 0;
	}
	func_800fda88(e);
	free(e->groups);
	free(e);
}
