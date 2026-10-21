/*
 * pricing_menu.c - the PRICING menu (0x8010f638-0x8010f64c, data 0x80164ee8-0x801650ec):
 * show the current pricing, a continent's submenu, custom pricing. Highlighting a continent
 * notes it for set_pricing() (pricing.c).
 */
#include "diag.h"

extern int	show_current_pricing(void);	/* 0x801110b8 */
extern int	custom_pricing(void);		/* 0x80111c88 */
extern menu_t	namerica_menu, samerica_menu, europe_ah_menu, europe_iu_menu, asia_menu, australia_menu;

int	pricing_continent = 0;

/* 0x8010f638: a continent item's hook (do_menu passes the highlighted item; item 0 is SHOW CURRENT) */
void set_pricing_continent(int n)
{
	pricing_continent = n - 1;
}

char	help_show_current[] = "SHOW THE CURRENT PRICING SETTINGS";
char	help_namerica[] = "NORTH AMERICAN PRICING MODES";
char	help_samerica[] = "SOUTH AMERICAN PRICING MODES";
char	help_europe_ah[] = "EUROPEAN PRICING MODES (COUNTRIES A TO H)";
char	help_europe_iu[] = "EUROPEAN PRICING MODES (COUNTRIES I TO U)";
char	help_asia[] = "ASIAN PRICING MODES";
char	help_australia[] = "AUSTRALIAN PRICING MODES";
char	help_custom_pricing[] = "SET UP CUSTOM PRICING";
char	help_pricing_exit[] = "RETURN TO ADJUSTMENTS MENU";

menu_item_t	pricing_items[] = {
	{ "SHOW CURRENT", MENU_FUNC, show_current_pricing, help_show_current, 0 },
	{ "NORTH AMERICA", MENU_SUB, &namerica_menu, help_namerica, set_pricing_continent },
	{ "SOUTH AMERICA", MENU_SUB, &samerica_menu, help_samerica, set_pricing_continent },
	{ "EUROPE (A-H)", MENU_SUB, &europe_ah_menu, help_europe_ah, set_pricing_continent },
	{ "EUROPE (I-U)", MENU_SUB, &europe_iu_menu, help_europe_iu, set_pricing_continent },
	{ "ASIA", MENU_SUB, &asia_menu, help_asia, set_pricing_continent },
	{ "AUSTRALIA", MENU_SUB, &australia_menu, help_australia, set_pricing_continent },
	{ "CUSTOM PRICING", MENU_FUNC, custom_pricing, help_custom_pricing, 0 },
	{ "EXIT", MENU_EXIT, 0, help_pricing_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	pricing_menu = { "PRICING", -1.0F, -1.0F, 0, 0, pricing_items };
