/*
 * attract.c - CarnEvil's attract mode: game_proc (the "attract" process) cycles through
 * eight steps: movies (intro, attract, Midway), screens drawn from texmaps, and the demo
 * play mode. Functions at 0x800cb084-0x800cb800.
 */
#include <stdio.h>
#include "texmap.h"

typedef struct game_object game_object_t;
/* the attract code sees the object functions without prototypes: int results, cast to short */
extern int	add_object1_wrapper();
extern void	remove_game_object(short);
extern void	remove_all_objects(void);

extern void	load_texmap(texmap_t *, int, char *);
extern void	remove_surface(texmap_t *, int);
extern void	normal_screen(void), black_screen(void);
extern void	fade(float, int, void (*)(void));
extern void	preload_wave(int);
extern void	set_bgnd_color(unsigned int);
extern void	sleep(int);
extern void	movie_load(int);
extern void	set_frame_factor(int);
extern void	func_800e74f0(void), func_800e7568(void);	/* movie play start / stop */
extern void	func_800ec260(int, char *), func_800ec2c8(void);	/* sound cue start / stop */
extern int	func_800d4990(void);
extern int	sub_800e4d90(int);
extern void	guTexSource(int), guColorCombineFunction(int);
extern void	draw_quad(float, float, float, float, float, float);
extern int	violence_level;
extern int	D_801de900;		/* movie status: bit 2 = finished */
extern int	D_80212e14;
extern int	attract_cycles;		/* 0x80212d98 */
extern void	grDepthBufferFunction(int), grDepthMask(int);
extern int	check_for_freeplay(void), check_credits_to_start_mode(void), check_credits_to_continue_mode(void);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_centered(int, char *, float, float, unsigned int);
extern char	credit_text[];		/* 0x801e7100 */
extern char	attract_text1[], attract_text2[], attract_text3[];	/* 0x8021eed0, 0x8021ef58, 0x8021eff0 */
extern int	attract_text_state;	/* 0x801fad18 */
int		play_mode_timer = 0;	/* 0x80179d38 */

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
int		attract_started = 0;	/* 0x80179d30 */

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
	movie_load_attract, sub_800e4d90, movie_load_midway, attract_generic
};

static void draw_screen_7201(game_object_t *);

void load_attract_texmap(void)
{
	load_texmap(disclaimer_texmap, 3, "attract");
	if(violence_level < 2)
	{
		load_texmap(attract_violent_texmap, 1, "attractnv");
	}
	else
	{
		load_texmap(attract_texmap, 1, "attractv");
	}
}

void load_aamav_texmap(void)
{
	load_texmap(preattract_texmap, 1, "preattract");
}

void remove_attract_texmap(void)
{
	if(violence_level < 2)
	{
		remove_surface(attract_violent_texmap, 1);
	}
	else
	{
		remove_surface(attract_texmap, 1);
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
	preload_wave(0xa0000);
	remove_all_objects();
	D_80212e14 = 1;
	set_bgnd_color(0xff000000);
	i = 0;
	if(!attract_started)
	{
		attract_started = 1;
		i = 8;
	}
	for(;;)
	{
		do
		{
			step = i & 7;
			attract_steps[step](i);
			i++;
		} while(step != 0);
		attract_cycles++;
	}
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
			graphical_printf(credit_text, "%d CREDIT TO START", check_credits_to_start_mode());
		}
		else
		{
			graphical_printf(credit_text, "%d CREDITS TO START", check_credits_to_start_mode());
		}
		draw_string_centered(1, credit_text, 200.0f, 140.0f, 0xaaaaaa);
		if(check_credits_to_continue_mode() == 1)
		{
			graphical_printf(credit_text, "%d CREDIT TO CONTINUE", check_credits_to_continue_mode());
		}
		else
		{
			graphical_printf(credit_text, "%d CREDITS TO CONTINUE", check_credits_to_continue_mode());
		}
		draw_string_centered(1, credit_text, 200.0f, 110.0f, 0x999999);
	}
	else if(attract_text_state == 2)
	{
		draw_string_centered(1, attract_text1, 200.0f, 140.0f, 0x999999);
		draw_string_centered(1, attract_text2, 200.0f, 110.0f, 0x999999);
	}
	else if(attract_text_state == 3)
	{
		draw_string_centered(1, attract_text1, 200.0f, 150.0f, 0x999999);
		draw_string_centered(1, attract_text2, 200.0f, 125.0f, 0x999999);
		draw_string_centered(1, attract_text3, 200.0f, 100.0f, 0x999999);
	}
	else
	{
		draw_string_centered(1, attract_text1, 200.0f, 125.0f, 0x999999);
	}
	play_mode_timer++;
	if(play_mode_timer >= 81)
	{
		play_mode_timer = 0;
	}
	grDepthMask(1);
	grDepthBufferFunction(3);
}

int attract_playmode(int arg)
{
	short	obj;
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
	func_800ec260(1, "intro4");
	fade(1.0f, 10, 0);
	while(!(D_801de900 & 4))
	{
		sleep(1);
	}
	func_800e7568();
	func_800ec2c8();
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
	func_800ec260(1, "attract");
	fade(1.0f, 10, 0);
	while(!(D_801de900 & 4))
	{
		sleep(1);
	}
	func_800e7568();
	func_800ec2c8();
	set_frame_factor(3);
	return(0);
}

int movie_load_midway(int arg)
{
	sleep(6);
	movie_load(4);
	func_800e74f0();
	func_800ec260(1, "midway");
	fade(1.0f, 10, 0);
	while(!(D_801de900 & 4))
	{
		sleep(1);
	}
	func_800e7568();
	func_800ec2c8();
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
	while(!(D_801de900 & 4))
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
	short	obj;
	int	i;

	black_screen();
	if(func_800d4990())
	{
		frames = 600;
		obj = add_object1_wrapper(0x20, draw_screen_7203, 0, 0);
	}
	else
	{
		obj = add_object1_wrapper(0x20, draw_screen_7200, 0, 0);
	}
	func_800ec260(0x58, "generic1");
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
	func_800ec2c8();
	return(1);
}

int attract_screen(int arg)
{
	short	obj;
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
