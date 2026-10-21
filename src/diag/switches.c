/*
 * switches.c - PLAYER SWITCH INPUTS TEST (0x80108460-0x80108d8c): every player switch and
 * coin-door input by name, a red or green block for each, redrawn when one changes, and the
 * 49-way joysticks' positions on games that have them (none here). Both volume buttons exit.
 */
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

extern int	diag_get_dip_coin_current(void);
extern int	diag_get_player_sw_current(void);
extern int	get_player_49way_current(void);
extern void	sleep(int);
extern int	snd_scall_bank(char *, int, int, int, int);
extern int	(*sw_volume_both)(void);	/* 0x80167d94 */

/* a switch's name, its bit (or -1: none) and the colour of the name */
typedef struct switch_name {
	char	*name;
	int	bit;
	int	color;
} switch_name_t;

switch_name_t	player_switch_names[32] = {
	{ "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 },
	{ "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 },
	{ "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 },
	{ "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 },
	{ "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 },
	{ "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 },
	{ "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 },
	{ "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 }, { "", -1, 0 }
};

switch_name_t	coin_switch_names[16] = {
	{ "LEFT COIN", 0, 0xffffffff },
	{ "RIGHT COIN", 1, 0xffffffff },
	{ "P1 START", 2, 0xff00ff00 },
	{ "SLAM/TILT", 3, 0xffffffff },
	{ "TEST", 4, 0xffffffff },
	{ "P2 START", 5, 0xffff00ff },
	{ "SERVICE CREDIT", 6, 0xffffffff },
	{ "CENTER COIN", 7, 0xffffffff },
	{ "EXTRA COIN", 8, 0xffffffff },
	{ "", -1, 0 },
	{ "", -1, 0 },
	{ "VOLUME DOWN", 11, 0xffffffff },
	{ "VOLUME UP", 12, 0xffffffff },
	{ "", -1, 0 },
	{ "", -1, 0 },
	{ "BILL VALID.", 15, 0xffffffff }
};

/* 0x80108460: how many players have a 49-way joystick */
inline int num_49way_players(void)
{
	return 0;
}

/* how many player switches there are: 16 for two players, 32 for four */
static inline int num_player_switches(void)
{
	return 32;
}

/* 0x80108468 */
int switch_tests(void)
{
	int	last_coin = -1, last_player = -1, done = 0;
	int	coin, player, way, last_way = 0;
	int	i, bit, color;
	int	nways = num_49way_players(), nswitches = num_player_switches();
	float	x, y;

	delete_multiple_strings(0, 0);
	del1c(0, 0);
	draw_title("PLAYER SWITCH INPUTS TEST");
	message_box("PRESS BOTH VOLUME BUTTONS TO EXIT", -1.0F, 5.0F, 1.0F);

	/* the names */
	set_text_font(2);
	set_text_justification_mode(0);
	y = vres - font0_height * 3.5F;
	for(i = 0; i < nswitches; i++)
	{
		if(i == 16)
			y = vres - font0_height * 3.5F;
		if(i < 16)
			set_text_position(60, (int)y, 1.0F);
		else
			set_text_position(180, (int)y, 1.0F);
		if(player_switch_names[i].bit == i)
		{
			set_text_color(player_switch_names[i].color);
			oprintf(player_switch_names[i].name);
		}
		set_text_position((int)(hres * 0.6F), (int)y, 1.0F);
		if(i < 16 && coin_switch_names[i].bit == i)
		{
			set_text_color(coin_switch_names[i].color);
			oprintf(coin_switch_names[i].name);
		}
		y -= font2_height + font2_height;
	}
	if(nways)
	{
		set_text_justification_mode(2);
		if(nswitches == 16)
		{
			set_text_color(player_switch_names[0].color);
			set_text_position((int)(hres * 0.25F), (int)(vres * 0.145F), 1.0F);
			oprintf("PLAYER 1 49-WAY");
			set_text_color(player_switch_names[8].color);
			set_text_position((int)(hres * 0.75F), (int)(vres * 0.145F), 1.0F);
			oprintf("PLAYER 2 49-WAY");
		}
		else
		{
			set_text_color(player_switch_names[0].color);
			set_text_position((int)(hres * 0.14F), (int)(vres * 0.145F), 1.0F);
			oprintf("PLAYER 1 49-WAY");
			set_text_color(player_switch_names[8].color);
			set_text_position((int)(hres * 0.38F), (int)(vres * 0.145F), 1.0F);
			oprintf("PLAYER 2 49-WAY");
			set_text_color(player_switch_names[16].color);
			set_text_position((int)(hres * 0.62F), (int)(vres * 0.145F), 1.0F);
			oprintf("PLAYER 3 49-WAY");
			set_text_color(player_switch_names[24].color);
			set_text_position((int)(hres * 0.86F), (int)(vres * 0.145F), 1.0F);
			oprintf("PLAYER 4 49-WAY");
		}
	}

	/* the switches */
	while(!done)
	{
		coin = diag_get_dip_coin_current();
		player = diag_get_player_sw_current();
		way = get_player_49way_current();
		if(!nways)
			last_way = way;
		if(coin != last_coin || player != last_player || way != last_way)
		{
			delete_multiple_strings(1, 1);
			del1c(1, 1);
			last_way = way;
			last_coin = coin;
			last_player = player;
			set_text_font(2);
			y = vres - font0_height * 3.5F;
			bit = 1;
			for(i = 0; i < nswitches; i++)
			{
				if(i == 16)
					y = vres - font0_height * 3.5F;
				x = i < 16 ? hres * 0.16666667F + 40.0F : 246.0F;
				color = player & bit ? 0xff00ff00 : 0xffff0000;
				if(player_switch_names[i].bit == i)
					draw_rect(x, y, x + font2_height * 1.3333334F, y + font2_height, 1.0F, color, 1);
				x = hres * 0.85F;
				color = coin & bit ? 0xff00ff00 : 0xffff0000;
				if(i < 16 && coin_switch_names[i].bit == i)
					draw_rect(x, y, x + font2_height * 1.3333334F, y + font2_height, 1.0F, color, 1);
				bit <<= 1;
				y -= font2_height + font2_height;
			}
			if(nways)
			{
				set_text_justification_mode(2);
				set_string_id(1);
				if(nswitches == 16)
				{
					set_text_color(-1);
					set_text_position((int)(hres * 0.25F), (int)(vres * 0.095F), 1.0F);
					oprintf("%d", way & 0xff);
					set_text_position((int)(hres * 0.75F), (int)(vres * 0.095F), 1.0F);
					oprintf("%d", way >> 8 & 0xff);
				}
				else
				{
					set_text_color(-1);
					set_text_position((int)(hres * 0.14F), (int)(vres * 0.095F), 1.0F);
					oprintf("%d", way & 0xff);
					set_text_position((int)(hres * 0.38F), (int)(vres * 0.095F), 1.0F);
					oprintf("%d", way >> 8 & 0xff);
					set_text_position((int)(hres * 0.62F), (int)(vres * 0.095F), 1.0F);
					oprintf("%d", way >> 16 & 0xff);
					set_text_position((int)(hres * 0.86F), (int)(vres * 0.095F), 1.0F);
					oprintf("%d", (unsigned)way >> 24);
				}
			}
			snd_scall_bank("ttest", 13, 180, 127, 0x8000);
		}
		if(sw_volume_both())
			done = 1;
		sleep(1);
	}
	snd_scall_bank("ttest", 11, 180, 127, 0x8000);
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	return 0;
}
