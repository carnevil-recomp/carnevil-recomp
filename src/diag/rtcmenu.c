/*
 * rtcmenu.c - the REAL TIME CLOCK menu (data only: 0x801621f0-0x801622a8, its names in rodata
 * 0x801566e0-).
 */
#include "diag.h"

extern int	rtc_test(void);
extern menu_t	set_rtc_menu;

char	help_rtc_display[] = "DISPLAY THE REAL TIME CLOCK";
char	help_rtc_set[] = "SET THE REAL TIME CLOCK";
char	help_rtc_exit[] = "RETURN TO DIAGNOSTICS MENU";

menu_item_t	rtc_items[] = {
	{ "DISPLAY", MENU_FUNC, rtc_test, help_rtc_display, 0 },
	{ "SET", MENU_SUB, &set_rtc_menu, help_rtc_set, 0 },
	{ "EXIT", MENU_EXIT, 0, help_rtc_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	rtc_menu = { "REAL TIME CLOCK", -1.0F, -1.0F, 0, 0, rtc_items };
