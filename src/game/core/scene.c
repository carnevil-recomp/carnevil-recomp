/*
 * scene.c - the game's 3D scene helpers and debug displays: one original file at
 * 0x800c8280-0x800ca4d0 (screen markers, the switch sampler, the debug nudges, the game's
 * object set-up). Decompiled as the draw pass needs them; the rest is still assembly.
 */
#include <stdio.h>
#include <glide/glide.h>
#include <goose/object.h>
#include "objects.h"
#include "enemy.h"
#include "model.h"			/* group_t (the radar reads the hit group) */
#include "credits.h"			/* debug_mode */
#include "movie.h"
#include "waves.h"
#include "lights.h"			/* light_root */

extern int	read_player12_direct(void), read_player34_direct(void);
extern int	get_free_mem_left(void);				/* 0x80143e90 */
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string(int, char *, float, float);
extern float	get_screen_pos(float *, float *);		/* 0x800d07a8: -1 when off screen */
extern void	func_800c8a44(float, float, float, float, unsigned int);	/* a screen marker */
extern void	rot_matrix_zyx(float *, float *);			/* a rotation matrix from three angles */
extern void	init_surfaces(void), init_lights(void);
extern float	D_801faca4, D_801facb4;			/* the debug aim point (objects.c) */
int		D_801fad20;				/* 0x801fad20 */
float		D_801e0984;				/* 0x801e0984 */
extern void	init_camera(void), rot_matrix_yxz(float *, float *), vec_negate(float *, float *);
extern float	camera_x, camera_z, camera_rx, view_matrix[9], camera_neg_pos[3];	/* camera.c */
extern void	init_player(int player);				/* 0x800e2f08 (player.c) */
extern int	read_player12_direct(void);
extern int	loaded_wave;					/* 0x80212e30 */
int		D_80212e28, D_80212e24, D_80212e2c, D_802372b8;
extern int	wave_busy;			/* wave.c, 0x80212e60 */
int		D_801e0960;					/* the player 1/2 switches at boot, inverted */
extern void	load_font_texmap(void);
extern void	load_misc_texmaps(void), init_smoke(void), init_blood_drops(void);
#include "sprites.h"
extern void	init_sparks(void), init_snow(void), init_flecks(void), stream_reset(void);
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
short		nearest_enemy;					/* 0x80212d92 */

/* a vertex of the constant-colour triangles with an alpha and a depth (func_800d2ff0) */
typedef struct qavert
{
	float	x, y;
	float	q;			/* 1/w */
	float	a;			/* 0..255 */
} qavert_t;
extern void	func_800d2ff0(qavert_t *, qavert_t *, qavert_t *);	/* the triangle set-up for qavert_t */
extern float	fsin(float), fcos(float);		/* GOOSE sincos.c */
extern void	world_to_view(float *, float *);	/* vecmat.c */
extern void	draw_string_color(int, char *, float, float, unsigned int);
extern void	func_800d14a4(float, float);		/* vecmat.c: a dot on the debug map */
void		func_800c918c(void);
float		marker_rim_alpha = 10.0f;		/* 0x80179c34: the markers' rim alpha, pulsing between 1 and 20 */
float		marker_rim_step = 0.1f;			/* 0x80179c38: its change per marker drawn */
float		ring_centre_alpha = 10.0f;		/* 0x80179c3c: the rings' centre alpha (plus 10), pulsing between 1 and 20 */
float		ring_centre_step = 0.1f;		/* 0x80179c40: its change per ring drawn */

/* 0x800c8774: a flat disc of radius r round (x, y) in the colour, every corner at alpha a and
   depth 0.909: 24 triangles from the centre, each corner snapped to a quarter pixel */
void func_800c8774(float x, float y, float r, unsigned int color, float a)
{
	qavert_t	v[3];
	float		angle, next;

	guAlphaSource(GR_ALPHASOURCE_ITERATED_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA);
	guColorCombineFunction(GR_COLORCOMBINE_CCRGB);
	grConstantColorValue(color);
	angle = 0.0f;
	v[0].x = x;
	v[0].y = y;
	v[0].q = v[1].q = v[2].q = 0.90909088f;
	v[0].a = v[1].a = v[2].a = a;
	do
	{
		v[1].x = v[0].x + r * fsin(angle);
		v[1].y = v[0].y + r * fcos(angle);
		next = angle + 0.26179934f;
		v[2].x = v[0].x + r * fsin(next);
		v[2].y = v[0].y + r * fcos(next);
		v[0].x = (v[0].x + 2097152.0f) - 2097152.0f;
		v[1].x = (v[1].x + 2097152.0f) - 2097152.0f;
		v[1].y = (v[1].y + 2097152.0f) - 2097152.0f;
		v[2].x = (v[2].x + 2097152.0f) - 2097152.0f;
		v[0].y = (v[0].y + 2097152.0f) - 2097152.0f;
		v[2].y = (v[2].y + 2097152.0f) - 2097152.0f;
		func_800d2ff0(&v[0], &v[1], &v[2]);
		angle = next;
	} while(angle < 6.1086516f);
	grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
}

/* 0x800c8938: the debug sphere (debug flag 0x100): a disc of the world radius at a world point,
   at alpha 128, when the point is in front of the camera */
void func_800c8938(float *pos, float radius, unsigned int color)
{
	float	scr[3];

	if(get_screen_pos(pos, scr) != -1.0f)
	{
		func_800c8774(scr[0], scr[1], radius * scr[2] * proj_x, color, 128.0f);
	}
}

/* 0x800c8a44: a disc of radius r round (x, y) at depth q in the colour: twelve triangles from
   the centre (alpha 255) to the rim (the pulsing alpha), every corner snapped to a quarter
   pixel; the rim alpha moves by its step each call and turns back past 20 and below 1 */
void func_800c8a44(float x, float y, float q, float r, unsigned int color)
{
	qavert_t	v[3];
	float		angle, next;

	guAlphaSource(GR_ALPHASOURCE_ITERATED_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA);
	guColorCombineFunction(GR_COLORCOMBINE_CCRGB);
	grConstantColorValue(color);
	v[2].q = q;
	v[1].q = q;
	v[0].q = q;
	v[0].a = 255.0f;
	v[1].a = v[2].a = marker_rim_alpha + 0.0f;
	marker_rim_alpha += marker_rim_step;
	if(marker_rim_alpha > 20.0f)
	{
		marker_rim_alpha -= marker_rim_step;
		marker_rim_step = -marker_rim_step;
	}
	if(marker_rim_alpha < 1.0f)
	{
		marker_rim_alpha -= marker_rim_step;
		marker_rim_step = -marker_rim_step;
	}
	angle = 0.0f;
	v[0].x = x;
	v[0].y = y;
	do
	{
		v[1].x = v[0].x + r * fsin(angle);
		v[1].y = v[0].y + r * fcos(angle);
		next = angle + 0.5235987f;
		v[2].x = v[0].x + r * fsin(next);
		v[2].y = v[0].y + r * fcos(next);
		v[0].x = (v[0].x + 2097152.0f) - 2097152.0f;
		v[0].y = (v[0].y + 2097152.0f) - 2097152.0f;
		v[1].x = (v[1].x + 2097152.0f) - 2097152.0f;
		v[1].y = (v[1].y + 2097152.0f) - 2097152.0f;
		v[2].x = (v[2].x + 2097152.0f) - 2097152.0f;
		v[2].y = (v[2].y + 2097152.0f) - 2097152.0f;
		func_800d2ff0(&v[0], &v[1], &v[2]);
		angle = next;
	} while(angle < 6.1086516f);
	grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
}

/* 0x800c8c98: a ring: the same twelve triangles as func_800c8a44 but the rim is solid (alpha
   125) and the centre faint (its own pulsing alpha plus 10); the gun's crosshair on the
   high-score initials board */
void draw_ring(float x, float y, float q, float r, unsigned int color)
{
	qavert_t	v[3];
	float		angle, next;

	guAlphaSource(GR_ALPHASOURCE_ITERATED_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA);
	guColorCombineFunction(GR_COLORCOMBINE_CCRGB);
	grConstantColorValue(color);
	v[2].q = q;
	v[1].q = q;
	v[0].q = q;
	v[0].a = ring_centre_alpha + 10.0f;
	v[1].a = v[2].a = 125.0f;
	ring_centre_alpha += ring_centre_step;
	if(ring_centre_alpha > 20.0f)
	{
		ring_centre_alpha -= ring_centre_step;
		ring_centre_step = -ring_centre_step;
	}
	if(ring_centre_alpha < 1.0f)
	{
		ring_centre_alpha -= ring_centre_step;
		ring_centre_step = -ring_centre_step;
	}
	angle = 0.0f;
	v[0].x = x;
	v[0].y = y;
	do
	{
		v[1].x = v[0].x + r * fsin(angle);
		v[1].y = v[0].y + r * fcos(angle);
		next = angle + 0.5235987f;
		v[2].x = v[0].x + r * fsin(next);
		v[2].y = v[0].y + r * fcos(next);
		v[0].x = (v[0].x + 2097152.0f) - 2097152.0f;
		v[0].y = (v[0].y + 2097152.0f) - 2097152.0f;
		v[1].x = (v[1].x + 2097152.0f) - 2097152.0f;
		v[1].y = (v[1].y + 2097152.0f) - 2097152.0f;
		v[2].x = (v[2].x + 2097152.0f) - 2097152.0f;
		v[2].y = (v[2].y + 2097152.0f) - 2097152.0f;
		func_800d2ff0(&v[0], &v[1], &v[2]);
		angle = next;
	} while(angle < 6.1086516f);
	grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
}

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

/* 0x800c918c: with debug flag 0x40, a marker in its colour on every point light that is on
   (three units across) and, with flag 0x20 as well, its dot on the debug map; then the number
   of directional lights that are on, in green */
void func_800c918c(void)
{
	level_light_t	*l;
	int		n;
	float		scr[3], size;
	unsigned int	color;

	l = light_root.next;
	n = 0;
	if(!(debug_mode & 0x40))
	{
		return;
	}
	for(; l; l = l->next)
	{
		if(!(l->flags & 1))
		{
			continue;
		}
		if(l->type == 4)
		{
			color = (((int)l->color[0] & 0xff) << 16) | (((int)l->color[1] & 0xff) << 8) | ((int)l->color[2] & 0xff);
			size = 3.0f;
			if(get_screen_pos(l->pos, scr) != -1.0f)
			{
				func_800c8a44(scr[0], scr[1], scr[2], scr[2] * size * proj_x, color);
			}
			if(debug_mode & 0x20)
			{
				guAlphaSource(GR_ALPHASOURCE_ITERATED_ALPHA);
				guColorCombineFunction(GR_COLORCOMBINE_CCRGB);
				grConstantColorValue((((int)l->color[0] & 0xff) << 16) | (((int)l->color[1] & 0xff) << 8) | ((int)l->color[2] & 0xff));
				grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA);
				func_800d14a4(l->pos[0] * 0.12f + 200.0f, l->pos[2] * 0.12f + 128.0f);
				grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
			}
		}
		else if(l->type == 2)
		{
			n++;
		}
	}
	if(n > 0)
	{
		graphical_printf(text_buffer, "%d PARALLEL LIGHTS", n);
		draw_string_color(0, text_buffer, 20.0f, 35.0f, 0xff00);
	}
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

/* 0x800c99e4: the debug radar (flag 0x1000): the enemy as a disc of its radius at its place
   relative to the camera (x across, z up, the camera at the screen's centre) and its number,
   black when gone, red dying, yellow hit, magenta 0x800, green attacking, white otherwise */
void func_800c99e4(enemy_t *e, unsigned int color)
{
	float	x, y;

	x = -(camera_x - e->hit_group->pos[0]) + 200.0f;
	y = -(camera_z - e->hit_group->pos[2]) + 128.0f;
	if(x > 0.0f && x < 400.0f && y > 0.0f && y < 256.0f)
	{
		func_800c8774(x, y, e->radius, color, 128.0f);
		graphical_printf(text_buffer, "%d", e->num);
		if(e->ai_state & 0x4000)
		{
			draw_string_color(0, text_buffer, x - 4.0f, y - 4.0f, 0);
		}
		else if(e->ai_state & 0x2000)
		{
			draw_string_color(0, text_buffer, x - 4.0f, y - 4.0f, 0xff0000);
		}
		else if(e->ai_state & 0x1000)
		{
			draw_string_color(0, text_buffer, x - 4.0f, y - 4.0f, 0xffff00);
		}
		else if(e->ai_state & 0x800)
		{
			draw_string_color(0, text_buffer, x - 4.0f, y - 4.0f, 0xff00ff);
		}
		else if(e->ai_state & 0x200)
		{
			draw_string_color(0, text_buffer, x - 4.0f, y - 4.0f, 0xff00);
		}
		else
		{
			draw_string_color(0, text_buffer, x - 4.0f, y - 4.0f, 0xffffff);
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
	wave_busy = 0;
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

/* 0x800c836c: two globals of unknown use and, in debug mode 2, the debug aim point */
void func_800c836c(void)
{
	D_801fad20 = 5;
	D_801e0984 = 3.2f;
	if(debug_mode & 2)
	{
		D_801faca4 = 100.0f;
		D_801facb4 = 100.0f;
	}
}

short		D_80179bf8 = -1;			/* 0x80179bf8: the object game_proc2 adds (type 0x2068) */
float		debug_marker[3];			/* 0x801e6ed8: a point the debug overlay marks and moves */

/* 0x800c83bc: the debug overlays the draw pass shows last: with debug flag 0x4000 a marker on
   debug_marker, its coordinates, and player 1/2 switches 8-14 moving it by half a unit (z with
   bits 8/9, x with 10/11, y with 12/14); with flag 0x80 the world and view position of the
   object game_proc2 added; then the light display */
void func_800c83bc(void)
{
	enemy_t	*o;
	float	v[3];

	if(debug_mode & 0x4000)
	{
		draw_marker(debug_marker, 2.0f, 0xffffffff);
		graphical_printf(text_buffer, "X %.4f Y %.4f Z %.4f", debug_marker[0], debug_marker[1], debug_marker[2]);
		draw_string(0, text_buffer, 40.0f, 166.0f);
		if(switches12 & 0x100)
		{
			debug_marker[2] += 0.5f;
		}
		if(switches12 & 0x200)
		{
			debug_marker[2] -= 0.5f;
		}
		if(switches12 & 0x400)
		{
			debug_marker[0] -= 0.5f;
		}
		if(switches12 & 0x800)
		{
			debug_marker[0] += 0.5f;
		}
		if(switches12 & 0x1000)
		{
			debug_marker[1] -= 0.5f;
		}
		if(switches12 & 0x4000)
		{
			debug_marker[1] += 0.5f;
		}
	}
	if(debug_mode & 0x80)
	{
		o = get_object_data(D_80179bf8);
		if(o)
		{
			graphical_printf(text_buffer, "X %.4f Y %.4f Z %.4f", o->x, o->y, o->z);
			draw_string(0, text_buffer, 40.0f, 166.0f);
			world_to_view(&o->x, v);
			graphical_printf(text_buffer, "RT X %.4f Y %.4f Z %.4f", v[0], v[1], v[2]);
			draw_string(0, text_buffer, 12.0f, 146.0f);
		}
	}
	func_800c918c();
}

/* 0x800c865c: empty */
void func_800c865c(void)
{
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
	init_screen_sprites();
	init_world_sprites();
	load_misc_texmaps();
	init_smoke();
	init_blood_drops();
	init_sparks();
	init_snow();
	init_flecks();
	stream_reset();
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
