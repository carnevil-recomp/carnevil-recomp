/*
 * attract.c - ATTRACT SOUND (0x80110a50-0x80110c44): whether the game plays sounds in its
 * attract mode.
 */
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include <goose/adjust.h>
#include "diag.h"

extern int	get_adjustment(int, int *);
extern int	set_adjustment(int, int);
extern int	graphical_printf(char *, char *, ...);
extern int	dialog(char *, float, float, float, int);

/* 0x80110a50 */
int attract_sounds(void)
{
	char	buf[40];
	int	val;

	if(get_adjustment(ATTRACT_SOUND_ADJ, &val))
	{
		set_text_font(0);
		set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		set_text_color(0xffff0000);
		oprintf("THE CMOS SETTING OF ATTRACT MODE SOUNDS");
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F - (font0_height + font0_height)), 1.0F);
		oprintf("INVALID - RESETTING TO ATTRACT SOUNDS ON");
		set_adjustment(ATTRACT_SOUND_ADJ, 1);
		wait_test_exit();
		delete_multiple_strings(0, 0);
		del1c(0, 0);
		get_adjustment(ATTRACT_SOUND_ADJ, &val);
	}
	val = dialog("ENABLE ATTRACT MODE SOUNDS ?", -1.0F, -1.0F, 1.0F, 3);
	set_adjustment(ATTRACT_SOUND_ADJ, val == 1);
	if(get_adjustment(ATTRACT_SOUND_ADJ, &val))
		message_box("SETTING ATTRACT MODE SOUND FAILED", -1.0F, -1.0F, 1.0F);
	else
	{
		graphical_printf(buf, "ATTRACT SOUNDS %s", val == 1 ? "ENABLED" : "DISABLED");
		message_box(buf, -1.0F, -1.0F, 1.0F);
	}
	wait_test_exit();
	return 0;
}
