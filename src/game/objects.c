/*
 * objects.c - CarnEvil's game object list: a fixed array of 128 nodes threaded into a list
 * ordered by type, with the head at index 0. Functions at 0x800cc398-0x800cc9c4.
 */
#include <stdio.h>
#include <glide/glide.h>

#include "objects.h"
#include "enemy.h"
#include "credits.h"			/* game_phase, player_stats */
#include "levelsnd.h"
#include "movie.h"
#include "spawn.h"
#include "waves.h"

extern int	get_free_mem_left(void);		/* 0x80143e90 */
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string(int, char *, float, float);
extern void	draw_string_color(int, char *, float, float, unsigned int);
extern void	draw_string_centered(int, char *, float, float, unsigned int);
extern void	attract_credit_lines(void);
extern void	show_volume_level(void);
extern float	get_screen_pos(float *, float *);	/* 0x800d07a8 */
extern void	sample_switches(void), debug_move_enemy(enemy_t *), debug_move_point(float *), debug_move_vectors(void);	/* scene.c */
extern void	draw_marker(float *, float, unsigned int);
extern void	update_camera(int), show_debug_light_(int);	/* camera.c, and the lights */
extern void	func_800d6e94(void), func_800ff418(void);
extern player_stats_t *something_to_do_with_shooting_(player_stats_t *);	/* 0x800d5a84 */
extern void	func_800ce2f8(float *, float, float *);
extern void	func_800fe580(void), func_800fef18(void), func_800ffbdc(void), func_800fe1fc(void);
extern void	func_800dbb80(void), func_80100558(void), func_800e2fe4(void), func_800db4b8(void), func_800c83bc(void);
extern void	func_800c9620(int), func_800c9bf0(void), print_heap_and_obj_info_onscreen(void);
extern float	fsqrt(float);				/* 0x80144900 */
extern int	free_mem_left;				/* 0x801e0ee8 */
extern int	D_801e5cbc;				/* draw the level's 3D even without debug bit 0 */
extern int	shown_frame;				/* 0x80212da0: the movie frame on screen */
extern int	loaded_wave;				/* 0x80212e30 */
extern short	nearest_enemy;				/* 0x80212d92: id of the enemy nearest the camera this frame */
short	nearest_enemy_hhouse(void), nearest_enemy_freakshow(void), nearest_enemy_rickity(void), nearest_enemy_bigtop(void);
extern int	RTP_val, DMA_val, D_801e0ecc;		/* 0x80237508, 0x80237414: per-frame triangle/DMA counters */
extern volatile int got_interrupt_save;		/* 0x801e0ebc: vblanks since the last frame, counted by the interrupt handler */
extern int	please_wait_timer;			/* 0x801e7050 */
extern float	debug_p1x, debug_p1y, debug_p1z, debug_p2x, debug_p2y, debug_p2z, debug_angles[3];	/* scene.c */
extern char	text_buffer[];

extern enemy_t	*debug_enemy;				/* 0x80179bfc: the enemy being debugged (scene.c) */
extern float		*D_80194650;

game_object_t	objects[MAX_OBJECTS];
short		num_objects_on_heap;
short		cur_object_id;
short		next_free_object;
int		movie_draw_disabled;		/* 0x801e5bfc */
int		debug_blink = 0;		/* 0x80179d40: "DEBUGGING CHARACTERS" blink counter */

/* 0x800cbb70, 0x800cbb84 */
void disable_movie_draw(void)
{
	movie_draw_disabled = 1;
}
void enable_movie_draw(void)
{
	movie_draw_disabled = 0;
}

/* 0x800cbb94: screen position of an enemy's origin */
float object_screen_pos(enemy_t *obj, float *out)
{
	return(get_screen_pos(&obj->x, out));
}

/* 0x800cbbb4: the draw pass, run by the "FOO" GOOSE object every frame: the movie frame (or
   a clear), the level's 3D, every game object's draw function, then the overlays and the
   debug displays */
void draw_pass(void)
{
	int		frame;
	game_object_t	*p;
	short		i;
	float		pt[3];

	free_mem_left = get_free_mem_left();
	grConstantColorValue(0x0ff00000);
	if(!movie_draw_disabled)
	{
		frame = movie_draw_frame();
	}
	else
	{
		frame = 0;
	}
	shown_frame = frame;
	sample_switches();
	if(D_801e5cbc || (debug_mode & DEBUG_ON))
	{
		update_camera(frame);
		show_debug_light_(frame);
		spawn_wave_enemies(frame);
	}
	grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
	grDepthBufferFunction(GR_CMP_LEQUAL);
	grDepthBiasLevel(0);
	func_800d6e94();
	func_800ff418();
	i = objects[0].next;
	while(i != OBJ_NONE)
	{
		p = &objects[i];
		i = p->next;
		p->draw(p);		/* no check: every object has a draw function */
	}
	if(player_stats[0].shooting == 1)
	{
		something_to_do_with_shooting_(&player_stats[0]);
	}
	if(player_stats[1].shooting == 1)
	{
		something_to_do_with_shooting_(&player_stats[1]);
	}
	if(debug_mode & DEBUG_CHARACTERS)
	{
		if(debug_enemy)
		{
			debug_move_enemy(debug_enemy);
			graphical_printf(text_buffer, "%d XYZ %f %f %f", debug_enemy->num, debug_enemy->x, debug_enemy->y, debug_enemy->z);
			draw_string_color(0, text_buffer, 0.0f, 186.0f, 0xff00);
			graphical_printf(text_buffer, "ROT %f %f %f", debug_enemy->rx, debug_enemy->ry, debug_enemy->rz);
			draw_string_color(0, text_buffer, 0.0f, 166.0f, 0xff00);
			graphical_printf(text_buffer, "AI 0X%04X FRAME %d PATH %d", debug_enemy->ai_state, debug_enemy->frame, debug_enemy->path);
			draw_string_color(0, text_buffer, 0.0f, 146.0f, 0xff00);
		}
		func_800ce2f8(camera_pos, -30.0f, pt);
		draw_marker(pt, 1.0f, 0xffff00ff);
		graphical_printf(text_buffer, "XYZ %f %f %f", pt[0], pt[1], pt[2]);
		draw_string_color(0, text_buffer, 40.0f, 20.0f, 0xffffff);
		draw_string_color(0, text_buffer, 40.0f, 0.0f, 0);
	}
	if((debug_mode & DEBUG_POINT) && D_80194650)
	{
		debug_move_point(D_80194650);
		graphical_printf(text_buffer, "XYZ %f %f %f", D_80194650[0], D_80194650[1], D_80194650[2]);
		draw_string_color(0, text_buffer, 0.0f, 80.0f, 0xff00);
		graphical_printf(text_buffer, "ROT %f %f %f", debug_angles[0], debug_angles[1], debug_angles[2]);
		draw_string_color(0, text_buffer, 0.0f, 60.0f, 0xff00);
	}
	if(debug_mode & DEBUG_VECTORS)
	{
		debug_move_vectors();
		graphical_printf(text_buffer, "XYZ %f %f %f", debug_p1x, debug_p1y, debug_p1z);
		draw_string_color(0, text_buffer, 0.0f, 30.0f, 0xff00);
		graphical_printf(text_buffer, "XYZ %f %f %f", debug_p2x, debug_p2y, debug_p2z);
		draw_string_color(0, text_buffer, 0.0f, 10.0f, 0xff00);
	}
	func_800fe580();
	func_800fef18();
	func_800ffbdc();
	func_800fe1fc();
	func_800dbb80();
	func_80100558();
	grDepthBufferFunction(GR_CMP_ALWAYS);
	show_volume_level();
	func_800e2fe4();
	func_800db4b8();
	func_800c83bc();
	if(game_phase == 0x80)
	{
		if(please_wait_timer > 0)
		{
			draw_string_centered(1, "PLEASE WAIT", 200.0f, 28.0f, (please_wait_timer * 0x10101) << 4);
			please_wait_timer--;
		}
	}
	if(game_phase == 1)
	{
		attract_credit_lines();
	}
	if(debug_mode & DEBUG_CHARACTERS)
	{
		debug_blink++;
		if(debug_blink & 2)
		{
			graphical_printf(text_buffer, "DEBUGGING CHARACTERS");
			draw_string_color(0, text_buffer, 10.0f, 240.0f, 0xff0000);
		}
	}
	if(debug_mode & DEBUG_RTP_FPS)
	{
		graphical_printf(text_buffer, "RTP %d DMA %d DRAWN %d", RTP_val, DMA_val, D_801e0ecc);
		draw_string(0, text_buffer, 10.0f, 235.0f);
		if(got_interrupt_save == 0)
		{
			got_interrupt_save = 1;
		}
		graphical_printf(text_buffer, "%d FPS", 60 / got_interrupt_save);
		if(got_interrupt_save < 3)
		{
			draw_string_color(0, text_buffer, 300.0f, 235.0f, 0xff00);
		}
		else if(got_interrupt_save >= 4)
		{
			draw_string_color(0, text_buffer, 300.0f, 235.0f, 0xff0000);
		}
		else
		{
			draw_string(0, text_buffer, 300.0f, 235.0f);
		}
		got_interrupt_save = 0;
	}
	if(debug_mode & 0x20)
	{
		func_800c9620(0);
	}
	if(debug_mode & 0x1000)
	{
		func_800c9bf0();
	}
	if(debug_mode & DEBUG_HEAP)
	{
		print_heap_and_obj_info_onscreen();
	}
}

void *get_object_data(int id)
{
	if((unsigned int)id > 0x80)
	{
		printf("Attempting to get_object_data with invalid id - %d\n", id);
		return(0);
	}
	if(objects[id].type == OBJ_FREE)
	{
		printf("Attempting to get_object_data with nonexistant id - %d\n", id);
		return(0);
	}
	return(objects[id].data);
}

void init_objects(void)
{
	int	i;

	movie_draw_disabled = 0;
	for(i = 0; i < MAX_OBJECTS; i++)
	{
		objects[i].type = OBJ_FREE;
		objects[i].next = OBJ_NONE;
		objects[i].prev = OBJ_NONE;
	}
	num_objects_on_heap = 1;
	next_free_object = 1;
	objects[0].type = OBJ_HEAD;
	objects[0].id = 0;
	objects[0].next = OBJ_NONE;
	objects[0].prev = OBJ_NONE;
}

inline short alloc_object_id(void)
{
	short	i;

	i = next_free_object;
	while(objects[i].type != OBJ_FREE)
	{
		i = (i + 1) & 0x7f;
	}
	next_free_object = (i + 1) & 0x7f;
	return(i);
}

short remove_game_object(short id);

short add_object(game_object_t *tmpl)
{
	game_object_t	*obj;
	game_object_t	*p;
	short		id;
	short		next;
	int		err;

	if(num_objects_on_heap >= MAX_OBJECTS)
	{
		return(0);
	}
	id = alloc_object_id();
	obj = &objects[id];
	*obj = *tmpl;
	obj->id = id;
	obj->unused18 = 0;
	obj->unused1c = 0;
	obj->remove = 0;
	obj->data = 0;

	/* insert in type order */
	p = &objects[0];
	if(objects[0].id != OBJ_NONE)
	{
		do
		{
			if(p->type != OBJ_HEAD && (unsigned int)p->type < (unsigned int)obj->type)
			{
				break;
			}
			next = p->next;
			if(p->next == OBJ_NONE)
			{
				break;
			}
			p = &objects[p->next];
		} while(next != OBJ_NONE);
	}
	if(p->next == OBJ_NONE)
	{
		obj->next = p->next;
		obj->prev = p->id;
		p->next = id;
	}
	else
	{
		obj->prev = p->prev;
		obj->next = p->id;
		objects[p->prev].next = id;
		p->prev = id;
	}

	err = 0;
	if(obj->init)
	{
		err = obj->init(obj);
	}
	if(obj->data && obj->type != 0x1000)
	{
		*obj->data = obj->id;
	}
	num_objects_on_heap++;
	if(err)
	{
		printf("Big time fuckup in add object TYPE %04X ERR %d\n", obj->type, err);
		remove_game_object(obj->id);
		return(0);
	}
	return(obj->id);
}

short add_object1_wrapper(short type, void (*draw)(game_object_t *), int (*init)(game_object_t *), void (*think)(game_object_t *))
{
	game_object_t	t;

	t.type = type;
	t.draw = draw;
	t.init = init;
	t.think = think;
	return(add_object(&t));
}

short remove_game_object(short id)
{
	game_object_t	*obj;

	obj = &objects[id];
	if(num_objects_on_heap < 2)
	{
		return(0);
	}
	if(id == 0)
	{
		printf("Trying to remove HEAD from object list\n");
		return(0);
	}
	if(obj->type == OBJ_FREE)
	{
		return(0);
	}
	if(obj->remove)
	{
		obj->remove(obj);
	}
	if(obj->id == cur_object_id)
	{
		cur_object_id = 0;
	}
	if(obj->next == OBJ_NONE)
	{
		objects[obj->prev].next = OBJ_NONE;
	}
	else
	{
		objects[obj->prev].next = obj->next;
		objects[obj->next].prev = obj->prev;
	}
	objects[id].type = OBJ_FREE;
	num_objects_on_heap--;
	return(1);
}

void remove_all_objects(void)
{
	game_object_t	*p;
	short		i;

	i = objects[0].next;
	while(i != OBJ_NONE)
	{
		p = &objects[i];
		i = p->next;
		if(!(p->type & 0x8000))
		{
			remove_game_object(p->id);
		}
	}
}

void remove_objects_of_type(short type)
{
	game_object_t	*p;
	short		i;

	i = objects[0].next;
	while(i != OBJ_NONE)
	{
		p = &objects[i];
		i = p->next;
		if(p->type == type)
		{
			remove_game_object(p->id);
		}
	}
}

/* 0x800cca68: an init callback that always succeeds */
int func_800cca68(void)
{
	return(1);
}

/* 0x800cca70: the think pass, run by the "BAR" GOOSE object every frame: the level's wave
   logic, the sound fades, then every game object's think function */
void think_pass(void *data)
{
	game_object_t	*p;
	short		i;

	RTP_val = 0;
	DMA_val = 0;
	D_801e0ecc = 0;
	switch(loaded_wave)
	{
	case WAVE_HHOUSE:
		nearest_enemy = nearest_enemy_hhouse();
		break;
	case WAVE_FREAKSHOW:
		nearest_enemy = nearest_enemy_freakshow();
		break;
	case WAVE_RICKITY:
		nearest_enemy = nearest_enemy_rickity();
		break;
	case WAVE_BIGTOP:
		nearest_enemy = nearest_enemy_bigtop();
		break;
	default:
		nearest_enemy = 0;
		break;
	}
	level_sound_fades();
	i = objects[0].next;
	while(i != OBJ_NONE)
	{
		p = &objects[i];
		i = p->next;
		if(p->think)
		{
			p->think(p);
		}
	}
}

/* 0x800cd1f8-0x800cdaa0: the id of the nearest live enemy of the level's kinds within
   1000 units of the camera (in x and z), 0 when none */
short nearest_enemy_hhouse(void)
{
	game_object_t	*p;
	enemy_t		*e;
	short		i, best;
	float		range, d;

	best = 0;
	range = 1000.0f;
	i = objects[0].next;
	while(i != OBJ_NONE)
	{
		p = &objects[i];
		i = p->next;
		if(p->type == 0x1102 || p->type == 0x200c)
		{
			e = (enemy_t *)p->data;
			d = fsqrt((e->x - camera_pos[0]) * (e->x - camera_pos[0]) + (e->z - camera_pos[2]) * (e->z - camera_pos[2]));
			if(d < range)
			{
				range = d;
				best = p->id;
			}
		}
	}
	return(best);
}

short nearest_enemy_freakshow(void)
{
	game_object_t	*p;
	enemy_t		*e;
	short		i, best;
	float		range, d;

	best = 0;
	range = 1000.0f;
	i = objects[0].next;
	while(i != OBJ_NONE)
	{
		p = &objects[i];
		i = p->next;
		if(p->type == 0x2029 || p->type == 0x2024 || p->type == 0x2020 || p->type == 0x2022 || p->type == 0x2026)
		{
			e = (enemy_t *)p->data;
			if(!(e->ai_state & 0x6000))
			{
				d = fsqrt((e->x - camera_pos[0]) * (e->x - camera_pos[0]) + (e->z - camera_pos[2]) * (e->z - camera_pos[2]));
				if(d < range)
				{
					range = d;
					best = p->id;
				}
			}
		}
	}
	return(best);
}

short nearest_enemy_rickity(void)
{
	game_object_t	*p;
	enemy_t		*e;
	short		i, best;
	float		range, d;

	best = 0;
	range = 1000.0f;
	i = objects[0].next;
	while(i != OBJ_NONE)
	{
		p = &objects[i];
		i = p->next;
		if(p->type == 0x2041 || p->type == 0x2044 || p->type == 0x2042 || p->type == 0x2045 || p->type == 0x2040)
		{
			e = (enemy_t *)p->data;
			if(!(e->ai_state & 0x6000))
			{
				d = fsqrt((e->x - camera_pos[0]) * (e->x - camera_pos[0]) + (e->z - camera_pos[2]) * (e->z - camera_pos[2]));
				if(d < range)
				{
					range = d;
					best = p->id;
				}
			}
		}
	}
	return(best);
}

short nearest_enemy_bigtop(void)
{
	game_object_t	*p;
	enemy_t		*e;
	short		i, best;
	float		range, d;

	best = 0;
	range = 1000.0f;
	i = objects[0].next;
	while(i != OBJ_NONE)
	{
		p = &objects[i];
		i = p->next;
		if(p->type == 0x2065 || p->type == 0x2061 || p->type == 0x2062 || p->type == 0x2063 || p->type == 0x2064 || p->type == 0x2060 || p->type == 0x2068)
		{
			e = (enemy_t *)p->data;
			if(!(e->ai_state & 0x6000))
			{
				d = fsqrt((e->x - camera_pos[0]) * (e->x - camera_pos[0]) + (e->z - camera_pos[2]) * (e->z - camera_pos[2]));
				if(d < range)
				{
					range = d;
					best = p->id;
				}
			}
		}
	}
	return(best);
}
