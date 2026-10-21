/*
 * diagmenu.c - the DIAGNOSTICS menu (data only: 0x80161538-0x80161754, its names in rodata
 * 0x80152c90-).
 */
#include "diag.h"

extern menu_t	monitor_menu, rtc_menu, disk_menu, speaker_menu;
extern int	system_info(void), sound_test(void), switch_tests(void), dip_switch_tests(void);
extern int	gun_test(void);

char	help_monitor[] = "COLOR BARS, CROSS HATCH, SATURATION, ETC.";
char	help_system_info[] = "SHOWS SERIAL NUMBER AND DATE CODE";
char	help_rtc[] = "DISPLAY/SET THE REAL TIME CLOCK";
char	help_sound[] = "SOUND SUBSYSTEM TESTS, ETC.";
char	help_disk[] = "DISK INFO AND PERFORMANCE TESTS";
char	help_switch_tests[] = "TEST FOR PLAYER INPUT SWITCHES AND STICKS";
char	help_dip_switch_tests[] = "TEST FOR DIP-SWITCH INPUTS";
char	help_speakers[] = "TEST SPEAKERS";
char	help_guns[] = "TEST GUNS";
char	help_diag_exit[] = "RETURN TO PREVIOUS MENU";

menu_item_t	diag_items[] = {
	{ "MONITOR SETUP", MENU_SUB, &monitor_menu, help_monitor, 0 },
	{ "SYSTEM INFO", MENU_FUNC, system_info, help_system_info, 0 },
	{ "REAL TIME CLOCK", MENU_SUB, &rtc_menu, help_rtc, 0 },
	{ "SOUND SUBSYSTEM", MENU_FUNC, sound_test, help_sound, 0 },
	{ "DISK TESTS", MENU_SUB, &disk_menu, help_disk, 0 },
	{ "SWITCH TESTS", MENU_FUNC, switch_tests, help_switch_tests, 0 },
	{ "DIP-SWITCH TESTS", MENU_FUNC, dip_switch_tests, help_dip_switch_tests, 0 },
	{ "SPEAKER TEST", MENU_SUB, &speaker_menu, help_speakers, 0 },
	{ "GUN TEST", MENU_FUNC, gun_test, help_guns, 0 },
	{ "EXIT", MENU_EXIT, 0, help_diag_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	diagnostics_menu = { "DIAGNOSTICS", -1.0F, -1.0F, 0, 0, diag_items };
