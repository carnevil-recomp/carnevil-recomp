/*
 * objects.c - CarnEvil's game object list: a fixed array of 128 nodes threaded into a list
 * ordered by type, with the head at index 0. Functions at 0x800cc398-0x800cc9c4.
 */
#include <stdio.h>
#include <glide/glide.h>

#include "objects.h"
#include "credits.h"			/* game_mode, player_stats */
#include "levelsnd.h"
#include "movie.h"

extern int	get_free_mem_left(void);		/* 0x80143e90 */
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string(int, char *, float, float);
extern void	draw_string_color(int, char *, float, float, unsigned int);
extern void	draw_string_centered(int, char *, float, float, unsigned int);
extern void	attract_credit_lines(void);
extern void	show_volume_level(void);
extern float	get_screen_pos(float *, float *);	/* 0x800d07a8 */
extern void	func_800c9908(void);			/* per frame after the movie: reads the players' switches */
extern void	func_800d41b4(int), show_debug_light_(int), func_800e9140(int);	/* the level's 3D draws (enemies, light, stats) */
extern void	func_800d6e94(void), func_800ff418(void);
extern player_stats_t *something_to_do_with_shooting_(player_stats_t *);	/* 0x800d5a84 */
extern void	func_800c9d54(void *), func_800ca194(void *), func_800c9f4c(void);	/* debug draws */
extern void	func_800ce2f8(float *, float, float *), func_800c8ef0(float *, float, unsigned int);
extern void	func_800fe580(void), func_800fef18(void), func_800ffbdc(void), func_800fe1fc(void);
extern void	func_800dbb80(void), func_80100558(void), func_800e2fe4(void), func_800db4b8(void), func_800c83bc(void);
extern void	func_800c9620(int), func_800c9bf0(void), print_heap_and_obj_info_onscreen(void);
extern short	func_800cd1f8(void), func_800cd4ac(void), func_800cd7a0(void), func_800cdaa0(void);	/* per-wave think */
extern int	free_mem_left;				/* 0x801e0ee8 */
extern int	D_801e5cbc;				/* draw the level's 3D even without debug bit 0 */
extern int	shown_frame;				/* 0x80212da0: the movie frame on screen */
extern int	wave_id_g2;				/* 0x80212e30 */
extern short	D_80212d92;
extern int	RTP_val, DMA_val, D_801e0ecc;		/* 0x80237508, 0x80237414: per-frame triangle/DMA counters */
extern volatile int got_interrupt_save;		/* 0x801e0ebc: vblanks since the last frame, counted by the interrupt handler */
extern int	please_wait_timer;			/* 0x801e7050 */
extern float	D_801de8f0[3];				/* a world point shown by the debug overlay */
extern float	D_80179c00[6], D_80179c44[3];		/* debug vectors */
extern char	text_buffer[];

/* the enemy record fields the debug overlay prints */
typedef struct debug_object
{
	unsigned char	pad0[8];
	short		frame;			/* 0x08 */
	unsigned char	pad1[0xa];
	unsigned short	ai_state;		/* 0x14 */
	unsigned char	pad2[2];
	short		num;			/* 0x18 */
	unsigned char	pad3[0xa];
	float		x, y, z;		/* 0x24 */
	float		rx, ry, rz;		/* 0x30 */
	unsigned char	pad4[0x58];
	short		path;			/* 0x94 */
} debug_object_t;
extern debug_object_t	*D_80179bfc;			/* the enemy being debugged */
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
float object_screen_pos(debug_object_t *obj, float *out)
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
	func_800c9908();
	if(D_801e5cbc || (debug_mode & 1))
	{
		func_800d41b4(frame);
		show_debug_light_(frame);
		func_800e9140(frame);
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
	if(debug_mode & 0x10000)
	{
		if(D_80179bfc)
		{
			func_800c9d54(D_80179bfc);
			graphical_printf(text_buffer, "%d XYZ %f %f %f", D_80179bfc->num, D_80179bfc->x, D_80179bfc->y, D_80179bfc->z);
			draw_string_color(0, text_buffer, 0.0f, 186.0f, 0xff00);
			graphical_printf(text_buffer, "ROT %f %f %f", D_80179bfc->rx, D_80179bfc->ry, D_80179bfc->rz);
			draw_string_color(0, text_buffer, 0.0f, 166.0f, 0xff00);
			graphical_printf(text_buffer, "AI 0X%04X FRAME %d PATH %d", D_80179bfc->ai_state, D_80179bfc->frame, D_80179bfc->path);
			draw_string_color(0, text_buffer, 0.0f, 146.0f, 0xff00);
		}
		func_800ce2f8(D_801de8f0, -30.0f, pt);
		func_800c8ef0(pt, 1.0f, 0xffff00ff);
		graphical_printf(text_buffer, "XYZ %f %f %f", pt[0], pt[1], pt[2]);
		draw_string_color(0, text_buffer, 40.0f, 20.0f, 0xffffff);
		draw_string_color(0, text_buffer, 40.0f, 0.0f, 0);
	}
	if((debug_mode & 0x20000) && D_80194650)
	{
		func_800ca194(D_80194650);
		graphical_printf(text_buffer, "XYZ %f %f %f", D_80194650[0], D_80194650[1], D_80194650[2]);
		draw_string_color(0, text_buffer, 0.0f, 80.0f, 0xff00);
		graphical_printf(text_buffer, "ROT %f %f %f", D_80179c44[0], D_80179c44[1], D_80179c44[2]);
		draw_string_color(0, text_buffer, 0.0f, 60.0f, 0xff00);
	}
	if(debug_mode & 0x80000)
	{
		func_800c9f4c();
		graphical_printf(text_buffer, "XYZ %f %f %f", D_80179c00[0], D_80179c00[1], D_80179c00[2]);
		draw_string_color(0, text_buffer, 0.0f, 30.0f, 0xff00);
		graphical_printf(text_buffer, "XYZ %f %f %f", D_80179c00[3], D_80179c00[4], D_80179c00[5]);
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
	if(game_mode == 0x80)
	{
		if(please_wait_timer > 0)
		{
			draw_string_centered(1, "PLEASE WAIT", 200.0f, 28.0f, (please_wait_timer * 0x10101) << 4);
			please_wait_timer--;
		}
	}
	if(game_mode == 1)
	{
		attract_credit_lines();
	}
	if(debug_mode & 0x10000)
	{
		debug_blink++;
		if(debug_blink & 2)
		{
			graphical_printf(text_buffer, "DEBUGGING CHARACTERS");
			draw_string_color(0, text_buffer, 10.0f, 240.0f, 0xff0000);
		}
	}
	if(debug_mode & 4)
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
	if(debug_mode & 0x8000)
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

short add_object1_wrapper(short type, void (*draw)(game_object_t *), int (*init)(game_object_t *), int think)
{
	game_object_t	t;

	t.type = type;
	t.draw = draw;
	t.init = init;
	t.think = (void (*)(game_object_t *))think;
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
	switch(wave_id_g2)
	{
	case 0x1020000:
		D_80212d92 = func_800cd1f8();
		break;
	case 0x1060000:
		D_80212d92 = func_800cd4ac();
		break;
	case 0x1040000:
		D_80212d92 = func_800cd7a0();
		break;
	case 0x1070000:
		D_80212d92 = func_800cdaa0();
		break;
	default:
		D_80212d92 = 0;
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
