/*
 * main.c - CarnEvil's main process (0x800c50ac, created by GOOSE main() as "main") and the
 * uptime audit process (0x800c4f30). Boot order: game init, PIC serial, the hardware info
 * screen, CMOS and audits, high scores, adjustments, credits, sound, switch handlers, the
 * level assets, the gun reset, then the attract process (which clears the info screen).
 */
#include <stdio.h>
#include "credits.h"

extern void	*qcreate(char *, int, void (*)(), int, int, int, int);
extern void	sleep(int);
extern void	ide_set_partition(int);
extern void	set_bgnd_color(unsigned int);
extern void	game_init(void);					/* 0x800ca3a0 */
extern void	read_pic_serial(int *);					/* 0x800c6480 */
extern void	hw_info_screen(void);					/* 0x800c65d4 */
extern int	setup_cmos(void);					/* 0x800f2a28 */
extern int	check_adjustments(void);				/* GOOSE (write_adjustments_file_wrapper) */
extern void	clear_all_audits(void), restore_game_adjustments(void);	/* 0x800f1a20, 0x800f1fa8 */
extern int	check_all_audits(void), load_game_adjustments(void);	/* 0x800f1bac, 0x800f1d88 */
extern void	set_percent_audit(int, int, int);			/* 0x800f1cd4 */
extern void	load_high_scores(void);					/* 0x800e5014 */
extern int	reset_high_scores(void);				/* 0x800e4eac */
extern int	get_audit(int, int *), set_audit(int, int), get_adjustment(int, int *), set_adjustment(int, int);
extern int	snd_master_volume(int);
extern int	gfx_scale_from_refresh(void);				/* 0x80157004 */
extern void	set_frame_factor(int), main_init_sound(void);
extern void	*set_dcsw_handler(int, void (*)(int, int));
extern short	add_object1_wrapper(int, void (*)(), int, int);
extern void	remove_game_object(short);
extern void	delete_all_sprites(void);				/* 0x8014c65c */
extern void	load_level_assets(void);				/* 0x800ca448 */
extern void	load_jester_texmap(void), load_jester_model(void), load_inng_texmap(void), load_chnk_texmap(void), load_chnk_model(void);
extern void	load_shotgun_texmap(void), load_health_texmap(void), load_ammmo_texmap(void), load_machinegun_texmap(void);
extern void	load_flamethrower_texmap(void), load_acid_texmap(void), load_shotgun_model(void), load_health_model(void);
extern void	load_ammo_model(void), load_machinegun_model(void), load_flamethrower_model(void), load_acid_model(void);
extern int	kss_reset_(void), kss_reset_w_calib_(void);
extern void	game_proc(void), game_proc2(void);
extern void	dispatch_diags_proc(int, int);
extern int	pic_serial[6];						/* 0x801e7030: serial number, game id, ... */
#define game_id		pic_serial[1]
extern int	snd_opsys_ver, volume, debug_mode, gfx_scale, uptime, pricing_from_dips;
extern float	D_80212df4;
extern FILE	*__dj_stderr;

int	cmos_status;					/* 1 restored, 2 bad, 3 ok */
int	coinage_source;					/* 0x801e096c: 1 CMOS, 2 dipswitch */
int	gun_status;					/* 0x80237404 */
int	D_801facc8;					/* seconds since boot while not in a game */
int	D_801fad10;					/* booting */
int	D_80212d98;
void	*ccproc_node;					/* 0x801e5da8 */

/* 0x800c4f30: uptime and play-time audits, every 300 frames */
void utime_proc(void)
{
	int	last, delta;
	int	total, part;

	part = 0;
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
			if(part < 0)
			{
				total = 0;
				set_audit(8, 0);
			}
			else
			{
				total += delta;
				set_audit(8, total);
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
	hw_obj = add_object1_wrapper(0x20, hw_info_screen, 0, 0);
	if(game_id != 0x210 && game_id != 0x1d5 && game_id != 0x1e6)
	{
		for(;;)
		{
			sleep(1);
		}
	}
	if(setup_cmos() == 0)
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
			restore_game_adjustments();
			clear_all_audits();
			restored = 1;
			set_adjustment(0x52, 8888);
			set_adjustment(0x2e, 0);
		}
		else if(val)
		{
			cmos_status = 1;
			printf("Factory restore adjustment %d\n", val);
			restored = 1;
			restore_game_adjustments();
			clear_all_audits();
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
		fprintf(__dj_stderr, "Error occured in trying to INIT. cmos err num %d\r\n", setup_cmos());
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
	load_game_adjustments();
	if(restored)
	{
		if(game_id == 0x1e6)
		{
			D_80212df4 += 0.1f;
			set_adjustment(0x37, 0x46);
		}
		volume = 200;
		set_adjustment(3, 200);
		snd_master_volume(volume);
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
	load_ammmo_texmap();
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
	hw_obj = add_object1_wrapper(0x20, hw_info_screen, 0, 0);
	sleep(20);
	D_80212d98 = 0;
	set_dcsw_handler(2, dispatch_start_proc);
	set_dcsw_handler(5, dispatch_start_proc);
	if(debug_mode & 1)
	{
		remove_game_object(hw_obj);
		qcreate("attract", 0x100, game_proc2, 0, 0, 0, 0);
	}
	else
	{
		qcreate("attract", 0x100, game_proc, 0, 0, 0, 0);
	}
}
