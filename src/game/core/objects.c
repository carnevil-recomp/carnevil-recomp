/*
 * objects.c - CarnEvil's game object list: a fixed array of 128 nodes threaded into a list
 * ordered by type, with the head at index 0. Functions at 0x800cc398-0x800cc9c4.
 */
#include <stdio.h>
#include <glide/glide.h>

#include "objects.h"
#include "enemy.h"
#include "model.h"			/* submesh_t, group_t, frame_t */
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
extern void	world_to_view(float *, float *), view_to_world(float *);	/* vecmat.c */
extern void	sample_switches(void), debug_move_enemy(enemy_t *), debug_move_point(float *), debug_move_vectors(void);	/* scene.c */
extern void	draw_marker(float *, float, unsigned int);
extern void	update_camera(int), update_lights(int);
extern void	*camera_data;				/* camera.c: the level's camera file, 0 when none */	/* camera.c, and the lights */
extern void	apply_fog(void), draw_smoke(void);
extern player_stats_t *resolve_shot(player_stats_t *);	/* 0x800d5a84 */
void	point_toward_camera(float *, float, float *);
extern void	draw_blood_drops(void), draw_sparks(void), draw_snow(void), draw_chunks(void);
extern void	draw_flecks(void), draw_hud(void), func_800c83bc(void);
#include "sprites.h"
extern void	func_800c9620(int), func_800c9bf0(void), print_heap_and_obj_info_onscreen(void);
extern float	fsqrt(float);				/* 0x80144900 */
extern float	project_point(float *, float *);		/* vecmat.c */
extern int	free_mem_left;				/* 0x801e0ee8 */
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
#include "weapons.h"		/* last_held_object */

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
	if(camera_data || (debug_mode & DEBUG_ON))
	{
		update_camera(frame);
		update_lights(frame);
		spawn_wave_enemies(frame);
	}
	grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
	grDepthBufferFunction(GR_CMP_LEQUAL);
	grDepthBiasLevel(0);
	apply_fog();
	draw_smoke();
	i = objects[0].next;
	while(i != OBJ_NONE)
	{
		p = &objects[i];
		i = p->next;
		p->draw(p);		/* no check: every object has a draw function */
	}
	if(player_stats[0].shooting == 1)
	{
		resolve_shot(&player_stats[0]);
	}
	if(player_stats[1].shooting == 1)
	{
		resolve_shot(&player_stats[1]);
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
		point_toward_camera(camera_pos, -30.0f, pt);
		draw_marker(pt, 1.0f, 0xffff00ff);
		graphical_printf(text_buffer, "XYZ %f %f %f", pt[0], pt[1], pt[2]);
		draw_string_color(0, text_buffer, 40.0f, 20.0f, 0xffffff);
		draw_string_color(0, text_buffer, 40.0f, 0.0f, 0);
	}
	if((debug_mode & DEBUG_POINT) && last_held_object)
	{
		debug_move_point(last_held_object->off);
		graphical_printf(text_buffer, "XYZ %f %f %f", last_held_object->off[0], last_held_object->off[1], last_held_object->off[2]);
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
	draw_blood_drops();
	draw_sparks();
	draw_snow();
	draw_chunks();
	draw_world_sprites();
	draw_flecks();
	grDepthBufferFunction(GR_CMP_ALWAYS);
	show_volume_level();
	draw_hud();
	draw_screen_sprites();
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
	obj->hit = 0;
	obj->die = 0;
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

/* 0x800cd610: Rickety Town's step test for move_cd: the step to np from pos is blocked when
   it ends inside another live enemy (the five Rickety Town types, not dying) that it was not
   already inside; the enemy nearest the camera is never blocked */
int step_blocked_rickity(float *np, float *pos, int id)
{
	game_object_t	*o;
	enemy_t		*e;
	short		i;
	float		d, dx, dz;

	if(nearest_enemy == (short)id)
	{
		return(0);
	}
	i = objects[0].next;
	while(i != OBJ_NONE)
	{
		o = &objects[i];
		i = o->next;
		if(o->id == (short)id)
		{
			continue;
		}
		if(o->type == 0x2041 || o->type == 0x2044 || o->type == 0x2042 || o->type == 0x2045 || o->type == 0x2040)
		{
			e = (enemy_t *)o->data;
			if(e->ai_state & 0x6000)
			{
				continue;
			}
			dx = e->x - np[0];
			dz = e->z - np[2];
			d = fsqrt(dx * dx + dz * dz);
			if(d < e->radius)
			{
				dx = e->x - pos[0];
				dz = e->z - pos[2];
				if(d < fsqrt(dx * dx + dz * dz))
				{
					return(1);
				}
			}
		}
	}
	return(0);
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


/* 0x800ce63c: an enemy's scale: s times the model units' per-axis scale (-8.3333, 10, 8.3333:
   the models' x and z run the other way), and its radius from its own base radius */
void set_enemy_scale(enemy_t *e, float s)
{
	e->scale[0] = s * -8.3333f;
	e->scale[1] = s * 10.0f;
	e->scale[2] = s * 8.3333f;
	e->radius = e->unkcc * e->scale[1];
}

/* 0x800cdc38: the enemy fields an init does not set: the effects list and the spare words */
void clear_enemy_fields(enemy_t *e)
{
	e->unk9c = 0.0f;
	e->unka0 = 0.0f;
	e->unka4 = 0;
	e->effects = 0;
	e->unk54 = 0;
	e->unkd8 = 0;
	e->unkdc = 0;
	e->unke0 = 0;
	e->unke4 = 0;
	e->unke8 = 0;
	e->unk6 = 0;
}

/* 0x800ce2f8: the point d units nearer the camera than pos (view space z less d) */
void point_toward_camera(float *pos, float d, float *out)
{
	world_to_view(pos, out);
	out[2] = out[2] - d;
	view_to_world(out);
}

/* 0x800ce294: the player the next enemy goes for: the two players in turn, each turn
   falling back to the other when its player is not in the game */
int	next_target_second;			/* 0x80179d48 (.data) */
player_stats_t *get_target_player(void)
{
	player_stats_t	*p;

	if(next_target_second == 0)
	{
		if(player_stats[0].state == 4)
		{
			p = &player_stats[0];
		}
		else
		{
			p = &player_stats[1];
		}
		next_target_second = 1;
	}
	else
	{
		if(player_stats[1].state == 4)
		{
			p = &player_stats[1];
		}
		else
		{
			p = &player_stats[0];
		}
		next_target_second = 0;
	}
	return(p);
}

/* 0x800cf5ac: |x| */
float ffabs(float x)
{
	if(0.0f <= x)
	{
		return(x);
	}
	return(-x);
}

/* 0x800cf240: scale a vector so that its largest component is 1 */
void vec_max_normalize(float *v)
{
	float	ax, ay, az, m;

	ax = ffabs(v[0]);
	ay = ffabs(v[1]);
	az = ffabs(v[2]);
	if(ay < ax)
	{
		m = az < ax ? ax : az;
	}
	else
	{
		m = az < ay ? ay : az;
	}
	m = 1.0f / m;
	v[0] = v[0] * m;
	v[1] = v[1] * m;
	v[2] = v[2] * m;
}

/* ---- the shot tests and the animation blends (0x800cdeac-0x800cf028) ---- */

extern void	world_to_view(float *, float *), view_to_world(float *);	/* vecmat.c */
extern void	rot_matrix_zyx(float *, float *), mat_vec_mul(float *, float *, float *);

/* 0x800ce344: the point dx to the right, dy up and dz nearer than pos as the camera sees
   it, back in the world */
void offset_in_view(float *pos, float dx, float dy, float dz, float *out)
{
	world_to_view(pos, out);
	out[0] = out[0] + dx;
	out[1] = out[1] + dy;
	out[2] = out[2] - dz;
	view_to_world(out);
}

/* 0x800cdc00: break the enemy's links to its partners: a holder and the one it holds point
   at each other through +0xd8/+0xdc, a second pair through +0xe0/+0xe4 */
void unlink_enemy(enemy_t *e)
{
	if(e->unke0)
	{
		((enemy_t *)e->unke0)->unke4 = 0;
	}
	if(e->unke4)
	{
		((enemy_t *)e->unke4)->unke0 = 0;
	}
	if(e->unkdc)
	{
		((enemy_t *)e->unkdc)->unkd8 = 0;
	}
	if(e->unkd8)
	{
		((enemy_t *)e->unkd8)->unkdc = 0;
	}
}

/* a chance table entry for pick_random_action: entries are tried in order and the first
   whose cumulative chance reaches a roll of 0..99 is taken */
typedef struct action_chance
{
	int		chance;
	unsigned short	state;
	short		frame;
} action_chance_t;

void	start_blend(enemy_t *, submesh_t *, int);
extern int	randrng(int);

/* 0x800cf098: switch the enemy to a random action from a chance table (rolling again if it
   would be the action it is already doing) and blend over len frames to the action's first
   animation frame */
void pick_random_action(enemy_t *e, action_chance_t *table, submesh_t *subs, int n, int len)
{
	action_chance_t	*a;
	int		r;

	a = table;
again:
	r = randrng(100);
	while(a->chance < r)
	{
		a++;
	}
	if(a->state == e->ai_state)
	{
		a = table;
		goto again;
	}
	e->ai_state = a->state;
	if(e->frame > 0)
	{
		e->frame--;
	}
	if(e->frame != a->frame)
	{
		e->frame_to = a->frame;
		e->frame_from = e->frame;
		e->frame = a->frame;
		e->blend_pos = 0;
		e->blend_len = len;
		e->flags |= 2;
		start_blend(e, subs, n);
	}
}

/* 0x800cf190: the player an enemy's attack lands on: its target, unless both players are in
   the game, when the enemy's place on the screen decides: left of 180 player 1, right of
   220 player 2 (off screen keeps the target) */
player_stats_t *attack_target(enemy_t *e)
{
	player_stats_t	*p;
	float		scr[3];

	p = e->target;
	if(player_stats[0].state == 4 && player_stats[1].state == 4)
	{
		if(project_point(&e->x, scr) == -1.0f)
		{
			return(p);
		}
		if(scr[0] < 180.0f)
		{
			p = &player_stats[0];
		}
		if(220.0f < scr[0])
		{
			p = &player_stats[1];
		}
	}
	return(p);
}

/* 0x800cde74: move the enemy by (dx, dy, dz) with no collision test; always 1 (the free
   counterpart of move_cd, used by the steps toward a goal near the camera) */
int func_800cde74(enemy_t *e, float dx, float dy, float dz)
{
	e->x = e->x + dx;
	e->y = e->y + dy;
	e->z = e->z + dz;
	return(1);
}

/* 0x800cdeac: the player's shot against a sphere of radius^2 r2 at pos: the shot's view-space
   ray is extended to the sphere's depth, the movie must not be nearer than the sphere (the
   hit point under the crosshair), and on a hit the aim point is converted to the world */
int shot_hits_sphere(float *pos, float r2, player_stats_t *p)
{
	float	v[3];
	float	a[3];

	world_to_view(pos, v);
	if((double)v[2] < 1.1 || v[2] > 400.0f)
	{
		return(0);
	}
	p->aim[2] = v[2] * p->aim_dir[2];
	p->aim[0] = p->aim_dir[0] * p->aim[2];
	p->aim[1] = p->aim_dir[1] * p->aim[2];
	world_to_view(p->hit_point, a);
#ifdef PORT_HOST
	{ extern int port_trace; if(port_trace) fprintf(stderr, "[shot]   sphere at view (%.1f %.1f %.1f) r2 %.1f: aim (%.1f %.1f %.1f), the movie at %.1f\n", v[0], v[1], v[2], r2, p->aim[0], p->aim[1], p->aim[2], a[2]); }
#endif
	if(a[2] < v[2])
	{
		return(0);
	}
	if((p->aim[0] - v[0]) * (p->aim[0] - v[0]) + (p->aim[1] - v[1]) * (p->aim[1] - v[1]) < r2)
	{
		p->aim[2] = v[2];
		view_to_world(p->aim);
		return(1);
	}
	return(0);
}

/* 0x800cdfb4: the first of n submeshes (visible, or a hit target) whose sphere the shot
   lands in, or -1 */
int find_submesh_hit(enemy_t *e, submesh_t *subs, int n, player_stats_t *p)
{
	int	i;
	group_t	*g;
	float	r2;
	float	v[3];
	float	a[3];
	int	hit;

	for(i = 0; i < n; i++, subs++)
	{
		g = &e->groups[i];
		if(!(g->flags & 0x3001))
		{
			continue;
		}
		r2 = (subs->radius * e->scale[0]) * (subs->radius * e->scale[0]);
		world_to_view(g->pivot, v);
		if((double)v[2] < 1.1 || v[2] > 400.0f)
		{
			hit = 0;
		}
		else
		{
			p->aim[2] = v[2] * p->aim_dir[2];
			p->aim[0] = p->aim_dir[0] * p->aim[2];
			p->aim[1] = p->aim_dir[1] * p->aim[2];
			world_to_view(p->hit_point, a);
			if(a[2] < v[2])
			{
				hit = 0;
			}
			else if((p->aim[0] - v[0]) * (p->aim[0] - v[0]) + (p->aim[1] - v[1]) * (p->aim[1] - v[1]) < r2)
			{
				p->aim[2] = v[2];
				view_to_world(p->aim);
				hit = 1;
			}
			else
			{
				hit = 0;
			}
		}
		if(hit)
		{
			return(i);
		}
	}
	return(-1);
}

/* 0x800ce720: a blend has been set up (frame_from, frame_to, blend_len): the root motion
   between the two frames, turned by the enemy's rotation, becomes its velocity */
void start_blend(enemy_t *e, submesh_t *subs, int n)
{
	float	d[3];
	float	m[9];
	frame_t	*anim;

	rot_matrix_zyx(m, &e->rx);
	anim = subs[n].anim;
	d[0] = anim[e->frame_from].pos[0] - anim[e->frame_to].pos[0];
	d[1] = anim[e->frame_from].pos[1] - anim[e->frame_to].pos[1];
	d[2] = anim[e->frame_from].pos[2] - anim[e->frame_to].pos[2];
	mat_vec_mul(d, m, d);
	e->vel[0] = d[0] * e->scale[0] / (float)e->blend_len;
	e->vel[1] = 0;
	e->vel[2] = d[2] * e->scale[2] / (float)e->blend_len;
}

/* 0x800ce884: jump to a frame, moving the enemy by the root motion between the frames */
void blend_from_pose(enemy_t *e, submesh_t *subs, int n, int frame)
{
	float	d[3];
	float	m[9];
	frame_t	*anim;

	rot_matrix_zyx(m, &e->rx);
	anim = subs[n].anim;
	d[0] = anim[e->frame].pos[0] - anim[frame].pos[0];
	d[1] = anim[e->frame].pos[1] - anim[frame].pos[1];
	d[2] = anim[e->frame].pos[2] - anim[frame].pos[2];
	mat_vec_mul(d, m, d);
	e->x = e->x + d[0] * e->scale[0];
	e->z = e->z + d[2] * e->scale[2];
	e->frame = frame;
}

/* 0x800ced14: blend from the current frame to another over len draws */
void blend_to_frame(enemy_t *e, submesh_t *subs, int n, int frame, int len)
{
	short	old;

	if(e->frame != frame)
	{
		old = e->frame;
		e->frame_to = frame;
		e->frame = frame;
		e->blend_pos = 0;
		e->blend_len = len;
		e->flags |= 2;
		e->frame_from = old;
		start_blend(e, subs, n);
	}
}

/* 0x800cef8c: a hit reaction: remember the state and frame to come back to, then jump to
   the reaction's frame (through the blend when one is running) */
void start_hit_reaction(enemy_t *e, int frame, int state, submesh_t *subs, int n)
{
	unsigned short old_state;
	short	old_frame;

	if(e->flags & 2)
	{
		old_state = e->ai_state;
		old_frame = e->frame;
		e->flags &= 0xfffd;
		e->frame = frame;
		e->ai_state = state;
		e->resume_state = old_state;
		e->resume_frame = old_frame;
	}
	else if(!(e->ai_state & 0x1000))
	{
		e->resume_state = e->ai_state;
		e->resume_frame = e->frame;
		blend_from_pose(e, subs, n, frame);
		e->ai_state = state;
	}
	else if(state != e->ai_state)
	{
		blend_from_pose(e, subs, n, frame);
		e->ai_state = state;
	}
	else
	{
		e->frame = frame;
		e->ai_state = state;
	}
}

/* ---- the shot (0x800ccbd0) ---- */

extern void	screen_to_ray(float *, float, float), screen_to_point(float *, float, float, float);	/* vecmat.c */
extern float	movie_depth_at(float, float);
float	D_801faca4, D_801facb4;			/* the debug aim point (a screen position) */

/* 0x800ccbd0: a player's shot: the aim ray from the screen position, the hit point from
   the movie's depth under it, every object's hit test, then the hit object's die callback */
void shoot_objects(player_stats_t *p)
{
	int	x, y;
	float	depth;
	game_object_t *obj;
	short	i;

	if(debug_mode & 2)
	{
		x = *(int *)&D_801faca4;
		y = *(int *)&D_801facb4;
	}
	else
	{
		x = *(int *)&p->shot_x;
		y = *(int *)&p->shot_y;
	}
	screen_to_ray(p->aim_dir, *(float *)&x, *(float *)&y);
	p->aim_dir[2] = 1.0f / p->aim_dir[2];
	p->hit = 0;
	p->unk10 = p->colour;
	depth = movie_depth_at(p->shot_x, p->shot_y);
	p->shot_depth = depth;
	if(depth <= 1.0f)
	{
		p->shot_depth = 1.1f;
	}
	screen_to_point(p->hit_point, *(float *)&x, *(float *)&y, p->shot_depth);
	view_to_world(p->hit_point);
#ifdef PORT_HOST
	{ extern int port_trace; extern unsigned long hal_frame_count;
	  if(port_trace) fprintf(stderr, "[shot] vblank %lu player %d at (%.0f, %.0f) depth %.1f aim (%.3f %.3f %.3f) point (%.1f %.1f %.1f)\n", hal_frame_count, p->index, p->shot_x, p->shot_y, p->shot_depth, p->aim_dir[0], p->aim_dir[1], p->aim_dir[2], p->hit_point[0], p->hit_point[1], p->hit_point[2]); }
#endif
	i = objects[0].next;
	while(i != OBJ_NONE)
	{
		obj = &objects[i];
		i = obj->next;
		if(obj->hit)
		{
			obj->hit(obj, p);
		}
	}
	if(p->hit == 1)
	{
#ifdef PORT_HOST
		{ extern int port_trace; if(port_trace) fprintf(stderr, "[shot] hit object %d type %04x group %d weapon %#x damage %.1f\n", p->hit_id, objects[p->hit_id].type, p->hit_group, p->shot_weapon, p->shot_damage); }
#endif
		((int (*)(game_object_t *, player_stats_t *))p->die)(&objects[p->hit_id], p);
	}
}
