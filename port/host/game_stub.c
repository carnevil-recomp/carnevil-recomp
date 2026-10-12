/*
 * game_stub.c - stand-ins for game-side code the port does not have in C yet, plus the port's
 * main_proc: it runs the game's real attract process (src/game/attract.c) on top of the
 * decompiled object/texmap modules. Each stub disappears as its function is decompiled.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <goose/process.h>
#include <goose/object.h>
#include <glide/glide.h>
#include "hal.h"
#include "../../src/game/objects.h"
#include "../../src/game/texmap.h"
#include "../../src/game/movie.h"

extern void sleep(int), die(int), set_frame_factor(int);
extern int interrupt_handler(void);
extern struct process_node *qcreate(char *, int, void (*)(), int, int, int, int);
extern struct process_node *cur_proc;
extern void game_proc(void), load_font_texmap(void);
extern void grDrawTriangleDma(MidVertex *, MidVertex *, MidVertex *, int);

/* ---- globals the game code references ---- */
int	violence_level = 3;
int	D_80212e14;
int	attract_cycles;
int	debug_mode = 0;
int	D_801fad10, D_801e0f10;

/* ---- GOOSE main() callees ---- */
void func_800ca4a8(void) { }					/* records get_free_mem_left() */
void func_8013f5b4(void) { }					/* two I/O ASIC ioctls after graphics init */
int  vblank_handler(void) { return interrupt_handler(); }	/* the game's handler wraps the library's */
int  interrupt_ioasic_handler(void) { return 1; }
void game_dma0_handler(void) { }
void func_80162a10(int *a, int *b, int *c, int *d) { *a = *b = *c = *d = 0; }	/* memory statistics */

/* ---- attract-mode callees not decompiled yet ---- */
void func_800e74f0(void) { movie_start(); }	/* movie play start */
void func_800e7568(void) { movie_stop(); }	/* movie play stop */
/* script callees and level data the attract movies touch */
int  func_8012fa20(void) { fprintf(stderr, "[stub] script call func_8012fa20\n"); return 0; }
int  func_8012fbf0(void) { fprintf(stderr, "[stub] script call func_8012fbf0\n"); return 0; }
void func_800ebdbc(int a, char *s) { fprintf(stderr, "[stub] level sound %d \"%s\"\n", a, s); }
void func_800ebe34(int a, char *s) { (void)a; (void)s; }
void func_800ebd70(int a, char *s) { (void)a; (void)s; }
void func_800ebe6c(void) { }
void func_800ebe8c(void) { }
void func_800ebb44(void) { }
void func_800d4968(void) { }
void func_800ec238(void) { }
void func_800ec1e0(int a, char *s) { fprintf(stderr, "[stub] sound cue %d \"%s\"\n", a, s); }
void func_800ec370(int a, int b) { (void)a; (void)b; }
void func_800ec504(int a) { (void)a; }
void func_801400a4(int a, int b) { (void)a; (void)b; }
void func_801400dc(int a, int b, int c) { (void)a; (void)b; (void)c; }
void *func_800d6758(int a) { (void)a; return 0; }
void level_end_stats_wrapper(void) { }
int  end_of_wave(void) { return 0; }
float func_800c9158(unsigned int *px) { return (float)(*px >> 16); }	/* depth word -> distance (placeholder scale) */
int  player_stats[64], D_801e1084, D_801e6e70, D_8018db90;
script_word_t hhouse_script[1], freakshow_script[1], rickity_town_script[1], bigtop_script[1];	/* level scripts: not extracted yet */
void func_800ec260(int a, char *cue) { fprintf(stderr, "[stub] sound cue %d \"%s\"\n", a, cue); }
void func_800ec2c8(void) { }
int  func_800d4990(void) { return 0; }		/* I/O ASIC bit test */
int  sub_800e4d90(int arg) { (void)arg; return 0; }	/* high-score reset screen */
extern void load_attract_texmap(void), load_aamav_texmap(void);
void preload_wave(int w) { fprintf(stderr, "[stub] preload_wave(%#x): level assets not loaded, attract texmaps only\n", w); load_attract_texmap(); load_aamav_texmap(); }

float get_screen_pos(int pos, float *out) { (void)pos; out[0] = out[1] = 0.0f; return -1.0f; }	/* 3D point -> screen: no camera yet */
void graphical_printf(char *buf, char *fmt, ...) { va_list ap; va_start(ap, fmt); vsprintf(buf, fmt, ap); va_end(ap); }
int is_freeplay = 0, coins_needed_to_play = 2, num_credits_to_continue = 1;
int check_for_freeplay(void) { return is_freeplay; }
int check_credits_to_start_mode(void) { return coins_needed_to_play; }
int check_credits_to_continue_mode(void) { return num_credits_to_continue; }
char attract_text1[136] = "PRESS START", attract_text2[136] = "TO PLAY", attract_text3[136];
int attract_text_state = 2;

/* the triangle setup (0x800d1750) writes Voodoo commands; here it is a plain draw */
void draw_tri(MidVertex *a, MidVertex *b, MidVertex *c) { grDrawTriangleDma(a, b, c, 0); }
/* 0x800d1d8c / 0x800d2664: the same with iterated RGB / alpha vertices (iteration ignored for now) */
void draw_tri_rgb(void *a, void *b, void *c) { grDrawTriangleDma(a, b, c, 0); }
void draw_tri_alpha(void *a, void *b, void *c) { grDrawTriangleDma(a, b, c, 0); }

/* ---- the per-frame passes the game installs as GOOSE objects in func_800ca3a0 ---- */
static void think_pass(void *data)		/* "BAR": object_draw_func (0x800cca70) */
{
	game_object_t	*p;
	short		i;
	(void)data;
	for(i = objects[0].next; i != OBJ_NONE; i = p->next)
	{
		p = &objects[i];
		if(p->think) p->think(p);
	}
}
extern unsigned int background_color;

static void draw_pass(void *data)		/* "FOO": print_onscreen_debuginfo's object loop */
{
	game_object_t	*p;
	short		i;
	(void)data;
	/* 0x800e77ec: put the movie frame up, or clear when no movie is playing */
	show_debug_frame_data();
	for(i = objects[0].next; i != OBJ_NONE; i = p->next)
	{
		p = &objects[i];
		if(p->draw) p->draw(p);
	}
}

static void game_init(void)			/* func_800ca3a0, the parts the port has */
{
	int	i;
	init_objects();
	for(i = 0; i < MAX_SURFACES; i++) { surfaces[i].key = -1; surfaces[i].refs = 0; surfaces[i].index = -1; }
	load_fram_wrapper();
	load_font_texmap();
	create_object("FOO", 0, 0, 1, 0, draw_pass);
	create_object("BAR", 0, 0, 0, 0, think_pass);
}

void main_proc(int *args)
{
	(void)args;
	fprintf(stderr, "[port] main_proc: starting the attract process\n");
	game_init();
	set_frame_factor(3);
	qcreate("attract", 0x100, game_proc, 0, 0, 0, 0);
	while(1) sleep(1);
}
