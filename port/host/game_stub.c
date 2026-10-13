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
#include "../../src/game/credits.h"

extern void sleep(int), die(int), set_frame_factor(int);
extern int interrupt_handler(void);
extern struct process_node *qcreate(char *, int, void (*)(), int, int, int, int);
extern struct process_node *cur_proc;
extern void game_proc(void), load_font_texmap(void), main_init_sound(void);
extern void *set_dcsw_handler(int, void (*)(int, int));
extern void grDrawTriangleDma(MidVertex *, MidVertex *, MidVertex *, int);

/* ---- globals the game code references ---- */
int	violence_level = 3;
int	attract_cycles;
int	debug_mode = 0;
int	D_801fad10, D_801e0f10;

/* ---- GOOSE main() callees ---- */
void func_800ca4a8(void) { }					/* records get_free_mem_left() */
extern void scan_switches(void);
extern int tick_counter, gfx_scale, got_interrupt, uptime;
int  D_80212e60, D_801facc8, D_80179d50;
/* 0x800c6030: the game's own vertical-retrace handler (it does not call the library's) */
int  vblank_handler(void)
{
	tick_counter++;
	got_interrupt++;
	if(!(tick_counter % gfx_scale))
	{
		uptime++;
		if(D_80212e60 != 1) D_801facc8++;
	}
	/* if(D_80212e50 == 1) { func_800d4f6c(); func_800d57b0(); }  gun sampling: later */
	scan_switches();
	return 1;
}
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
void func_800ec370(int a, int b) { (void)a; (void)b; }
void func_800ec504(int a) { (void)a; }
void *func_800d6758(int a) { (void)a; return 0; }
void level_end_stats_wrapper(void) { }
int  end_of_wave(void) { return 0; }
float func_800c9158(unsigned int *px) { return (float)(*px >> 16); }	/* depth word -> distance (placeholder scale) */
int  D_801e6e70, D_8018db90;
script_word_t hhouse_script[1], freakshow_script[1], rickity_town_script[1], bigtop_script[1];	/* level scripts: not extracted yet */
int  func_800d4990(void) { return 0; }		/* I/O ASIC bit test */
int  sub_800e4d90(int arg) { (void)arg; return 0; }	/* high-score reset screen */
extern void load_attract_texmap(void), load_aamav_texmap(void);
extern int snd_bank_load_playing(char *);
void preload_wave(int w)		/* 0x800f59dc: only the attract wave's banks and texmaps for now */
{
	fprintf(stderr, "[stub] preload_wave(%#x): level assets not loaded, attract texmaps and banks only\n", w);
	load_attract_texmap(); load_aamav_texmap();
	snd_bank_load_playing("select1"); snd_bank_load_playing("midway"); snd_bank_load_playing("intro4"); snd_bank_load_playing("attract");	/* load_level_sounds, wave 0xa0000 */
	/* load_wave_proc starts the PCM music stream for the level waves (bit 24 set); the attract wave has none.
	   CARNEVIL_STREAM_TEST=<wave> starts one anyway so the stream path can be heard before the levels exist. */
	if(getenv("CARNEVIL_STREAM_TEST"))
	{
		extern void stop_level_cue(void), install_disk_info(void *, void *, void *), stream_start(void), stream_set_volume(int);
		extern int stream_select_wave(int);
		int wave = (int)strtoul(getenv("CARNEVIL_STREAM_TEST"), 0, 0);
		fprintf(stderr, "[stub] CARNEVIL_STREAM_TEST: streaming the music of wave %#x\n", wave);
		stop_level_cue(); install_disk_info(0, 0, 0); stream_set_volume(0xaa);
		stream_select_wave(wave); sleep(3); stream_start();
	}
}

float get_screen_pos(int pos, float *out) { (void)pos; out[0] = out[1] = 0.0f; return -1.0f; }	/* 3D point -> screen: no camera yet */
void graphical_printf(char *buf, char *fmt, ...) { va_list ap; va_start(ap, fmt); vsprintf(buf, fmt, ap); va_end(ap); }
char attract_text1[136], attract_text2[136], attract_text3[136];	/* 0x8021eed0.. filled by something_todo_with_credits */
int attract_text_state;
/* the game proper: not decompiled yet */
void init_player_stats(int p) { fprintf(stderr, "[stub] init_player_stats(%d)\n", p); }
void init_main_game_proc(int p) { fprintf(stderr, "[stub] init_main_game_proc(%d): GAME START requested by player %d, credits now %d\n", p, p + 1, credits); }
void play_level_cue_pri(int scall, char *bank, int pri) { extern unsigned int next_cue_priority(void); extern int snd_scall_bank(char *, unsigned, unsigned, unsigned, unsigned); snd_scall_bank(bank, scall, 0xe6, 0x80, (pri | next_cue_priority()) & 0xdfff); }	/* 0x800ebfa8 */
void dispatch_diags_proc(int closed, int swid) { (void)closed; (void)swid; fprintf(stderr, "[stub] test switch: diagnostics not implemented\n"); }

/* the triangle setup (0x800d1750) writes Voodoo commands; here it is a plain draw */
void draw_tri(MidVertex *a, MidVertex *b, MidVertex *c) { grDrawTriangleDma(a, b, c, 0); }
/* 0x800d1d8c / 0x800d2664: the same with iterated RGB / alpha vertices (iteration ignored for now) */
void draw_tri_rgb(void *a, void *b, void *c) { grDrawTriangleDma(a, b, c, 0); }
void draw_tri_alpha(void *a, void *b, void *c) { grDrawTriangleDma(a, b, c, 0); }
/* 0x800d2174: a constant-colour triangle (the original sorts the vertices by y) */
void draw_tri_flat(void *a, void *b, void *c) { grDrawTriangleDma(a, b, c, 0); }

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
	/* the overlays print_onscreen_debuginfo draws after the objects (0x800cc0ac..) */
	grDepthBufferFunction(GR_CMP_ALWAYS);
	show_volume_level();
	if(game_mode == 1) attract_credit_lines();
}

void game_init(void)			/* func_800ca3a0, the parts the port has */
{
	int	i;
	init_objects();
	for(i = 0; i < MAX_SURFACES; i++) { surfaces[i].key = -1; surfaces[i].refs = 0; surfaces[i].index = -1; }
	load_fram_wrapper();
	load_font_texmap();
	create_object("FOO", 0, 0, 1, 0, draw_pass);
	create_object("BAR", 0, 0, 0, 0, think_pass);
}

/* the boot callees the port does not have yet: level assets, models, the gun calibration */
void load_level_assets(void) { fprintf(stderr, "[stub] load_level_assets (0x800ca448): misc texmaps and models not loaded\n"); }
#define LOAD_STUB(n) void n(void) { }
LOAD_STUB(load_jester_texmap) LOAD_STUB(load_jester_model) LOAD_STUB(load_inng_texmap) LOAD_STUB(load_chnk_texmap)
LOAD_STUB(load_chnk_model) LOAD_STUB(load_shotgun_texmap) LOAD_STUB(load_health_texmap) LOAD_STUB(load_ammmo_texmap)
LOAD_STUB(load_machinegun_texmap) LOAD_STUB(load_flamethrower_texmap) LOAD_STUB(load_acid_texmap) LOAD_STUB(load_shotgun_model)
LOAD_STUB(load_health_model) LOAD_STUB(load_ammo_model) LOAD_STUB(load_machinegun_model) LOAD_STUB(load_flamethrower_model)
LOAD_STUB(load_acid_model)
void cmos_calibration_(void) { fprintf(stderr, "[stub] cmos_calibration_ (0x800d4d00): gun calibration from CMOS not loaded\n"); }
void startaudit(FILE *f) { (void)f; }					/* 0x800ef870: the audit names for audits.fmt */
void game_proc2(void) { fprintf(stderr, "[stub] game_proc2: debug attract not implemented\n"); for(;;) sleep(1); }
void ide_set_partition(int n) { (void)n; }				/* GOOSE IDE: the HAL maps partition 1 itself */
int gfx_scale_from_refresh(void) { return 54; }				/* 0x80157004: frames per second of the display mode */
