/*
 * adjust.c - the ADJUSTMENTS menu (0x8010efc8-0x8010f088): pricing, free play, attract mode
 * sounds, the full factory restore and, when ADJUST.FMT lists any, ADDITIONAL ADJUSTMENTS.
 */
#include <string.h>
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

extern int	free_play(void), attract_sounds(void), factory_restore(void);
extern menu_t	pricing_menu;
extern menu_item_t	additional_items[];


char	help_adj_pricing[] = "SELECT PRICING FOR THE GAME";
char	help_adj_free_play[] = "ENABLE/DISABLE FREE PLAY MODE";
char	help_adj_attract_sounds[] = "ENABLE/DISABLE ATTRACT MODE SOUNDS";
char	help_adj_factory_restore[] = "RESTORE AUDITS & ADJUSTMENTS TO FACTORY SETTINGS";
char	help_adj_exit[] = "RETURN TO MAIN MENU";

/* one spare entry for ADDITIONAL ADJUSTMENTS */
menu_item_t	adjust_items[] = {
	{ "PRICING", MENU_SUB, &pricing_menu, help_adj_pricing, 0 },
	{ "FREE PLAY", MENU_FUNC, free_play, help_adj_free_play, 0 },
	{ "ATTRACT SOUND", MENU_FUNC, attract_sounds, help_adj_attract_sounds, 0 },
	{ "FULL FACTORY RESTORE", MENU_FUNC, factory_restore, help_adj_factory_restore, 0 },
	{ "EXIT", MENU_EXIT, 0, help_adj_exit, 0 },
	{ 0, -1, 0, 0, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	adjust_menu = { "ADJUSTMENTS", -1.0F, -1.0F, 0, 0, adjust_items };
menu_t	additional_menu = { "ADDITIONAL ADJUSTMENTS", -1.0F, -1.0F, 0, 0, additional_items };
int	additional_added = 0;

/* 0x8010efc8 */
int adjustments(void)
{
	if(game_adjustments && !additional_added)
	{
		memcpy(&adjust_items[5], &adjust_items[4], sizeof(menu_item_t));
		memcpy(&adjust_items[4], &adjust_items[3], sizeof(menu_item_t));
		adjust_items[3].name = "ADDITIONAL ADJUSTMENTS";
		adjust_items[3].target = &additional_menu;
		adjust_items[3].kind = MENU_SUB;
		adjust_items[3].help = "ADDITIONAL ADJUSTMENTS";
		adjust_items[3].hook = 0;
		additional_added = 1;
	}
	return do_menu(&adjust_menu);
}
