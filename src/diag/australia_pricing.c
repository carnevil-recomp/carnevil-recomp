/*
 * australia_pricing.c - the pricing menu's AUSTRALIA submenu (data only:
 * 0x80167458-0x80167510).
 */
#include "diag.h"

extern int	set_pricing(int);		/* 0x8010f8a4 */
extern void	set_pricing_country(int);

char	help_australia_exit[] = "RETURN TO PRICING MENU";
char	help_australian[] = "AUSTRALIAN PRICING MODES";
char	help_new_zealand[] = "NEW ZEALANDER PRICING MODES";

menu_item_t	australia_items[] = {
	{ "AUSTRALIA", MENU_FUNC, set_pricing, help_australian, set_pricing_country },
	{ "NEW ZEALAND", MENU_FUNC, set_pricing, help_new_zealand, set_pricing_country },
	{ "EXIT", MENU_EXIT, 0, help_australia_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	australia_menu = { "AUSTRALIA", -1.0F, -1.0F, 0, 0, australia_items };
