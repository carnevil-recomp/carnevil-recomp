/*
 * monitor.c - the monitor test menu (data only: 0x801611a0-0x801614a4, its names in rodata
 * 0x80152bc8-). Each pattern is a file of its own: color_bars.c, crosshatch.c and the solid
 * screens.
 */
#include "diag.h"

extern int	color_bars(void), crosshatch(void), red_screen(void), blue_screen(void), green_screen(void);
extern int	white_screen(void), black_screen(void), gray50_screen(void), gray25_screen(void);

char	help_color_bars[] = "COLOR BARS  (PRESS CREDIT OR TEST TO EXIT TEST)";
char	help_crosshatch[] = "CROSS-HATCH  (PRESS CREDIT OR TEST TO EXIT TEST)";
char	help_red_screen[] = "SATURATED RED SCREEN  (PRESS CREDIT OR TEST TO EXIT TEST)";
char	help_blue_screen[] = "SATURATED BLUE SCREEN  (PRESS CREDIT OR TEST TO EXIT TEST)";
char	help_green_screen[] = "SATURATED GREEN SCREEN  (PRESS CREDIT OR TEST TO EXIT TEST)";
char	help_white_screen[] = "SATURATED WHITE SCREEN  (PRESS CREDIT OR TEST TO EXIT TEST)";
char	help_black_screen[] = "BLACK SCREEN  (PRESS CREDIT OR TEST TO EXIT TEST)";
char	help_gray50_screen[] = "GRAY (50%) SCREEN  (PRESS CREDIT OR TEST TO EXIT TEST)";
char	help_gray25_screen[] = "GRAY (25%) SCREEN  (PRESS CREDIT OR TEST TO EXIT TEST)";
char	help_monitor_exit[] = "RETURN TO PREVIOUS MENU";

menu_item_t	monitor_items[] = {
	{ "COLOR BARS", MENU_FUNC, color_bars, help_color_bars, 0 },
	{ "CROSS HATCH", MENU_FUNC, crosshatch, help_crosshatch, 0 },
	{ "RED SCREEN", MENU_FUNC, red_screen, help_red_screen, 0 },
	{ "BLUE SCREEN", MENU_FUNC, blue_screen, help_blue_screen, 0 },
	{ "GREEN SCREEN", MENU_FUNC, green_screen, help_green_screen, 0 },
	{ "WHITE SCREEN", MENU_FUNC, white_screen, help_white_screen, 0 },
	{ "BLACK SCREEN", MENU_FUNC, black_screen, help_black_screen, 0 },
	{ "50 PCT. GRAY SCREEN", MENU_FUNC, gray50_screen, help_gray50_screen, 0 },
	{ "25 PCT. GRAY SCREEN", MENU_FUNC, gray25_screen, help_gray25_screen, 0 },
	{ "EXIT", MENU_EXIT, 0, help_monitor_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	monitor_menu = { "MONITOR SETUP", -1.0F, -1.0F, 0, 0, monitor_items };
