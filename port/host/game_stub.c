/*
 * game_stub.c - stand-ins for game-side code the port does not have in C yet, plus the port's
 * main_proc: it runs the game's real attract process (src/game/attract.c) on top of the
 * decompiled object/texmap modules. Each stub disappears as its function is decompiled.
 */
#include <stdio.h>
#include "../../src/game/camera.h"		/* gun_t, guns[] */
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <goose/process.h>
#include <goose/object.h>
#include <glide/glide.h>
#include "hal.h"
#define STUB(n)	do { static int seen; if(!seen) { seen = 1; stub_reached(#n); } } while(0)
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
int  D_801facc8;
extern int D_80179d50;			/* camera.c: guns sampled every vblank */
extern int D_80212e60;
/* 0x800c6030: the game's own vertical-retrace handler (it does not call the library's) */
extern void read_guns(void), gun_vblank(void);
int  vblank_handler(void)
{
	tick_counter++;
	got_interrupt++;
	if(!(tick_counter % gfx_scale))
	{
		uptime++;
		if(D_80212e60 != 1) D_801facc8++;
	}
	if(D_80179d50 == 1) { read_guns(); gun_vblank(); }
	scan_switches();
	return 1;
}
void game_dma0_handler(void) { }

/* ---- attract-mode callees not decompiled yet ---- */
void func_800e74f0(void) { movie_start(); }	/* movie play start */
void func_800e7568(void) { movie_stop(); }	/* movie play stop */
/* script callees and level data the attract movies touch */
void level_end_stats_wrapper(void) { STUB(level_end_stats_wrapper); }
int end_of_wave(void) { STUB(end_of_wave); return 0; }
int attract_high_scores(int arg) { STUB(attract_high_scores); (void)arg; return 0; }	/* high-score reset screen */
float get_screen_pos(int pos, float *out) { STUB(get_screen_pos); (void)pos; out[0] = out[1] = 0.0f; return -1.0f; }	/* 3D point -> screen: no camera yet */
void graphical_printf(char *buf, char *fmt, ...) { va_list ap; va_start(ap, fmt); vsprintf(buf, fmt, ap); va_end(ap); }
char attract_text1[136], attract_text2[136], attract_text3[136];	/* 0x8021eed0.. filled by something_todo_with_credits */
int attract_text_state;
/* the game proper: not decompiled yet */
void dispatch_diags_proc(int closed, int swid) { STUB(dispatch_diags_proc); (void)closed; (void)swid; }

/* the triangle setup (0x800d1750) writes Voodoo commands; here it is a plain draw */
void draw_tri(MidVertex *a, MidVertex *b, MidVertex *c) { grDrawTriangleDma(a, b, c, 0); }
/* the game's own triangle set-up routines write the Voodoo's triangle registers from its vertex
   layouts; the port hands each layout to the renderer with the colour and alpha positions:
   0x800d1d8c / 0x800d3834: x, y, q, s/w, t/w, r, g, b (model.c tvert_t)
   0x800d2664: x, y, q, s/w, t/w, a (text.c MidVertexWithTA)
   0x800d2174: a constant-colour triangle (the original sorts the vertices by y)
   0x800d1a38: x, y, q, r, g, b (fvert_t, no texture)
   0x800d299c / 0x800d3bf0: x, y, q, s/w, t/w, a, r, g, b (avert_t) */
void port_draw_tri(const float *, const float *, const float *, int, int, int);
void draw_tri_rgb(void *a, void *b, void *c) { port_draw_tri(a, b, c, 1, 5, -1); }
void draw_tri_alpha(void *a, void *b, void *c) { port_draw_tri(a, b, c, 1, -1, 5); }
void draw_tri_flat(void *a, void *b, void *c) { grDrawTriangleDma(a, b, c, 0); }
void func_800d1a38(void *a, void *b, void *c) { port_draw_tri(a, b, c, 0, 3, -1); }
void func_800d299c(void *a, void *b, void *c) { port_draw_tri(a, b, c, 1, 6, 5); }
void func_800d3834(void *a, void *b, void *c) { port_draw_tri(a, b, c, 1, 5, -1); }
void func_800d3bf0(void *a, void *b, void *c) { port_draw_tri(a, b, c, 1, 6, 5); }
/* 0x800d326c: x, y, q, s/w, t/w with the depth gradients (the billboards, draw2d.c) */
void func_800d326c(void *a, void *b, void *c) { port_draw_tri(a, b, c, 1, -1, -1); }

/* 0x800d3528: x, y, q, s/w, t/w, a with the depth gradients (the clipper, clip.c) */
void func_800d3528(void *a, void *b, void *c) { port_draw_tri(a, b, c, 1, -1, 5); }

/* ---- the per-frame passes (objects.c: draw_pass 0x800cbbb4, think_pass 0x800cca70) call
   into level code the port does not have yet; these keep them no-ops in the attract ---- */
void draw_pass(void);
void think_pass(void *);
int get_free_mem_left(void) { return 0x400000; }			/* GOOSE 0x80143e90 */
float fsqrt(float x) { return sqrtf(x); }				/* 0x80144900: sqrt.s */
void func_800ff418(void) { STUB(func_800ff418); }
void func_800ce2f8(float *a, float b, float *c) { STUB(func_800ce2f8); (void)a; (void)b; c[0] = c[1] = c[2] = 0.0f; }
void func_800fe580(void) { STUB(func_800fe580); }
void func_800fef18(void) { STUB(func_800fef18); }
void func_800ffbdc(void) { STUB(func_800ffbdc); }
void func_800fe1fc(void) { STUB(func_800fe1fc); }
void func_80100558(void) { STUB(func_80100558); }
void func_800e2fe4(void) { STUB(func_800e2fe4); }
void func_800c83bc(void) { STUB(func_800c83bc); }
void func_800c9620(int a) { STUB(func_800c9620); (void)a; }
void func_800c9bf0(void) { STUB(func_800c9bf0); }
int	shown_frame, RTP_val, DMA_val, D_801e0ecc, please_wait_timer;
extern unsigned int background_color;

/* the spawn table's per-type constructor (0x800e9570) and the levels' spawn lists */
#include "../../src/game/spawn.h"
spawn_t hhouse_spawn_blob[1], freakshow_spawn_blob[1], rickity_spawn_blob[1], bigtop_spawn_blob[1];
int D_801e0f0c;
/* game_init (0x800ca3a0, scene.c) is real; the set-up calls it makes that the port lacks */
void init_surfaces(void)			/* 0x800d6768: the texmap surface table */
{
	int	i;
	for(i = 0; i < MAX_SURFACES; i++) { surfaces[i].key = -1; surfaces[i].refs = 0; surfaces[i].index = -1; }
}
void func_800c8a44(float x, float y, float w, float r, unsigned int c) { STUB(func_800c8a44); (void)x; (void)y; (void)w; (void)r; (void)c; }	/* a screen marker */
/* load_level_assets (0x800ca448, scene.c) is real; its loaders are not */
void func_800ff0b0(void) { STUB(func_800ff0b0); }
void func_800fe440(void) { STUB(func_800fe440); }
void func_800fedd8(void) { STUB(func_800fedd8); }
void func_800ffb08(void) { STUB(func_800ffb08); }
void func_80100478(void) { STUB(func_80100478); }
void func_8013f6d8(void) { STUB(func_8013f6d8); }

/* the boot callees the port does not have yet: level assets, models, the gun calibration */
#define LOAD_STUB(n) void n(void) { STUB(n); }
LOAD_STUB(load_jester_texmap) LOAD_STUB(load_jester_model) LOAD_STUB(load_inng_texmap) LOAD_STUB(load_chnk_texmap)
LOAD_STUB(load_chnk_model) LOAD_STUB(load_shotgun_texmap) LOAD_STUB(load_health_texmap) LOAD_STUB(load_ammmo_texmap)
LOAD_STUB(load_machinegun_texmap) LOAD_STUB(load_flamethrower_texmap) LOAD_STUB(load_acid_texmap) LOAD_STUB(load_shotgun_model)
LOAD_STUB(load_health_model) LOAD_STUB(load_ammo_model) LOAD_STUB(load_machinegun_model) LOAD_STUB(load_flamethrower_model)
LOAD_STUB(load_acid_model)
extern int randrng(int);
float func_800fe8dc(float range) { return (float)(randrng(10000) - 5000) / (5000.0f / range); }	/* 0x800fe8dc: a random float in +-range */
void func_800ffa20(float *pos) { STUB(func_800ffa20); (void)pos; }					/* 0x800ffa20: drawn around the camera when camera_follow is set */
void debug_move_camera(void) { STUB(debug_move_camera); }						/* 0x800c93c4: the player 3/4 switches move the camera (debug) */
void load_backup_file(char *name) { STUB(load_backup_file); }	/* 0x800caf48 */
void startaudit(FILE *f) { STUB(startaudit); (void)f; }					/* 0x800ef870: the audit names for audits.fmt */
void game_proc2(void) { STUB(game_proc2); for(;;) sleep(1); }
void ide_set_partition(int n) { (void)n; }				/* GOOSE IDE: the HAL maps partition 1 itself */
int gfx_scale_from_refresh(void) { return 54; }				/* 0x80157004: frames per second of the display mode */

/* 0x801410a4 load_credits reloads the pricing (after a won game) */
void load_credits(void) { STUB(load_credits); }

/* ---- the level flow (main_game_proc, wave.c): what the port still lacks ---- */
int  D_80212e10, D_801e0e24, D_801e70a8, D_801a25c8, D_80212da4, D_801e5cd4, D_801e0f0c_pad;
int  something_with_reload_p1, something_with_reload_p2, hhouse_texmap_set;
void func_80162a10(int *size, int *avail, int *used, int *frag) { *size = *avail = *used = *frag = 0; }	/* texture memory statistics */
void func_80163eec(int n) { (void)n; }
void func_80102690(void) { STUB(func_80102690); }			/* 0x80102690: three select-screen words cleared */
void func_80102500(player_stats_t *p) { STUB(func_80102500); (void)p; }	/* the player's panel on the select screen */
void something_player_score(player_stats_t *p) { STUB(something_player_score); (void)p; }	/* 0x800e5560: high-score entry */
void L_80101908(game_object_t *o) { STUB(L_80101908); (void)o; }			/* the level-select targets */
int func_801020b4(game_object_t *o) { STUB(func_801020b4); (void)o; return 1; }
void func_80102454(game_object_t *o) { STUB(func_80102454); (void)o; }
void func_800fe3d8(void) { STUB(func_800fe3d8); }
void func_80109260(void *o, int n) { STUB(func_80109260); (void)o; (void)n; }
void func_80107e30(game_object_t *o) { STUB(func_80107e30); (void)o; }			/* the loading jester (object 0x2014) */
int func_80108ab0(game_object_t *o) { STUB(func_80108ab0); (void)o; return 1; }
void func_80108c94(game_object_t *o) { STUB(func_80108c94); (void)o; }

/* the mouse's game-screen position as the raw gun reading the game's read_guns turns back
   into that position through the player's calibration (camera.c guns[]) */
void port_gun_raw(int player, float sx, float sy, int *rx, int *ry)
{
	extern gun_t guns[2];
	gun_t *g = &guns[player];
	if(g->scale_x == 0.0f || g->scale_y == 0.0f) { *rx = 128 << 4; *ry = 128 << 4; return; }
	*rx = (int)(sx * g->scale_x + g->off_x + 0.5f);
	*ry = (int)(sy * g->scale_y + g->off_y + 0.5f);		/* the game's screen_y = 256 - (raw - off) / scale runs top-down */
	if(*rx < 0) *rx = 0; if(*rx > 0xfff) *rx = 0xfff;
	if(*ry < 0) *ry = 0; if(*ry > 0x3ff) *ry = 0x3ff;
}
