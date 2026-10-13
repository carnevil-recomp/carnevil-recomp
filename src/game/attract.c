/*
 * attract.c - CarnEvil's attract mode: game_proc (the "attract" process) cycles through
 * eight steps: movies (intro, attract, Midway), screens drawn from texmaps, and the demo
 * play mode. One original file: 0x800caf80-0x800cbb6c (text), data at 0x80179c50-0x80179d40.
 */
#include <stdio.h>
#include "texmap.h"
#include "movie.h"

typedef struct game_object game_object_t;
/* the attract code sees the object functions without prototypes: int results, cast to short */
extern short	add_object1_wrapper(int, void (*)(), int, int);
extern void	remove_game_object(short);
extern void	remove_all_objects(void);

extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern void	normal_screen(void), black_screen(void);
extern void	fade(float, int, void (*)(void));
extern void	preload_wave(int);
extern void	set_bgnd_color(unsigned int);
extern void	sleep(int);
extern void	set_frame_factor(int);
extern void	func_800e74f0(void), func_800e7568(void);	/* movie play start / stop */
extern void	play_attract_cue(int, char *), stop_attract_cue(void);
extern int	both_guns_bit4(void);
extern int	attract_high_scores(int);
extern void	guTexSource(int), guColorCombineFunction(int);
extern void	draw_quad(float, float, float, float, float, float);
extern int	violence_level;
#include "credits.h"			/* game_phase (0x80212e14) */
#include "waves.h"
extern int	attract_cycles;		/* 0x80212d98 */
extern void	grDepthBufferFunction(int), grDepthMask(int);
extern int	check_for_freeplay(void), check_credits_to_start_mode(void), check_credits_to_continue_mode(void);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_centered(int, char *, float, float, unsigned int);
extern char	text_buffer[];		/* 0x801e7100 */
extern char	attract_text1[], attract_text2[], attract_text3[];	/* 0x8021eed0, 0x8021ef58, 0x8021eff0 */
extern int	attract_text_state;	/* 0x801fad18 */


int movie_load_intro(int);
int movie_load_attract(int);
int movie_load_midway(int);
int attract_playmode(int);
int attract_movie_violence(int);
int attract_generic(int);
int attract_screen(int);

int	(*attract_steps[8])(int) =
{
	movie_load_intro, attract_playmode, attract_movie_violence, attract_screen,
	movie_load_attract, attract_high_scores, movie_load_midway, attract_generic
};

texmap_t	disclaimer_texmap[3] =
{
	{ 0x7200, 1, 0xffffff, 0, "3DFXLEG.WMS" },
	{ 0x7203, 1, 0xffffff, 0, "GROUP08.WMS" },
	{ 0x7204, 1, 0xffffff, 0, "LEGAL1.WMS" },
};
texmap_t	attract_texmap[1] =
{
	{ 0x7202, 1, 0xffffff, 0, "AAMAV.WMS" },
};
texmap_t	attract_violent_texmap[1] =
{
	{ 0x7202, 1, 0xffffff, 0, "AAMANV.WMS" },
};
texmap_t	preattract_texmap[1] =
{
	{ 0x7201, 1, 0xffffff, 0, "GAMEOV01.WMS" },
};
int		attract_started = 0;		/* 0x80179d30 */
int		credits_step_counter = 0;	/* 0x80179d34 */
int		play_mode_timer = 0;		/* 0x80179d38 */
int		credit_line_timer = 0;		/* 0x80179d3c */

static void draw_screen_7201(game_object_t *);
void do_play_mode(game_object_t *);

void load_attract_texmap(void)
{
	load_texmap(disclaimer_texmap, 3, "attract");
	if(violence_level >= 2)
	{
		load_texmap(attract_texmap, 1, "attractv");
	}
	else
	{
		load_texmap(attract_violent_texmap, 1, "attractnv");
	}
}

void load_aamav_texmap(void)
{
	load_texmap(preattract_texmap, 1, "preattract");
}

void remove_attract_texmap(void)
{
	if(violence_level >= 2)
	{
		remove_surface(attract_texmap, 1);
	}
	else
	{
		remove_surface(attract_violent_texmap, 1);
	}
	remove_surface(disclaimer_texmap, 3);
	remove_surface(preattract_texmap, 1);
}

void game_proc(void)
{
	int	i;
	int	step;

	load_texmap(preattract_texmap, 1, "preattract");
	if(attract_started)
	{
		normal_screen();
		add_object1_wrapper(0x20, draw_screen_7201, 0, 0);
	}
	preload_wave(WAVE_ATTRACT);
	remove_all_objects();
	game_phase = 1;
	set_bgnd_color(0xff000000);
	i = 0;
	if(!attract_started)
	{
		attract_started = 1;
		i = 8;
	}
	for(;; i++)
	{
		step = i & 7;
#ifdef PORT_HOST
		{ extern int port_trace; extern unsigned long hal_frame_count; if(port_trace) fprintf(stderr, "[attract] vblank %lu step %d\n", hal_frame_count, step); }
#endif
		attract_steps[step](i);
		if(step == 0)
		{
			attract_cycles++;
		}
	}
}

/* 0x800cb17c: step 1, the demo-play screen for 240 frames */
int attract_playmode(int arg)
{
	int	obj;
	int	i;

	black_screen();
	obj = add_object1_wrapper(0x20, do_play_mode, 0, 0);
	fade(1.0f, 5, 0);
	sleep(5);
	for(i = 0; i < 240; i++)
	{
		sleep(1);
	}
	fade(0.0f, 5, 0);
	sleep(5);
	remove_game_object(obj);
	sleep(1);
	return(1);
}

int movie_load_intro(int arg)
{
	sleep(6);
	movie_load(3);
	set_frame_factor(2);
	func_800e74f0();
	play_attract_cue(1, "intro4");
	fade(1.0f, 10, 0);
	while(!(movie.flags & 4))
	{
		sleep(1);
	}
	func_800e7568();
	stop_attract_cue();
	set_frame_factor(3);
	return(0);
}

int movie_load_attract(int arg)
{
	if(violence_level < 3)
	{
		return(0);
	}
	sleep(6);
	movie_load(9);
	set_frame_factor(2);
	func_800e74f0();
	play_attract_cue(1, "attract");
	fade(1.0f, 10, 0);
	while(!(movie.flags & 4))
	{
		sleep(1);
	}
	func_800e7568();
	stop_attract_cue();
	set_frame_factor(3);
	return(0);
}

int movie_load_midway(int arg)
{
	sleep(6);
	movie_load(4);
	func_800e74f0();
	play_attract_cue(1, "midway");
	fade(1.0f, 10, 0);
	while(!(movie.flags & 4))
	{
		sleep(1);
	}
	func_800e7568();
	stop_attract_cue();
	return(0);
}

int attract_movie_violence(int arg)
{
	sleep(6);
	if(violence_level >= 2)
	{
		movie_load(6);
	}
	else
	{
		movie_load(0xb);
	}
	func_800e74f0();
	fade(1.0f, 10, 0);
	while(!(movie.flags & 4))
	{
		sleep(1);
	}
	func_800e7568();
	return(0);
}

static void draw_screen_7200(game_object_t *);
static void draw_screen_7202(game_object_t *);
static void draw_screen_7203(game_object_t *);

int attract_generic(int arg)
{
	int	frames = 120;
	int	obj;
	int	i;

	black_screen();
	if(both_guns_bit4())
	{
		frames = 600;
		obj = add_object1_wrapper(0x20, draw_screen_7203, 0, 0);
	}
	else
	{
		obj = add_object1_wrapper(0x20, draw_screen_7200, 0, 0);
	}
	play_attract_cue(0x58, "generic1");
	fade(1.0f, 5, 0);
	sleep(5);
	for(i = 0; i < frames; i++)
	{
		sleep(1);
	}
	fade(0.0f, 5, 0);
	sleep(5);
	remove_game_object(obj);
	sleep(1);
	stop_attract_cue();
	return(1);
}

int attract_screen(int arg)
{
	int	obj;
	int	i;

	black_screen();
	obj = add_object1_wrapper(0x20, draw_screen_7202, 0, 0);
	sleep(2);
	fade(1.0f, 5, 0);
	for(i = 0; i < 120; i++)
	{
		sleep(1);
	}
	fade(0.0f, 5, 0);
	sleep(5);
	remove_game_object(obj);
	sleep(1);
	return(1);
}

/* 0x800cb61c: an unused step: after ten frames of the test switch, reload the credits */
extern int	both_guns_bit5(void);			/* a switch-page test */
extern void	load_credits(void);
int attract_credits_step(int arg)
{
	if(both_guns_bit5())
	{
		credits_step_counter = 10;
	}
	credits_step_counter++;
	if(credits_step_counter < 10)
	{
		return(0);
	}
	credits_step_counter = 0;
	load_credits();
	return(1);
}

/* full-screen texmap draws used by the attract screens */
static void draw_screen_7202(game_object_t *obj)
{
	guTexSource(find_surface(0x7202)->handle);
	guColorCombineFunction(4);
	draw_quad(200.0f, 128.0f, 400.0f, 256.0f, 255.0f, 255.0f);
}

static void draw_screen_7201(game_object_t *obj)
{
	guTexSource(find_surface(0x7201)->handle);
	guColorCombineFunction(4);
	draw_quad(200.0f, 128.0f, 400.0f, 256.0f, 255.0f, 255.0f);
}

static void draw_screen_7200(game_object_t *obj)
{
	guTexSource(find_surface(0x7200)->handle);
	guColorCombineFunction(4);
	draw_quad(200.0f, 128.0f, 400.0f, 256.0f, 255.0f, 255.0f);
}

static void draw_screen_7203(game_object_t *obj)
{
	guTexSource(find_surface(0x7203)->handle);
	guColorCombineFunction(4);
	draw_quad(200.0f, 128.0f, 400.0f, 256.0f, 255.0f, 255.0f);
}

/* 0x800cb800: the "play mode" attract screen: legal texmap, credits line, blinking messages */
void do_play_mode(game_object_t *obj)
{
	guTexSource(find_surface(0x7204)->handle);
	guColorCombineFunction(4);
	draw_quad(200.0f, 128.0f, 400.0f, 256.0f, 255.0f, 255.0f);
	grDepthBufferFunction(7);
	grDepthMask(0);
	if(check_for_freeplay())
	{
		draw_string_centered(1, "FREE PLAY", 200.0f, 140.0f, 0xff00);
	}
	else if(play_mode_timer < 40)
	{
		if(check_credits_to_start_mode() == 1)
		{
			graphical_printf(text_buffer, "%d CREDIT TO START", check_credits_to_start_mode());
		}
		else
		{
			graphical_printf(text_buffer, "%d CREDITS TO START", check_credits_to_start_mode());
		}
		draw_string_centered(1, text_buffer, 200.0f, 140.0f, 0xaaaaaa);
		if(check_credits_to_continue_mode() == 1)
		{
			graphical_printf(text_buffer, "%d CREDIT TO CONTINUE", check_credits_to_continue_mode());
		}
		else
		{
			graphical_printf(text_buffer, "%d CREDITS TO CONTINUE", check_credits_to_continue_mode());
		}
		draw_string_centered(1, text_buffer, 200.0f, 110.0f, 0x999999);
	}
	else
	{
		switch(attract_text_state)
		{
		case 3:
			draw_string_centered(1, attract_text1, 200.0f, 150.0f, 0x999999);
			draw_string_centered(1, attract_text2, 200.0f, 125.0f, 0x999999);
			draw_string_centered(1, attract_text3, 200.0f, 100.0f, 0x999999);
			break;
		case 2:
			draw_string_centered(1, attract_text1, 200.0f, 140.0f, 0x999999);
			draw_string_centered(1, attract_text2, 200.0f, 110.0f, 0x999999);
			break;
		default:
			draw_string_centered(1, attract_text1, 200.0f, 125.0f, 0x999999);
			break;
		}
	}
	play_mode_timer++;
	if(play_mode_timer >= 81)
	{
		play_mode_timer = 0;
	}
	grDepthMask(1);
	grDepthBufferFunction(3);
}

/* 0x800cbab4: the attract-mode credit lines, alternating every 50 frames */
void attract_credit_lines(void)
{
	grDepthBufferFunction(7);
	grDepthMask(0);
	credit_info(1);
	credit_line_timer += 4;
	if(credit_line_timer < 50)
	{
		draw_string_centered(1, credit_line1, 200.0f, 5.0f, 0x00ffffff);
	}
	else
	{
		draw_string_centered(1, credit_line2, 200.0f, 5.0f, 0x00ffffff);
	}
	if(credit_line_timer >= 101)
	{
		credit_line_timer = 0;
	}
	grDepthMask(1);
	grDepthBufferFunction(3);
}
