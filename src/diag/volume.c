/*
 * volume.c - SET VOLUME LEVEL (0x80110c48-0x801110b4): a red bar for the level (0-255) in a
 * green frame, set with the volume buttons while a sample sound plays.
 */
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include <goose/adjust.h>
#include "diag.h"

extern int	get_adjustment(int, int *);
extern int	set_adjustment(int, int);
extern void	sleep(int);
extern int	snd_scall_bank(char *, int, int, int, int);
extern void	snd_master_volume(int);
extern char	*snd_bank_name;
extern int	volume_call;			/* 0x802fdc08: the sound played to set it by */
extern int	(*sw_vol_up)(void);		/* 0x80167d8c */
extern int	(*sw_vol_down)(void);		/* 0x80167d90 */

static image_info_t	volume_ii = { 0, 32.0F, 32.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F };

/* 0x80110c48 */
int volume_level(void)
{
	sprite_info_t	*s;
	int	vol = 128;

	draw_title("SET VOLUME LEVEL");
	message_box("PRESS TEST OR CREDIT TO EXIT", -1.0F, 5.0F, 1.0F);
	snd_scall_bank(snd_bank_name, volume_call, 180, 127, 0x8000);
	for(;;)
	{
		delete_multiple_strings(0x4f00, -1);
		del1c(0x4f00, -1);
		if(get_adjustment(VOLUME_LEVEL_ADJ, &vol))
		{
			set_adjustment(VOLUME_LEVEL_ADJ, 127);
			vol = 127;
		}
		volume_ii.w = hres * 0.8F * ((float)vol / 255.0F);
		volume_ii.h = vres * 0.2F;
		s = beginobj(&volume_ii, hres * 0.1F, (vres - volume_ii.h) / 2.0F, 1.0F, 0);
		s->state.constant_color = 0xffff0000;
		s->id = 0x4f00;
		draw_thick_box(hres * 0.1F - 3.0F, (vres - volume_ii.h) / 2.0F - 3.0F,
			hres * 0.9F + 4.0F, (vres + volume_ii.h) / 2.0F + 3.0F,
			1.0F, 3.0F, 0xff00ff00, 0x4f00, 0);
		set_text_font(1);
		set_text_color(-1);
		set_text_position((int)(hres / 2.0F), (int)(vres * 0.25F), 1.0F);
		set_text_justification_mode(2);
		set_string_id(0x4f00);
		oprintf("VOL UP/DOWN TO ADJUST VOLUME");
		set_text_color(0xffffff00);
		set_text_position((int)(hres * 0.1F), (int)((vres + volume_ii.h) / 2.0F + 8.0F), 1.0F);
		oprintf("MIN");
		set_text_position((int)(hres * 0.9F), (int)((vres + volume_ii.h) / 2.0F + 8.0F), 1.0F);
		oprintf("MAX");
		do
			sleep(1);
		while(!sw_select() && !sw_back() && !sw_vol_up() && !sw_vol_down());
		if(sw_select() || sw_back())
			break;
		if(sw_vol_up())
		{
			if(++vol > 255)
				vol = 255;
		}
		else if(sw_vol_down())
		{
			if(--vol < 0)
				vol = 0;
		}
		set_adjustment(VOLUME_LEVEL_ADJ, vol);
		snd_master_volume(vol);
	}
	snd_scall_bank("ttest", 11, 180, 127, 0x8000);
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	return 0;
}
