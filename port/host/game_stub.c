/*
 * game_stub.c - stand-ins for game-side code the port does not have in C yet, plus the port's
 * main_proc: it runs the game's real attract process (src/game/attract.c) on top of the
 * decompiled object/texmap modules. Each stub disappears as its function is decompiled.
 */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <goose/process.h>
#include <goose/object.h>
#include <glide/glide.h>
#include "hal.h"
#include "../../src/game/objects.h"
#include "../../src/game/credits.h"
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
int	D_801fad10, D_801e0f10;

/* ---- GOOSE main() callees ---- */
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
void func_800d4968(void) { }
void *func_800d6758(int a) { (void)a; return 0; }
void level_end_stats_wrapper(void) { }
int  end_of_wave(void) { return 0; }
float func_800c9158(unsigned int *px) { return (float)(*px >> 16); }	/* depth word -> distance (placeholder scale) */
int  D_801e6e70, D_8018db90;
script_word_t hhouse_script[1], freakshow_script[1], rickity_town_script[1], bigtop_script[1];	/* level scripts: not extracted yet */
int  func_800d4990(void) { return 0; }		/* I/O ASIC bit test */
int  attract_high_scores(int arg) { (void)arg; return 0; }	/* high-score reset screen */
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
void dispatch_diags_proc(int closed, int swid) { (void)closed; (void)swid; fprintf(stderr, "[stub] test switch: diagnostics not implemented\n"); }

/* the triangle setup (0x800d1750) writes Voodoo commands; here it is a plain draw */
void draw_tri(MidVertex *a, MidVertex *b, MidVertex *c) { grDrawTriangleDma(a, b, c, 0); }
/* 0x800d1d8c / 0x800d2664: the same with iterated RGB / alpha vertices (iteration ignored for now) */
void draw_tri_rgb(void *a, void *b, void *c) { grDrawTriangleDma(a, b, c, 0); }
void draw_tri_alpha(void *a, void *b, void *c) { grDrawTriangleDma(a, b, c, 0); }
/* 0x800d2174: a constant-colour triangle (the original sorts the vertices by y) */
void draw_tri_flat(void *a, void *b, void *c) { grDrawTriangleDma(a, b, c, 0); }

/* ---- the per-frame passes (objects.c: draw_pass 0x800cbbb4, think_pass 0x800cca70) call
   into level code the port does not have yet; these keep them no-ops in the attract ---- */
void draw_pass(void);
void think_pass(void *);
int  get_free_mem_left(void) { return 0x400000; }			/* GOOSE 0x80143e90 */
float fsqrt(float x) { return sqrtf(x); }				/* 0x80144900: sqrt.s */
void func_800d41b4(int f) { (void)f; }					/* the level's 3D draws */
int *show_debug_light_(int f) { (void)f; return 0; }
void func_800e9140(int f) { (void)f; }					/* enemy stats */
void func_800d6e94(void) { }
void func_800ff418(void) { }
player_stats_t *something_to_do_with_shooting_(player_stats_t *p) { return p; }
void func_800ce2f8(float *a, float b, float *c) { (void)a; (void)b; c[0] = c[1] = c[2] = 0.0f; }
void func_800fe580(void) { }
void func_800fef18(void) { }
void func_800ffbdc(void) { }
void func_800fe1fc(void) { }
void func_800dbb80(void) { }
void func_80100558(void) { }
void func_800e2fe4(void) { }
void func_800db4b8(void) { }
void func_800c83bc(void) { }
void func_800c9620(int a) { (void)a; }
void func_800c9bf0(void) { }
int	D_801e5cbc, shown_frame, wave_id_g2, RTP_val, DMA_val, D_801e0ecc, please_wait_timer;
extern unsigned int background_color;

/* game_init (0x800ca3a0, scene.c) is real; the set-up calls it makes that the port lacks */
void func_800c836c(void) { }
void func_800c81b8(void) { }
void func_800c865c(void) { }
void func_800d6768(void)			/* 0x800d6768: the texmap surface table */
{
	int	i;
	for(i = 0; i < MAX_SURFACES; i++) { surfaces[i].key = -1; surfaces[i].refs = 0; surfaces[i].index = -1; }
}
void func_800d78fc(void) { }
void func_800c8a44(float x, float y, float w, float r, unsigned int c) { (void)x; (void)y; (void)w; (void)r; (void)c; }	/* a screen marker */
void func_800cfda8(float *m, float *a) { (void)m; (void)a; }	/* rotation matrix from angles */
/* load_level_assets (0x800ca448, scene.c) is real; its loaders are not */
void func_800db148(void) { }
void func_800db800(void) { }
void load_misc_texmaps(void) { fprintf(stderr, "[stub] load_misc_texmaps (0x800d8c60): level texmaps not loaded\n"); }
void func_800ff0b0(void) { }
void func_800fe440(void) { }
void func_800fedd8(void) { }
void func_800ffb08(void) { }
void func_80100478(void) { }
void func_8013f6d8(void) { }

/* the boot callees the port does not have yet: level assets, models, the gun calibration */
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

/* 0x800d49c4: a switch-page test used by the attract's unused credits step; 0x801410a4
   load_credits reloads the pricing. Neither runs in the attract loop as shipped. */
int func_800d49c4(void) { return 0; }
void load_credits(void) { fprintf(stderr, "[stub] load_credits (0x801410a4)\n"); }
