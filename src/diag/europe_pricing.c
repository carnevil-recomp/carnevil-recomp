/*
 * europe_pricing.c - the pricing menu's two EUROPE submenus, countries A to H and I to U
 * (data only: 0x801651a0-0x801654c8).
 */
#include "diag.h"

extern int	set_pricing(int);		/* 0x8010f8a4 */
extern void	set_pricing_country(int);

char	help_europe_exit[] = "RETURN TO PRICING MENU";
char	help_austria[] = "AUSTRIAN PRICING MODES";
char	help_belgium[] = "BELGIAN PRICING MODES";
char	help_hungary[] = "HUNGARIAN PRICING MODES";
char	help_denmark[] = "DANISH PRICING MODES";
char	help_finland[] = "FINNISH PRICING MODES";
char	help_france[] = "FRENCH PRICING MODES";
char	help_france_eca[] = "FRENCH ECA PRICING MODES";
char	help_germany[] = "GERMAN PRICING MODES";

menu_item_t	europe_ah_items[] = {
	{ "AUSTRIA", MENU_FUNC, set_pricing, help_austria, set_pricing_country },
	{ "BELGIUM", MENU_FUNC, set_pricing, help_belgium, set_pricing_country },
	{ "DENMARK", MENU_FUNC, set_pricing, help_denmark, set_pricing_country },
	{ "FINLAND", MENU_FUNC, set_pricing, help_finland, set_pricing_country },
	{ "FRANCE", MENU_FUNC, set_pricing, help_france, set_pricing_country },
	{ "FRANCE ECA", MENU_FUNC, set_pricing, help_france_eca, set_pricing_country },
	{ "GERMANY", MENU_FUNC, set_pricing, help_germany, set_pricing_country },
	{ "HUNGARY", MENU_FUNC, set_pricing, help_hungary, set_pricing_country },
	{ "EXIT", MENU_EXIT, 0, help_europe_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	europe_ah_menu = { "EUROPE", -1.0F, -1.0F, 0, 0, europe_ah_items };

char	help_uk[] = "BRITISH PRICING MODES";
char	help_italy[] = "ITALIAN PRICING MODES";
char	help_netherlands[] = "DUTCH PRICING MODES";
char	help_norway[] = "NORWEGIAN PRICING MODES";
char	help_spain[] = "SPANISH PRICING MODES";
char	help_sweden[] = "SWEDISH PRICING MODES";
char	help_switzerland[] = "SWISS PRICING MODES";

menu_item_t	europe_iu_items[] = {
	{ "ITALY", MENU_FUNC, set_pricing, help_italy, set_pricing_country },
	{ "NETHERLANDS", MENU_FUNC, set_pricing, help_netherlands, set_pricing_country },
	{ "NORWAY", MENU_FUNC, set_pricing, help_norway, set_pricing_country },
	{ "SPAIN", MENU_FUNC, set_pricing, help_spain, set_pricing_country },
	{ "SWEDEN", MENU_FUNC, set_pricing, help_sweden, set_pricing_country },
	{ "SWITZERLAND", MENU_FUNC, set_pricing, help_switzerland, set_pricing_country },
	{ "UNITED KINGDOM", MENU_FUNC, set_pricing, help_uk, set_pricing_country },
	{ "EXIT", MENU_EXIT, 0, help_europe_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	europe_iu_menu = { "EUROPE", -1.0F, -1.0F, 0, 0, europe_iu_items };
