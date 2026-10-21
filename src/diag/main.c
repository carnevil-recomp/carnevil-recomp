/*
 * main.c - DIAG.EXE's main() and its start-up helpers (0x801009b0-0x8010138c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <goose/sound.h>
#include <goose/sprites.h>
#include <goose/fonts.h>
#include <goose/ostrings.h>
#include "diag.h"

extern char	*snd_bank_name;					/* 0x802ef5b4 */
extern int	get_adjustment(int, int *);
extern int	exec(char *, unsigned int, int *);
extern char	*get_rom_version(void);
extern int	get_dip_coin_current(void);
extern void	sleep(int);
extern void	func_8012a0a0(int);
extern void	func_8011b4d8(void);
extern void	message_box(char *, float, float, float);
extern void	wait_test_exit(void);
extern int	font_set;					/* 0x801694cc */
/* the DFONT.WMS fonts (0x801610a0-), by height */
extern font_info_t	dfont6, dfont8, dfont10, dfont13, dfont20;

/* what the menus run (named after their items) */
extern int	dump_audits(void), clear_credits(void), reset_high_scores(void), gun_calibration(void);
extern int	default_adjustments(void), factory_restore(void), adjustments(void), volume_level(void);
extern int	switch_tests(void), dip_switch_tests(void);
extern menu_t	audits_menu, clear_audits_menu, diagnostics_menu;
extern char	help_switch_tests[], help_dip_switch_tests[];	/* 0x801615e4, 0x80161610 (with the tests) */

/* 0x8015cc68: the diagnostics' version, shown as 3.4 */
char	diag_revision[] = "$Revision: 34 $";

char	help_leave[] = "LEAVE THE DIAGNOSTICS";
char	help_diagnostics[] = "SYSTEM LEVEL DIAGNOSTICS";
char	help_volume[] = "SET THE VOLUME LEVEL FOR THE GAME";
char	help_audits[] = "VIEW/CLEAR GAME AUDITS";
char	help_adjustments[] = "VIEW/ADJUST GAME ADJUSTMENTS";
char	help_dump_audits[] = "DUMP AUDITS TO SERIAL PORT";
char	help_clear_credits[] = "CLEAR CREDITS AUDIT";
char	help_clear_audits[] = "CLEAR ASSORTED AUDITS";
char	help_high_scores[] = "RESET HIGH SCORES";
char	help_gun_calibration[] = "GUN CALIBRATION";
char	help_default_adjustments[] = "RESTORE ADJUSTMENTS TO FACTORY SETTINGS";
char	help_factory_restore[] = "RESTORE AUDITS & ADJUSTMENTS TO FACTORY SETTINGS";
char	help_previous[] = "RETURN TO PREVIOUS MENU";
char	help_utilities[] = "UTILITIES";

menu_item_t	utilities_items[] = {
	{ "DUMP AUDITS", MENU_FUNC, dump_audits, help_dump_audits, 0 },
	{ "CLEAR CREDITS", MENU_FUNC, clear_credits, help_clear_credits, 0 },
	{ "CLEAR AUDITS", MENU_SUB, &clear_audits_menu, help_clear_audits, 0 },
	{ "RESET HIGH SCORES", MENU_FUNC, reset_high_scores, help_high_scores, 0 },
	{ "GUN CALIBRATION", MENU_FUNC, gun_calibration, help_gun_calibration, 0 },
	{ "DEFAULT ADJUSTMENTS", MENU_FUNC, default_adjustments, help_default_adjustments, 0 },
	{ "FULL FACTORY RESTORE", MENU_FUNC, factory_restore, help_factory_restore, 0 },
	{ "EXIT", MENU_EXIT, 0, help_previous, 0 },
	{ 0, -1, 0, 0, 0 }
};
menu_t	utilities_menu = { "UTILITIES", -1.0F, -1.0F, 0, 0, utilities_items };

menu_item_t	main_items[] = {
	{ "AUDITS", MENU_SUB, &audits_menu, help_audits, 0 },
	{ "GUN CALIBRATION", MENU_FUNC, gun_calibration, help_gun_calibration, 0 },
	{ "ADJUSTMENTS", MENU_FUNC, adjustments, help_adjustments, 0 },
	{ "DIAGNOSTICS", MENU_SUB, &diagnostics_menu, help_diagnostics, 0 },
	{ "VOLUME LEVEL", MENU_FUNC, volume_level, help_volume, 0 },
	{ "UTILITIES", MENU_SUB, &utilities_menu, help_utilities, 0 },
	{ "EXIT", MENU_EXIT, 0, help_leave, 0 },
	{ 0, -1, 0, 0, 0 }
};

/* the main menu without the operator settings (the BIOS argument says which) */
menu_item_t	main_items_short[] = {
	{ "AUDITS", MENU_SUB, &audits_menu, help_audits, 0 },
	{ "VOLUME LEVEL", MENU_FUNC, volume_level, help_volume, 0 },
	{ "SWITCH TESTS", MENU_FUNC, switch_tests, help_switch_tests, 0 },
	{ "DIP-SWITCH TESTS", MENU_FUNC, dip_switch_tests, help_dip_switch_tests, 0 },
	{ "EXIT", MENU_EXIT, 0, help_leave, 0 },
	{ 0, -1, 0, 0, 0 }
};

char	main_title[80] = "CARNEVIL MAIN MENU";
menu_t	main_menu = { main_title, -1.0F, -1.0F, 0, 0, main_items };
menu_t	main_menu_short = { main_title, -1.0F, -1.0F, 0, 0, main_items_short };

/* 0x8015d058: the three fonts at each resolution, as fonts 2, 1 and 0 */
font_info_t	*fonts[2][3] = {
	{ &dfont8, &dfont10, &dfont20 },
	{ &dfont6, &dfont8, &dfont13 }
};

extern int	diag_version;					/* 0x801726f8 */
extern char	rom_version[];					/* 0x80172708 */
extern char	rom_version_number[];				/* 0x80172748 */

extern int	update_rom_check(void);				/* 0x80116020: -11 = no update ROM */
extern int	update_rom_process(void);			/* 0x80116068 */
extern void	draw_title(char *);				/* 0x80101874 */
extern void	wait_prompt(char *);				/* 0x80101390 */
extern float	update_text_y;					/* 0x802fdd00: the update's next line */

void	main_init_sound(void);
void	software_update(void);

/* 0x801009b0: the main process (GOOSE main() starts it with the BIOS arguments): the version
   screen, the start-up tests, then the main menu until it exits; with dip switch 8 still on
   it says so and goes back to the menu, else the BIOS loads the game */
void main_proc(int *args)
{
	int	exec_args[4] = { 0, 0, 0, 0 };
	char	*p;
	int	i = 0, ver;

	if(hres < 512.0F)
		dfont6.char_space = 1;
	main_init_sound();
	func_8012a0a0(0xff000000);
	if(!create_font(fonts[font_set][0], 2))
		while(1) ;
	if(!create_font(fonts[font_set][1], 1))
		while(1) ;
	if(!create_font(fonts[font_set][2], 0))
		while(1) ;
	font_set = 0;
	font0_height = (float)get_font_height(0);
	font1_height = (float)get_font_height(1);
	font2_height = (float)get_font_height(2);
	set_text_justification_mode(HJUST_CENTER);
	set_text_color(0xffffff00);
	set_string_id(0);
	set_text_font(1);

	set_text_position((int)(hres / 2.0F), (int)(vres * 0.8F), 1.0F);
	p = diag_revision;
	while(*p != ' ')
		p++;
	sscanf(p, " %d", &diag_version);
	oprintf("Diagnostics - Version: %d.%d", diag_version / 10, diag_version % 10);

	set_text_position((int)(hres / 2.0F), (int)(vres * 0.75F), 1.0F);
	oprintf("%s  %s", __DATE__, __TIME__);

	strcpy(rom_version, get_rom_version());
	p = rom_version;
	while(*p != ' ')
		p++;
	p++;
	while(*p != ' ')
	{
		rom_version_number[i] = *p;
		i++;
		p++;
	}
	rom_version_number[i] = 0;
	ver = atoi(rom_version_number);
	ver += 10;
	set_text_position((int)(hres / 2.0F), (int)(vres * 0.7F), 1.0F);
	oprintf("Boot ROM Version:  %d.%d", ver / 10, ver % 10);

	func_8011b4d8();
	sleep(285);
	delete_multiple_strings(0, 0);
	software_update();
again:
	do_menu(args[0] ? &main_menu_short : &main_menu);
	if(get_dip_coin_current() < 0)
	{
		while(sw_select() || sw_back() || sw_8())
			sleep(1);
		delete_multiple_strings(0, 0);
		del1c(0, 0);
		message_box("\x1b-65536cU8 SWITCH 8 IS ON - PLEASE TURN OFF", -1.0F, -1.0F, 1.0F);
		wait_test_exit();
		if(get_dip_coin_current() < 0)
			goto again;
	}
	exec("GAME.EXE", 0x800c0000, exec_args);
	exit(0);
}

/* 0x80100eac: the sound system with the test bank and the game's own, at the master volume
   adjustment (150 if it cannot be read) */
void main_init_sound(void)
{
	int	vol = 0x80;

	snd_reset();
	if(snd_init())
	{
		printf("\n");
		printf("main_init_sound(): ERROR initializing sound system.\n");
	}
	snd_bank_init();
	snd_bank_load("ttest");
	snd_bank_load(snd_bank_name);
	if(get_adjustment(3, &vol))
		vol = 150;
	snd_master_volume(vol);
}

/* 0x80100f4c: with an update ROM fitted, apply it and say how it went, then wait for the power
   to go off (the screen comes back after each button press) */
void software_update(void)
{
	int	status, done;
	float	y;

	status = update_rom_check();
	if(status == -11)
		return;
	done = 0;
	for(;;)
	{
		draw_title("SOFTWARE UPDATE MODE");
		set_text_justification_mode(HJUST_CENTER);
		set_string_id(0);
		set_text_font(1);
		y = vres * 0.8F;
		set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
		if(!done)
		{
			done = 1;
			if(status == 0)
			{
				set_text_color(0xffffffff);
				oprintf("VALID UPDATE ROM DETECTED");
				y -= font1_height;
				y -= font1_height;
				y -= font1_height;
				y -= font1_height;
				y -= font1_height;
				set_text_color(0xffffff00);
				set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
				oprintf("PROCESSING ROM.");
				y -= font1_height;
				y -= font1_height;
				update_text_y = y;
				sleep(2);
				status = update_rom_process();
				set_text_font(1);
				update_text_y -= font1_height;
				update_text_y -= font1_height;
				set_text_position((int)(hres / 2.0F), (int)update_text_y, 1.0F);
				switch(status)
				{
				case -31: case -30: case -29: case -28: case -27: case -26: case -25:
				case -24: case -23: case -22: case -21: case -18: case -15: case -14:
				case -13: case -12: case -10: case -9: case -8: case -6: case -5:
				case -4: case -3: case -2: case -1:
					set_text_color(0xffff0000);
					oprintf("OPERATION FAILURE");
					break;
				case -16:
					set_text_color(0xffffff00);
					oprintf("NOTHING TO DO");
					sleep(180);
					delete_multiple_strings(0, 0);
					del1c(0, 0);
					return;
				default:
					set_text_color(0xff00ff00);
					oprintf("OPERATION COMPLETE");
					break;
				}
			}
			else
			{
				set_text_color(0xffff0000);
				oprintf("UPDATE ROM CHECKSUM ERROR DETECTED");
				y -= font1_height;
				y -= font1_height;
				y -= font1_height;
				y -= font1_height;
				y -= font1_height;
				set_text_color(0xffffff00);
				set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
				oprintf("CAN NOT PERFORM SOFTWARE UPDATE.");
				y -= font1_height;
				y -= font1_height;
				y -= font1_height;
				y -= font1_height;
				set_text_color(0xffffffff);
				set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
				oprintf("PLEASE CONTACT YOUR DISTRIBUTOR");
				y -= font1_height;
				y -= font1_height;
				set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
				oprintf("TO GET YOUR EPROM UPDATE SET");
				y -= font1_height;
				y -= font1_height;
				set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
				oprintf("CHECKED AND/OR REPLACED.");
			}
		}
		wait_prompt("POWER DOWN AND REMOVE ROM TO EXIT");
		delete_multiple_strings(0, 0);
		del1c(0, 0);
		while(sw_select() || sw_back() || sw_8())
			sleep(1);
	}
}
