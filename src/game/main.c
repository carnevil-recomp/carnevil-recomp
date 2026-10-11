/*
 * main.c - CarnEvil's main process (created by GOOSE main() as "main").
 */
#include <stdio.h>
#include "types.h"

struct process_node;
extern struct process_node *qcreate(char *, int, void (*)(), int, int, int, int);
extern void	qyield(int);
extern void	ide_set_partition(int);
extern void	func_801459e0(u32);
extern void	func_800ca3a0(void), func_800c7570(void), func_800f1a20(void), func_800f1fa8(void), func_800f1d88(void), func_800e5014(void);
extern void	func_8014c65c(void), func_800ca448(void);
extern int	func_800f1bac(void);
extern int	func_80157004(void);
extern void	func_800c6480(int *);
extern int	setup_cmos_800f2a28(void);
extern int	write_adjustments_file_wrapper(void);	/* check_adjustments() */
extern int	get_audit(int, int *), get_adjustment(int, int *);
extern int	set_audit(int, int), set_adjustment(int, int);
extern void	reset_high_scores(void);
extern void	set_frame_factor(int), main_init_sound(void), snd_master_volume(int);
extern void	set_psw_handler(int, void (*)());
extern int	add_object1_wrapper(int, void (*)(), int, int);
extern void	remove_game_object(short);
extern int	kss_reset_(void), kss_reset_w_calib_(void);
extern void	hw_info_screen(), utime_proc(), coin_volume_proc(), ccproc_proc(), game_proc(), game_proc2();
extern void	func_800c718c(), func_800c726c(), func_800c6f28(), dispatch_slam_proc(), dispatch_diags_proc(), dispatch_start_proc();
extern void	load_jester_texmap(void), load_jester_model(void), load_inng_texmap(void), load_chnk_texmap(void), load_chnk_model(void);
extern void	load_shotgun_texmap(void), load_health_texmap(void), load_ammmo_texmap(void), load_machinegun_texmap(void);
extern void	load_flamethrower_texmap(void), load_acid_texmap(void), load_shotgun_model(void), load_health_model(void);
extern void	load_ammo_model(void), load_machinegun_model(void), load_flamethrower_model(void), load_acid_model(void);

extern int	D_801facc8, cmos_status, D_801e096c, D_80237404, snd_opsys_ver, D_801fad10, serial_number, game_id;
extern int	volume, D_80237274, D_80212d98, debug_mode, gfx_scale;
extern float	D_80212df4;
extern struct process_node *D_801e5da8;

void main_proc(void)
{
	int	hw_obj;
	int	restored;
	int	val;
	int	n;

	restored = 0;
	D_801facc8 = 0;
	cmos_status = 0;
	D_801e096c = 0;
	D_80237404 = 0;
	snd_opsys_ver = 0;
	ide_set_partition(1);
	func_801459e0(0xff000000);
	D_801fad10 = 1;
	func_800ca3a0();
	func_800c6480(&serial_number);
	hw_obj = add_object1_wrapper(0x20, hw_info_screen, 0, 0);
	if(game_id != 0x210 && game_id != 0x1d5 && game_id != 0x1e6)
	{
		while(1)
		{
			qyield(1);
		}
	}
	if(setup_cmos_800f2a28() == 0)
	{
		if(get_audit(0xb5, &val) || val != 0xb5)
		{
			func_800f1a20();
		}
		set_audit(0xb5, 0xb5);
		if((n = write_adjustments_file_wrapper()) != 0)
		{
			printf("Errors in check_adjustments %d\n", n);
			func_800f1fa8();
			restored = 1;
			func_800f1a20();
			set_adjustment(0x52, 9999);
			if(write_adjustments_file_wrapper())
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
			func_800f1fa8();
			func_800f1a20();
			restored = 1;
			set_adjustment(0x52, 8888);
			set_adjustment(0x2e, 0);
		}
		else if(val)
		{
			cmos_status = 1;
			printf("Factory restore adjustment %d\n", val);
			restored = 1;
			func_800f1fa8();
			func_800f1a20();
			set_adjustment(0x52, 8888);
			set_adjustment(0x2e, 0);
		}
		if(!func_800f1bac() || write_adjustments_file_wrapper())
		{
			cmos_status = 2;
		}
		if(cmos_status != 2)
		{
			func_800f1bac();
			qcreate("utime", 0x8000, utime_proc, 0, 0, 0, 0);
		}
	}
	else
	{
		fprintf(stderr, "Error occured in trying to INIT. cmos err num %d\n", setup_cmos_800f2a28());
		cmos_status = 2;
	}
	if(cmos_status == 0)
	{
		cmos_status = 3;
		func_800e5014();
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
	func_800f1d88();
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
	func_800c7570();
	if(cmos_status == 2 || D_80237274)
	{
		D_801e096c = 2;
	}
	else
	{
		D_801e096c = 1;
	}
	gfx_scale = func_80157004();
	set_frame_factor(3);
	main_init_sound();
	set_psw_handler(0, func_800c718c);
	set_psw_handler(1, func_800c718c);
	set_psw_handler(7, func_800c718c);
	set_psw_handler(8, func_800c718c);
	set_psw_handler(0xf, func_800c718c);
	set_psw_handler(6, func_800c726c);
	set_psw_handler(3, dispatch_slam_proc);
	set_psw_handler(0xb, func_800c6f28);
	set_psw_handler(0xc, func_800c6f28);
	set_psw_handler(4, dispatch_diags_proc);
	qcreate("volume", 0x8201, coin_volume_proc, 0, 0, 0, 0);
	D_801e5da8 = qcreate("ccproc", 0x8200, ccproc_proc, 0, 0, 0, 0);
	func_8014c65c();
	func_800ca448();
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
	qyield(1);
	remove_game_object(hw_obj);
	if(cmos_status == 2)
	{
		D_80237404 = kss_reset_();
	}
	else
	{
		D_80237404 = kss_reset_w_calib_();
	}
	hw_obj = add_object1_wrapper(0x20, hw_info_screen, 0, 0);
	qyield(20);
	D_80212d98 = 0;
	set_psw_handler(2, dispatch_start_proc);
	set_psw_handler(5, dispatch_start_proc);
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
