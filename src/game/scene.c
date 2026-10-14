/*
 * scene.c - the game's 3D scene helpers and debug displays: one original file at
 * 0x800c8280-0x800ca4d0 (screen markers, the switch sampler, the debug nudges, the game's
 * object set-up). Decompiled as the draw pass needs them; the rest is still assembly.
 */
#include <stdio.h>
#include <goose/object.h>
#include "objects.h"
#include "enemy.h"
#include "credits.h"			/* debug_mode */
#include "movie.h"
#include "waves.h"

extern int	read_player12_direct(void), read_player34_direct(void);
extern int	get_free_mem_left(void);				/* 0x80143e90 */
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string(int, char *, float, float);
extern float	get_screen_pos(float *, float *);		/* 0x800d07a8: -1 when off screen */
extern void	func_800c8a44(float, float, float, float, unsigned int);	/* a screen marker */
extern void	rot_matrix_zyx(float *, float *);			/* a rotation matrix from three angles */
extern void	func_800c836c(void), func_800c865c(void), init_surfaces(void), init_lights(void);
extern void	init_camera(void), rot_matrix_yxz(float *, float *), vec_negate(float *, float *);
extern float	camera_x, camera_rx, view_matrix[9], camera_neg_pos[3];	/* camera.c */
extern void	init_player(int player);				/* 0x800e2f08 (player.c) */
extern int	read_player12_direct(void);
extern int	loaded_wave;					/* 0x80212e30 */
int		D_80212e60, D_80212e28, D_80212e24, D_80212e2c, D_802372b8;
int		D_801e0960;					/* the player 1/2 switches at boot, inverted */
extern void	load_font_texmap(void);
extern void	func_800db148(void), func_800db800(void), load_misc_texmaps(void), func_800ff0b0(void), func_800fe440(void);
extern void	func_800fedd8(void), func_800ffb08(void), func_80100478(void), func_8013f6d8(void);
extern void	draw_pass(void), think_pass(void *);
extern int	free_mem_left;					/* 0x801e0ee8 */
extern char	text_buffer[];
float		proj_x;					/* 0x801e5db0: world-to-screen size factor of the markers */

volatile int	switches12_prev, switches34_prev;		/* 0x801e5d94, 0x801e6e94: last frame's words */
volatile int	switches12, switches34;				/* 0x8022b170, 0x80237450: players 1-2 and 3-4 switch words, active high */
enemy_t		*debug_enemy = 0;				/* 0x80179bfc: the enemy the debug overlay follows */
float		debug_p1x = 10.0f, debug_p1y = 0.1f, debug_p1z = 10.0f;	/* 0x80179c00: two debug points (separate variables: */
float		debug_p2x = 0.1f, debug_p2y = 0.0f, debug_p2z = 0.0f;		/* 0x80179c0c  their stores are not through one address) */
float		debug_angles[3];				/* 0x80179c44 */
float		*D_80194650;					/* a positioned thing the debug overlay can nudge */
short		nearest_enemy;					/* 0x80212d92 */

/* 0x800c8ef0: a marker on the screen position of a world point (size in world units) */
void draw_marker(float *pt, float size, unsigned int color)
{
	float	scr[3];

	if(get_screen_pos(pt, scr) == -1.0f)
	{
		return;
	}
	func_800c8a44(scr[0], scr[1], scr[2], size * scr[2] * proj_x, color);
}

/* 0x800c9908: sample the switches once a frame; a press of player 3/4's bit 5 toggles the
   debug displays (bit 1 of the debug flags selects which) */
void sample_switches(void)
{
	switches12_prev = switches12;
	switches34_prev = switches34;
	switches12 = ~read_player12_direct();
	switches34 = ~read_player34_direct();
	if((switches34 & 0x20) && !(switches34_prev & 0x20))
	{
		if(debug_mode & DEBUG_ON)
		{
			if(debug_mode & DEBUG_POINT)
			{
				debug_mode &= ~DEBUG_POINT;
			}
			else
			{
				debug_mode |= DEBUG_POINT;
			}
		}
		else
		{
			if(debug_mode & DEBUG_CHARACTERS)
			{
				debug_mode &= ~DEBUG_CHARACTERS;
			}
			else
			{
				debug_mode |= DEBUG_CHARACTERS;
			}
		}
	}
}

/* 0x800c9d54: nudge the debugged enemy with the player 3/4 switches (bit 4 = big steps) */
void debug_move_enemy(enemy_t *e)
{
	float	step;

	step = 0.1f;
	if(switches34 & 0x10)
	{
		step = 2.0f;
	}
	if(switches34 & 0x2000)
	{
		e->path--;
	}
	if(switches34 & 0x4000)
	{
		e->path++;
	}
	if(switches34 & 0x100)
	{
		e->z -= step;
	}
	else if(switches34 & 0x200)
	{
		e->z += step;
	}
	if(switches34 & 0x1000)
	{
		e->y -= step;
	}
	else if(switches34 & 0x8000)
	{
		e->y += step;
	}
	if(switches34 & 0x400)
	{
		e->x -= step;
	}
	else if(switches34 & 0x800)
	{
		e->x += step;
	}
	if(switches34 & 4)
	{
		e->ry -= 0.1f;
	}
	else if(switches34 & 8)
	{
		e->ry += 0.1f;
	}
}

/* 0x800c9ed0 */
void print_heap_and_obj_info_onscreen(void)
{
	int	left;

	left = get_free_mem_left();
	graphical_printf(text_buffer, "HEAP %dK %dK %d OBJECTS", left / 1024, free_mem_left / 1024, num_objects_on_heap);
	draw_string(0, text_buffer, 10.0f, 210.0f);
}

/* 0x800c9f4c: nudge the two debug vectors with the player 3/4 switches */
void debug_move_vectors(void)
{
	if(switches34 & 1)
	{
		debug_p1x += 0.1f;
	}
	if(switches34 & 2)
	{
		debug_p1x -= 0.1f;
	}
	if(switches34 & 4)
	{
		debug_p1y += 0.1f;
	}
	if(switches34 & 8)
	{
		debug_p1y -= 0.1f;
	}
	if(switches34 & 0x10)
	{
		debug_p1z += 0.1f;
	}
	if(switches34 & 0x80)
	{
		debug_p1z -= 0.1f;
	}
	if(switches34 & 0x100)
	{
		debug_p2x += 0.1f;
	}
	if(switches34 & 0x200)
	{
		debug_p2x -= 0.1f;
	}
	if(switches34 & 0x400)
	{
		debug_p2y += 0.1f;
	}
	if(switches34 & 0x800)
	{
		debug_p2y -= 0.1f;
	}
	if(switches34 & 0x1000)
	{
		debug_p2z += 0.1f;
	}
	if(switches34 & 0x8000)
	{
		debug_p2z -= 0.1f;
	}
}

/* 0x800ca194: nudge a positioned thing (position, then its rotation matrix at +12 rebuilt
   from the debug angles) with the player 3/4 switches */
void debug_move_point(float *p)
{
	float	step = 0.01f;

	if(switches34 & 0x100)
	{
		p[1] -= step;
	}
	else if(switches34 & 0x200)
	{
		p[1] += step;
	}
	if(switches34 & 0x4000)
	{
		p[2] += step;
	}
	else if(switches34 & 0x2000)
	{
		p[2] -= step;
	}
	if(switches34 & 0x400)
	{
		p[0] -= step;
	}
	else if(switches34 & 0x800)
	{
		p[0] += step;
	}
	if(switches34 & 4)
	{
		debug_angles[1] -= 0.01f;
	}
	else if(switches34 & 8)
	{
		debug_angles[1] += 0.01f;
	}
	if(switches34 & 1)
	{
		debug_angles[0] -= 0.01f;
	}
	else if(switches34 & 2)
	{
		debug_angles[0] += 0.01f;
	}
	if(switches34 & 0x10)
	{
		debug_angles[2] += 0.01f;
	}
	else if(switches34 & 0x80)
	{
		debug_angles[2] -= 0.01f;
	}
	rot_matrix_zyx(p + 3, debug_angles);
}

/* 0x800c81b8: the game's state at boot: attract (0x800), no wave, no debug, the camera at
   the origin, both players reset */
void init_game_state(void)
{
	int	i;

	game_state[0] = 0x800;
	loaded_wave = 0;
	D_80212e60 = 0;
	debug_mode = 0;
	D_80212e28 = 0;
	D_80212e24 = 0;
	D_80212e2c = 0;
	init_camera();
	rot_matrix_yxz(view_matrix, &camera_rx);
	vec_negate(&camera_x, camera_neg_pos);
	D_802372b8 = 0;
	for(i = 0; i < 2; i++)
	{
		init_player(i);
	}
	D_801e0960 = ~read_player12_direct();
}

/* 0x800ca3a0: the game's set-up at boot: the movie player, the object list, the fonts, and
   the two GOOSE objects that run the draw and think passes every frame */
void game_init(void)
{
	func_800c836c();
	movie_init();
	init_game_state();
	func_800c865c();
	init_objects();
	init_surfaces();
	init_lights();
	load_font_texmap();
	create_object("FOO", 0, 0, 1, 0, (void (*)(void *))draw_pass);	/* the draw pass takes no argument */
	create_object("BAR", 0, 0, 0, 0, think_pass);
}

/* 0x800ca448: the level assets loaded once at boot */
void load_level_assets(void)
{
	func_800db148();
	func_800db800();
	load_misc_texmaps();
	func_800ff0b0();
	func_800fe440();
	func_800fedd8();
	func_800ffb08();
	func_80100478();
	func_8013f6d8();
}

/* 0x800ca4a8 */
void record_free_mem(void)
{
	free_mem_left = get_free_mem_left();
}

/* 0x800c9158: a movie depth word (the high half: a 4-bit exponent and a 12-bit mantissa) as
   the distance it encodes */
float depth_word_to_float(unsigned int *p)
{
	unsigned int w;
	unsigned int e;

	w = *p & 0xffff0000;
	e = (w >> 28) + 0x7f;
	w = (((w >> 16) & 0xfff) << 11) | (e << 23);
	return(*(float *)&w);
}
