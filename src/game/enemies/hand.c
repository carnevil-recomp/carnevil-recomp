/*
 * hand.c - the Haunted House hand (object type 0x2008; one original file, 0x800f70b8-0x800f7984,
 * its tables at 0x8018ffa0). A two-submesh model (HAND.ZM: 358 vertices, 352 normals, 576
 * faces) that plays hand.za (171 frames) on a loop from frame 59, dies to one shot, and
 * hurts the player when the background movie reaches the frame its spawn entry is tied to.
 * (wave.c's header calls 0x800f70b8-0x800f7420 the jester's: it is this file.)
 */
#include <stdlib.h>
#include <glide/glide.h>
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "texmap.h"
#include "credits.h"			/* debug_mode, player_stats_t */
#include "movie.h"

extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, int, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);
extern void	func_800d14a4(float, float);
extern void	func_800c99e4(enemy_t *, int);
extern void	set_enemy_scale(enemy_t *, float);
extern void	set_enemy_health(enemy_t *, float);
extern player_stats_t *get_target_player(void);
extern void	clear_enemy_fields(enemy_t *);
extern void	func_8011464c(enemy_t *, submesh_t *, int);	/* model.c: pose the groups at the current frame */
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern void	burst_blood(float *, float, int);
extern void	add_world_sprite(float *, int, int);
extern int	remove_die(int, player_stats_t *);
extern void	award_points(int, int);
extern void	hurt_player(player_stats_t *, int, int, float);
extern void	remove_animation(submesh_t *, int, int, char *), remove_model(void **, int, void **, int, void **, int, char *);
extern enemy_t	*debug_enemy;
extern char	text_buffer[];
extern movie_t	movie;

extern texmap_t	hand_texmap[4];			/* hand_data.c */
extern submesh_t submesh_info_hand[2];

float	*hand_verts;			/* 0x801e57f8 */
face_t	*hand_faces;			/* 0x80212dcc */
float	*hand_normals;			/* 0x801e0f30 */
int	hand_spare;			/* 0x80190060: in the file's data, never used */
int	hand_loaded;			/* 0x80190064: set while the model is loaded */

int	func_800f7420(game_object_t *, player_stats_t *);

/* 0x800f70b8: the draw callback (keying off, the model, the debug overlays) */
void func_800f70b8(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	draw_model(e, submesh_info_hand, 2, hand_verts, 0x166, hand_faces, 0x240, hand_normals, 0x160);
	if(debug_mode & 0x400)
	{
		graphical_printf(text_buffer, "%d", e->num);
		if(e->ai_state == 0x101)
		{
			draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xff0000);
		}
		else
		{
			draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xffffff);
		}
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
	if(debug_mode & 0x1000)
	{
		func_800c99e4(e, 0xff);
	}
}

/* 0x800f72cc: the hit callback: any shot inside the sphere lands on group 0 */
int func_800f72cc(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		p->hit = 1;
		p->hit_id = obj->id;
		p->hit_group = 0;
		p->die = (void *)func_800f7420;
		p->hit_point[0] = p->aim[0];
		p->hit_point[1] = p->aim[1];
		p->hit_point[2] = p->aim[2];
	}
	return(0);
}

/* 0x800f735c: damage with a fade-out death (state 0x101, both groups alpha-blended from 255)
   and 500 points, else 50 points. Nothing in the game calls it: the hand dies at once
   (func_800f7420) */
void func_800f735c(int unused, player_stats_t *p, float damage, enemy_t *e)
{
	int	i;

	if(e->ai_state != 0x101)
	{
		e->health = e->health - damage;
		if(e->health <= 0.0f)
		{
			e->ai_state = 0x101;
			e->killer = p;
			e->alpha = 255.0f;
			*(float *)&e->unk44 = -13.0f;
			for(i = 0; i < 2; i++)
			{
				e->groups[i].flags |= 0x80;
				e->groups[i].alpha = e->alpha;
			}
			award_points(p->index, 500);
		}
		else
		{
			award_points(p->index, 50);
		}
	}
}

/* 0x800f7420: the die callback: the hit group goes, three bursts of blood at the hit point
   (10 below the hand for their floor), a hit mark, and the hand is gone */
int func_800f7420(game_object_t *obj, player_stats_t *p)
{
	enemy_t	*e;
	group_t	*g;

	e = (enemy_t *)obj->data;
	if(debug_mode & 0x10000)
	{
		debug_enemy = e;
		return(0);
	}
	g = &e->groups[p->hit_group];
	g->flags |= 0xf0000000;
	burst_blood(p->hit_point, e->y - 10.0f, 0xff0000);
	burst_blood(p->hit_point, e->y - 10.0f, 0xff0000);
	burst_blood(p->hit_point, e->y - 10.0f, 0xff0000);
	add_world_sprite(p->hit_point, 0, 0);
	remove_die(obj->id, p);
	return(0);
}

/* 0x800f7534, 0x800f7564, 0x800f75f8: the texmaps; the model and animation; their removal */
void load_hand_texmap(void)
{
	load_texmap(hand_texmap, 4, "hand");
}

void load_hand(void)
{
	load_model((void **)&hand_verts, 0x166, (void **)&hand_faces, 0x240, (void **)&hand_normals, 0x160, "HAND.ZM", 0x481b8a4f);
	load_anim(submesh_info_hand, 2, 0xab, "hand.za", 0x63f83cdd);
	hand_loaded = 1;
}

void func_800f75f8(void)
{
	hand_loaded = 0;
	remove_surface(hand_texmap, 4);
	remove_animation(submesh_info_hand, 2, 0xab, "hand.za");
	remove_model((void **)&hand_verts, 0x166, (void **)&hand_faces, 0x240, (void **)&hand_normals, 0x160, "HAND.ZM");
}

void	func_800f77d4(game_object_t *);

/* 0x800f7680: the init callback */
int func_800f7680(game_object_t *obj)
{
	enemy_t	*e;
	short	*p;
	int	i;

	p = malloc(sizeof(enemy_t));
	obj->data = p;
	if(!p)
	{
		return(1);
	}
	obj->hit = func_800f72cc;
	obj->die = func_800f7420;
	obj->remove = func_800f77d4;
	e = (enemy_t *)p;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0x1c;
	e->ai_state = 0;
	e->target = get_target_player();
	e->rx = 0;
	e->ry = 0;
	e->rz = 0;
	e->z = 0;
	e->x = -7.0f;
	e->y = 2.0f;
	e->unkcc = 0.4f;
	set_enemy_scale(e, 1.0f);
	e->vel[0] = 0;
	e->vel[1] = 0;
	e->vel[2] = 0;
	set_enemy_health(e, 0.75f);
	e->groups = malloc(2 * sizeof(group_t));
	if(!e->groups)
	{
		free(e);
		return(1);
	}
	for(i = 0; i < 2; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	func_8011464c(e, submesh_info_hand, 2);
	e->hit_group = &e->groups[0];
	e->hit_group->floor = 0.0f;
	clear_enemy_fields(e);
	return(0);
}

/* 0x800f77d4: the remove callback */
void func_800f77d4(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(debug_enemy == e)
	{
		debug_enemy = 0;
	}
	free(e->groups);
	free(e);
}

/* 0x800f7824: the think callback: the animation loops frames 59..170; spawn entries 0x7f-0x87
   each hurt the player (10) when the movie shows their frame */
void func_800f7824(game_object_t *obj)
{
	enemy_t	*e;

	e = (enemy_t *)obj->data;
	if(!(debug_mode & 0x10000))
	{
		if(++e->frame >= 0xab)
		{
			e->frame -= 0x3c;
		}
		switch(e->num)
		{
		case 0x7f:
			if(movie.disp->frame == 0x51e)
			{
				hurt_player(e->target, 10, 1, 0.0f);
			}
			break;
		case 0x80:
			if(movie.disp->frame == 0x52e)
			{
				hurt_player(e->target, 10, 1, 0.0f);
			}
			break;
		case 0x81:
			if(movie.disp->frame == 0x53d)
			{
				hurt_player(e->target, 10, 1, 0.0f);
			}
			break;
		case 0x82:
			if(movie.disp->frame == 0x527)
			{
				hurt_player(e->target, 10, 1, 0.0f);
			}
			break;
		case 0x83:
			if(movie.disp->frame == 0x536)
			{
				hurt_player(e->target, 10, 1, 0.0f);
			}
			break;
		case 0x84:
			if(movie.disp->frame == 0x556)
			{
				hurt_player(e->target, 10, 1, 0.0f);
			}
			break;
		case 0x85:
			if(movie.disp->frame == 0x54c)
			{
				hurt_player(e->target, 10, 1, 0.0f);
			}
			break;
		case 0x87:
			if(movie.disp->frame == 0x575)
			{
				hurt_player(e->target, 10, 1, 0.0f);
			}
			break;
		}
	}
}

/* 0x800f7960: the state set-up: state 0x3c starts the hand at frame 5 */
void func_800f7960(enemy_t *e, int state)
{
	e->ai_state = state;
	if(state == 0x3c)
	{
		e->frame = 5;
	}
	else
	{
		e->frame = 0;
	}
}
