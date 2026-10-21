/*
 * namerica_pricing.c - the pricing menu's NORTH AMERICA submenu (data only:
 * 0x801650f0-0x8016519c).
 */
#include "diag.h"

extern int	set_pricing(int);		/* 0x8010f8a4 */
extern void	set_pricing_country(int);

char	help_usa[] = "UNITED STATES PRICING";
char	help_canada[] = "CANADIAN PRICING";
char	help_namerica_exit[] = "RETURN TO PRICING MENU";

menu_item_t	namerica_items[] = {
	{ "UNITED STATES", MENU_FUNC, set_pricing, help_usa, set_pricing_country },
	{ "CANADA", MENU_FUNC, set_pricing, help_canada, set_pricing_country },
	{ "EXIT", MENU_EXIT, 0, help_namerica_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	namerica_menu = { "NORTH AMERICA", -1.0F, -1.0F, 0, 0, namerica_items };
