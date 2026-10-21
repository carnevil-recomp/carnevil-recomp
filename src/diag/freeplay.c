/*
 * freeplay.c - free play and the totalizer (0x8010f088-0x8010f634). Free play is the dip
 * switches' when they set the coinage.
 */
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include <goose/adjust.h>
#include "diag.h"

extern int	get_dip_coin_current(void);
extern int	get_adjustment(int, int *);
extern int	set_adjustment(int, int);
extern int	graphical_printf(char *, char *, ...);
extern int	dialog(char *, float, float, float, int);

/* 0x8010f088: set by the dip switches (bit 16 clear: their coinage) or asked */
int free_play(void)
{
	char	buf[40];
	int	dips, val;

	dips = get_dip_coin_current();
	if(!(dips & 0x10000))
	{
		set_text_font(0);
		set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
		set_text_color(0xffff0000);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F + (font0_height + font0_height)), 1.0F);
		if((dips & 0x3e0000) == 0x3e0000)
			oprintf("FREE PLAY IS ENABLED BY THE");
		else
			oprintf("FREE PLAY IS DISABLED BY THE");
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		oprintf("CURRENT DIPSWITCH SETTINGS");
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F - (font0_height + font0_height)), 1.0F);
		oprintf("AND CAN NOT BE ADJUSTED");
		wait_test_exit();
		delete_multiple_strings(0, 0);
		del1c(0, 0);
		return 0;
	}
	if(get_adjustment(FREEPLAY_ADJ, &val))
	{
		set_text_font(0);
		set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		set_text_color(0xffff0000);
		oprintf("THE CMOS SETTING OF FREE PLAY IS");
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F - (font0_height + font0_height)), 1.0F);
		oprintf("INVALID - RESETTING TO FREE PLAY OFF");
		set_adjustment(FREEPLAY_ADJ, 0);
		wait_test_exit();
		delete_multiple_strings(0, 0);
		del1c(0, 0);
		get_adjustment(FREEPLAY_ADJ, &val);
	}
	val = dialog("ENABLE FREE PLAY ?", -1.0F, -1.0F, 1.0F, 3);
	set_adjustment(FREEPLAY_ADJ, val == 1);
	if(get_adjustment(FREEPLAY_ADJ, &val))
		message_box("SETTING FREE PLAY MODE FAILED", -1.0F, -1.0F, 1.0F);
	else
	{
		graphical_printf(buf, "FREE PLAY %s", val == 1 ? "ENABLED" : "DISABLED");
		message_box(buf, -1.0F, -1.0F, 1.0F);
	}
	wait_test_exit();
	return 0;
}

/* 0x8010f3f0 */
int totalizer(void)
{
	int	val;

	if(get_adjustment(TOTALIZER_ADJ, &val))
	{
		set_text_font(0);
		set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		set_text_color(0xffff0000);
		oprintf("THE CMOS SETTING OF TOTALIZER MODE IS");
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F - (font0_height + font0_height)), 1.0F);
		oprintf("INVALID - RESETTING TO TOTALIZER MODE ON");
		set_adjustment(TOTALIZER_ADJ, 1);
		wait_test_exit();
		delete_multiple_strings(0, 0);
		del1c(0, 0);
		get_adjustment(TOTALIZER_ADJ, &val);
	}
	if(val)
	{
		val = dialog("TOTALIZER MODE IS ENABLED - DO YOU WANT TO DISABLE ?", -1.0F, -1.0F, 1.0F, 3);
		if(val == 1)
		{
			set_adjustment(TOTALIZER_ADJ, 0);
			if(get_adjustment(TOTALIZER_ADJ, &val))
				message_box("SETTING OF TOTALIZER MODE MODE FAILED", -1.0F, -1.0F, 1.0F);
			else
				message_box("TOTALIZER MODE MODE DISABLED", -1.0F, -1.0F, 1.0F);
		}
		else
			message_box("TOTALIZER MODE MODE NOT CHANGED", -1.0F, -1.0F, 1.0F);
	}
	else
	{
		val = dialog("TOTALIZER MODE IS DISABLED - DO YOU WANT TO ENABLE ?", -1.0F, -1.0F, 1.0F, 3);
		if(val == 1)
		{
			set_adjustment(TOTALIZER_ADJ, 1);
			if(get_adjustment(TOTALIZER_ADJ, &val))
				message_box("SETTING OF TOTALIZER MODE MODE FAILED", -1.0F, -1.0F, 1.0F);
			else
				message_box("TOTALIZER MODE MODE ENABLED", -1.0F, -1.0F, 1.0F);
		}
		else
			message_box("TOTALIZER MODE MODE NOT CHANGED", -1.0F, -1.0F, 1.0F);
	}
	wait_test_exit();
	return 0;
}
