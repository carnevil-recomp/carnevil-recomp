/*
 * main.c - CarnEvil's main process (0x800c50ac, created by GOOSE main() as "main") and the
 * uptime audit process (0x800c4f30). Boot order: game init, PIC serial, the hardware info
 * screen, CMOS and audits, high scores, adjustments, credits, sound, switch handlers, the
 * level assets, the gun reset, then the attract process (which clears the info screen).
 */
#include <stdio.h>
#include "credits.h"
#include "soundio.h"
#include "waves.h"

extern void	*qcreate(char *, int, void (*)(), int, int, int, int);
extern void	sleep(int);
extern void	ide_set_partition(int);
extern void	set_bgnd_color(unsigned int);
extern void	game_init(void);					/* 0x800ca3a0 */
extern void	read_pic_serial(int *);					/* 0x800c6480 */
extern void	hw_info_screen(void);					/* 0x800c65d4 */
extern int	game_setup_cmos(void);				/* 0x800f2a28 (setup_cmos is the SDK template's name) */
extern int	check_adjustments(void);				/* GOOSE (write_adjustments_file_wrapper) */
extern void	clear_all_audits(void), restore_game_adjustments(void);	/* 0x800f1a20, 0x800f1fa8 */
extern int	check_all_audits(void), load_game_adjustments(void);	/* 0x800f1bac, 0x800f1d88 */
extern void	set_percent_audit(int, int, int);			/* 0x800f1cd4 */
extern void	load_high_scores(void);					/* 0x800e5014 */
extern void	reset_high_scores(void);				/* 0x800e4eac */
extern int	get_audit(int, int *), set_audit(int, int), get_adjustment(int, int *), set_adjustment(int, int);
extern int	snd_master_volume(int);
extern int	gfx_scale_from_refresh(void);				/* 0x80157004 */
extern void	set_frame_factor(int), main_init_sound(void);
extern void	*set_dcsw_handler(int, void (*)(int, int));
#include "objects.h"
extern void	delete_all_sprites(void), delete_all_background_sprites(void);	/* 0x8014c65c, 0x8014849c */
extern void	load_level_assets(void);				/* 0x800ca448 */
extern void	load_jester_texmap(void), load_jester_model(void), load_inng_texmap(void), load_chnk_texmap(void), load_chnk_model(void);
extern void	load_shotgun_texmap(void), load_health_texmap(void), load_ammo_texmap(void), load_machinegun_texmap(void);
extern void	load_flamethrower_texmap(void), load_acid_texmap(void), load_shotgun_model(void), load_health_model(void);
extern void	load_ammo_model(void), load_machinegun_model(void), load_flamethrower_model(void), load_acid_model(void);
extern int	kss_reset_(void), kss_reset_w_calib_(void);
extern void	game_proc(void), game_proc2(void), main_game_proc(void);
extern void	dispatch_diags_proc(int, int);
extern int	pic_serial[6];						/* 0x801e7030: serial number, game id, ... */
#define game_id		pic_serial[1]
extern int	snd_opsys_ver, gfx_scale, uptime, pricing_from_dips;
extern float	D_80212df4;

int	cmos_status;					/* 1 restored, 2 bad, 3 ok */
int	coinage_source;					/* 0x801e096c: 1 CMOS, 2 dipswitch */
int	gun_status;					/* 0x80237404 */
int	D_801facc8;					/* seconds since boot while not in a game */
int	D_801fad10;					/* booting */
extern int	attract_cycles;		/* 0x80212d98 */
void	*ccproc_node;					/* 0x801e5da8 */

/* 0x800c4f30: uptime and play-time audits, every 300 frames */
void utime_proc(void)
{
	int	last, delta;
	int	total, part;

	if(get_audit(7, &uptime))
	{
		uptime = 0;
	}
	last = uptime;
	for(;;)
	{
		sleep(300);
		delta = uptime - last;
		last = uptime;
		set_audit(7, last);
		if((player_stats[0].state & 0x1d) || (player_stats[1].state & 0x1d))
		{
			get_audit(8, &total);
			if(part >= 0)
			{
				total += delta;
				set_audit(8, total);
			}
			else
			{
				total = 0;
				set_audit(8, 0);
			}
			set_percent_audit(8, 7, 9);
			if((player_stats[0].state & 0x1d) && (player_stats[1].state & 0x1d))
			{
				get_audit(0x28, &part);
				if(part >= 0)
				{
					part += delta;
					set_audit(0x28, part);
				}
				set_percent_audit(0x28, 8, 0x2a);
			}
			else
			{
				get_audit(0x27, &part);
				if(part >= 0)
				{
					part += delta;
					set_audit(0x27, part);
				}
				set_percent_audit(0x27, 8, 0x29);
			}
		}
	}
}

/* 0x800c50ac */
void main_proc(void)
{
	short	hw_obj;
	int	restored;
	int	val;
	int	n;

	restored = 0;
	D_801facc8 = 0;
	cmos_status = 0;
	coinage_source = 0;
	gun_status = 0;
	snd_opsys_ver = 0;
	ide_set_partition(1);
	set_bgnd_color(0xff000000);
	D_801fad10 = 1;
	game_init();
	read_pic_serial(pic_serial);
	hw_obj = add_object1_wrapper(0x20, (void (*)(game_object_t *))hw_info_screen, 0, 0);
	if(game_id != 0x210 && game_id != 0x1d5 && game_id != 0x1e6)
	{
		for(;;)
		{
			sleep(1);
		}
	}
	if(game_setup_cmos() == 0)
	{
		if(get_audit(0xb5, &val) || val != 0xb5)
		{
			clear_all_audits();
		}
		set_audit(0xb5, 0xb5);
		if((n = check_adjustments()) != 0)
		{
			printf("Errors in check_adjustments %d\n", n);
			restore_game_adjustments();
			restored = 1;
			clear_all_audits();
			set_adjustment(0x52, 9999);
			if(check_adjustments())
			{
				cmos_status = 2;
			}
			else
			{
				cmos_status = 1;
			}
			set_adjustment(0x2e, 0);
		}
		if(get_adjustment(0x2e, &val))
		{
			cmos_status = 1;
			printf("Adjustment failure %d\n", val);
			restored = 1;
			restore_game_adjustments();
			clear_all_audits();
			set_adjustment(0x52, 8888);
			set_adjustment(0x2e, 0);
		}
		else if(val)
		{
			cmos_status = 1;
			printf("Factory restore adjustment %d\n", val);
			restore_game_adjustments();
			clear_all_audits();
			restored = 1;
			set_adjustment(0x52, 8888);
			set_adjustment(0x2e, 0);
		}
		if(!check_all_audits() || check_adjustments())
		{
			cmos_status = 2;
		}
		if(cmos_status != 2)
		{
			check_all_audits();
			qcreate("utime", 0x8000, utime_proc, 0, 0, 0, 0);
		}
	}
	else
	{
		fprintf(stderr, "Error occured in trying to INIT. cmos err num %d\r\n", game_setup_cmos());
		cmos_status = 2;
	}
	if(cmos_status == 0)
	{
		cmos_status = 3;
		load_high_scores();
	}
	else
	{
		reset_high_scores();
	}
	if(get_adjustment(0x38, &val) || val)
	{
		set_adjustment(0x38, 0);
		reset_high_scores();
	}
	if(get_adjustment(0x2f, &val) || val)
	{
		set_adjustment(0x2f, 0);
		reset_high_scores();
	}
#ifdef PORT_HOST
	{ extern void port_apply_adjustments(void); port_apply_adjustments(); }	/* --adjust / --violence */
#endif
	load_game_adjustments();
	if(restored)
	{
		if(game_id == 0x1e6)
		{
			D_80212df4 += 0.1f;
			set_adjustment(0x37, 0x46);
		}
		sound_state.volume = 200;
		set_adjustment(3, 200);
		snd_master_volume(sound_state.volume);
	}
	init_credits();
	if(cmos_status == 2 || pricing_from_dips)
	{
		coinage_source = 2;
	}
	else
	{
		coinage_source = 1;
	}
	gfx_scale = gfx_scale_from_refresh();
	set_frame_factor(3);
	main_init_sound();
	set_dcsw_handler(0, coin_handler);
	set_dcsw_handler(1, coin_handler);
	set_dcsw_handler(7, coin_handler);
	set_dcsw_handler(8, coin_handler);
	set_dcsw_handler(0xf, coin_handler);
	set_dcsw_handler(6, service_credit_handler);
	set_dcsw_handler(3, dispatch_slam_proc);
	set_dcsw_handler(0xb, volume_switch_handler);
	set_dcsw_handler(0xc, volume_switch_handler);
	set_dcsw_handler(4, dispatch_diags_proc);
	qcreate("volume", 0x8201, coin_volume_proc, 0, 0, 0, 0);
	ccproc_node = qcreate("ccproc", 0x8200, ccproc_proc, 0, 0, 0, 0);
	delete_all_sprites();
	load_level_assets();
	load_jester_texmap();
	load_jester_model();
	load_inng_texmap();
	load_chnk_texmap();
	load_chnk_model();
	load_shotgun_texmap();
	load_health_texmap();
	load_ammo_texmap();
	load_machinegun_texmap();
	load_flamethrower_texmap();
	load_acid_texmap();
	load_shotgun_model();
	load_health_model();
	load_ammo_model();
	load_machinegun_model();
	load_flamethrower_model();
	load_acid_model();
	D_801fad10 = 0;
	sleep(1);
	remove_game_object(hw_obj);
	if(cmos_status == 2)
	{
		gun_status = kss_reset_();
	}
	else
	{
		gun_status = kss_reset_w_calib_();
	}
	hw_obj = add_object1_wrapper(0x20, (void (*)(game_object_t *))hw_info_screen, 0, 0);
	sleep(20);
	attract_cycles = 0;
	set_dcsw_handler(2, dispatch_start_proc);
	set_dcsw_handler(5, dispatch_start_proc);
	if(debug_mode & DEBUG_ON)
	{
		remove_game_object(hw_obj);
		qcreate("attract", 0x100, game_proc2, 0, 0, 0, 0);
	}
	else
	{
		qcreate("attract", 0x100, game_proc, 0, 0, 0, 0);
	}
}

/* ---- the game proper: the main game process and the level select (0x800c5748-0x800c5e50) ---- */
#include <glide/glide.h>
#include "movie.h"
#include "levelsnd.h"
extern void	enable_guns(void), normal_screen(void), delete_process_id(int);
extern void	remove_all_objects(void), preload_wave(int), unload_wave(void), load_credits(void);
extern void	fade(float, int, int), script_level_end_stats(void), graphical_printf(char *, char *, ...);
extern void	draw_string_centered(int, char *, float, float, unsigned int);
extern void	enable_accuracy_tracking(void), disable_accuracy_tracking(void), reset_select_lights(void), set_weapon(player_stats_t *, int);
extern void	set_level_start_time(player_stats_t *), aim_select_light(player_stats_t *), check_high_score(player_stats_t *);
extern void	draw_select_target(game_object_t *), select_target_think(game_object_t *);	/* select.c: the level-select targets (objects 0x710-0x713) */
extern int	init_select_target(game_object_t *);
extern int	increment_audit(int);
extern void	clear_bonus_count(void);				/* 0x800c7c78 (credits.c) */
extern void	average_audit(int count_audit, int average_audit, int value);	/* 0x800f24c8 */
extern void	print_select_countdown(game_object_t *);
extern int	wave_busy, please_wait_timer, attract_cycles;
extern int	D_80212e10, D_80212e24, D_80212e28, D_80212e2c, D_801e0e24, D_801e70a8;
int		levels_cleared;				/* 0x80212e34: bit 0 Rickety Town, 1 haunted house, 2 freak show, 3 big top */
int		next_wave;				/* 0x801fac6c: the level picked on the select screen, -1 while none */
int		level_start_counter;			/* 0x80237400: the select screen's countdown */
int		select_blink = 0;			/* 0x80177be4 (.data) */
extern char	text_buffer[];
int		game_start_time;				/* 0x801e09e8: D_801facc8 when the game started */

/* 0x800c5748: a game starts (from the credits' start process): the main game process
   replaces the attract one. A player already joining keeps their place. */
void init_main_game_proc(int player)
{
#ifdef PORT_HOST
	{ extern int port_trace; if(port_trace) fprintf(stderr, "[game] start by player %d, phase %#x\n", player, game_phase); }
#endif
	enable_guns();
	delete_all_sprites();
	delete_all_background_sprites();
	game_phase = 0x10;
	game_state[1] = 1;
	D_80212e10 = 0;
	D_80212e24 = 1;
	D_80212e2c = 0;
	D_80212e28 = 0;
	levels_cleared = 0;
	game_start_time = D_801facc8;
	normal_screen();
	if(player_stats[0].state == 0x20)
	{
		player_stats[0].state = 0;
	}
	else
	{
		remove_all_objects();
		delete_process_id(0x100);
		stop_level_cue_and_stream();
		set_frame_factor(3);
	}
	qcreate("game", 0, main_game_proc, 0, 0, 0, 0);
}

/* 0x800c63c8: the level select's countdown (an object callback), in a pulsing red, and the
   players' panels */
void print_select_countdown(game_object_t *obj)
{
	int	blink;

	blink = select_blink + 0x11;
	select_blink = blink;
	if(level_start_counter < 200)
	{
		graphical_printf(text_buffer, "%d", level_start_counter / 10);
		draw_string_centered(1, text_buffer, 200.0f, 180.0f, (blink & 0xff) << 16);
	}
	aim_select_light(&player_stats[0]);
	aim_select_light(&player_stats[1]);
}

/* 0x800c5830: the main game process: the level-select gallery (a shot target or the countdown
   picks the next level), the level itself, and when all four are cleared or both players are
   out, the high-score entry and back to the attract */
void main_game_proc(void)
{
	int	won;

#ifdef PORT_HOST
	{ extern int port_trace; if(port_trace) fprintf(stderr, "[game] main game process\n"); }
#endif
	grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
	grDepthBufferFunction(GR_CMP_LEQUAL);
	player_stats[0].score = 0;
	player_stats[1].score = 0;
	player_stats[0].clicks = 0;
	player_stats[1].clicks = 0;
	disable_accuracy_tracking();
	while(wave_busy == 1)
	{
		sleep(1);
	}
	game_phase = 0x20;
	movie_stop();
	sleep(2);
	for(;;)
	{
		level_start_counter = 400;
		if(game_phase == 0x400)
		{
			/* back from a level: the gallery is loaded again. The first time round the
			   attract wave is still loaded (it holds the gallery and the "select1" bank),
			   and unloading it before the first level frees the attract's textures and
			   sound banks (remove_level_sounds' fall-through) */
			game_phase = 0x20;
			preload_wave(WAVE_LEVEL_SELECT);
		}
		next_wave = -1;
		play_level_cue(2, "select1");
		movie_load(8);
		movie_start();
		reset_select_lights();
		enable_guns();
		set_weapon(&player_stats[0], 1);
		set_weapon(&player_stats[1], 1);
		sleep(2);
		normal_screen();
		sleep(5);
		/* a target for each level not cleared yet (end_of_wave: Rickety Town 1, Haunted House 2,
		   Freak Show 4); 0x710 is the haunted house, 0x711 the freak show, 0x712 the Ferris wheel */
		if(!(levels_cleared & 2))
		{
			add_object1_wrapper(0x710, draw_select_target, init_select_target, select_target_think);
		}
		if(!(levels_cleared & 4))
		{
			add_object1_wrapper(0x711, draw_select_target, init_select_target, select_target_think);
		}
		if(!(levels_cleared & 1))
		{
			add_object1_wrapper(0x712, draw_select_target, init_select_target, select_target_think);
		}
		add_object1_wrapper(0x20, print_select_countdown, 0, 0);
		if(levels_cleared == 7)
		{
			sleep(20);
			add_object1_wrapper(0x713, draw_select_target, init_select_target, select_target_think);
		}
		if(next_wave == -1)
		{
			do
			{
				sleep(1);
				level_start_counter--;
				if(levels_cleared == 7 && level_start_counter < 340)
				{
					next_wave = WAVE_BIGTOP;
				}
				if(level_start_counter <= 0)
				{
					if(!(levels_cleared & 1))
					{
						next_wave = WAVE_RICKITY;
					}
					else if(!(levels_cleared & 4))
					{
						next_wave = WAVE_FREAKSHOW;
					}
					else if(!(levels_cleared & 2))
					{
						next_wave = WAVE_HHOUSE;
					}
					else
					{
						next_wave = WAVE_BIGTOP;
					}
				}
			} while(next_wave == -1);
		}
		remove_all_objects();
		clean_up_level_sounds();
		movie_stop();
		sleep(2);
		unload_wave();
		game_phase = 0x80;
		D_801e0e24 = D_801facc8;
		please_wait_timer = 0;
		switch(next_wave)
		{
		case WAVE_FREAKSHOW:
			increment_audit(0x3a);
			movie_load(2);
			preload_wave(WAVE_FREAKSHOW);
			break;
		case WAVE_HHOUSE:
			increment_audit(0x33);
			movie_load(1);
			preload_wave(WAVE_HHOUSE);
			break;
		case WAVE_RICKITY:
			increment_audit(0x40);
			movie_load(5);
			preload_wave(WAVE_RICKITY);
			break;
		case WAVE_BIGTOP:
			increment_audit(0x47);
			movie_load(7);
			preload_wave(WAVE_BIGTOP);
			break;
		default:
			printf("Starting game with unknown wave!!!!\n");
			break;
		}
		movie_start();
		please_wait_timer = 0;
		game_phase = 2;
		enable_accuracy_tracking();
		if(player_stats[0].state)
		{
			set_level_start_time(&player_stats[0]);
		}
		if(player_stats[1].state)
		{
			set_level_start_time(&player_stats[1]);
		}
		/* the level runs until every player is out or the level's end changes the phase (the
		   test of game_phase is made on every pass, 0x800c5cd0) */
		while((player_stats[0].state || player_stats[1].state) && game_phase == 2)
		{
			sleep(1);
		}
		won = 0;
		if(!player_stats[0].state && !player_stats[1].state)
		{
			break;
		}
		if(levels_cleared == 0xf)
		{
			won = 1;
			D_801e70a8 = D_801facc8;
			average_audit(0x69, 0x68, D_801facc8 - game_start_time);
			break;
		}
	}
	if(won)
	{
		script_level_end_stats();
		sleep(2);
		fade(1.0f, 15, 0);
		sleep(140);
	}
	enable_guns();
	game_phase = 8;
	player_stats[0].state = 0;
	player_stats[1].state = 0;
	remove_all_objects();
	clear_bonus_count();
	fade(0.0f, 15, 0);
	preload_wave(WAVE_HISCORE_ENTRY);
	sleep(17);
	check_high_score(&player_stats[0]);
	check_high_score(&player_stats[1]);
	unload_wave();
	if(won)
	{
		load_credits();
	}
	attract_cycles = 0;
	qcreate("game", 0x100, game_proc, 0, 0, 0, 0);
}
