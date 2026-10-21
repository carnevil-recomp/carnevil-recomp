/*
 * asia_pricing.c - the pricing menu's ASIA submenu (0x801105a0-0x801105b0, data
 * 0x80167510-0x80167598), and the hook all the country submenus use to note the country.
 */
#include "diag.h"

extern int	set_pricing(void);		/* 0x8010f8a4 */

int	pricing_country = 0;

/* 0x801105a0: a country item's hook (do_menu passes the highlighted item) */
void set_pricing_country(int n)
{
	pricing_country = n;
}

char	help_asia_exit[] = "RETURN TO PRICING MENU";
char	help_japan[] = "JAPANESE PRICING MODES";

menu_item_t	asia_items[] = {
	{ "JAPAN", MENU_FUNC, set_pricing, help_japan, set_pricing_country },
	{ "EXIT", MENU_EXIT, 0, help_asia_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	asia_menu = { "ASIA", -1.0F, -1.0F, 0, 0, asia_items };
