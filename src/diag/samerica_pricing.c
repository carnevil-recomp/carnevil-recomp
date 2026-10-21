/*
 * samerica_pricing.c - the pricing menu's SOUTH AMERICA submenu (data only:
 * 0x80167598-0x80167618).
 */
#include "diag.h"

extern int	set_pricing(void);		/* 0x8010f8a4 */
extern void	set_pricing_country(int);

char	help_antilles[] = "ANTILLES PRICING";
char	help_samerica_exit[] = "RETURN TO PRICING MENU";

menu_item_t	samerica_items[] = {
	{ "ANTILLES", MENU_FUNC, set_pricing, help_antilles, set_pricing_country },
	{ "EXIT", MENU_EXIT, 0, help_samerica_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	samerica_menu = { "SOUTH AMERICA", -1.0F, -1.0F, 0, 0, samerica_items };
