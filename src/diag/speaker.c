/*
 * speaker.c - the start-up button legend and SPEAKER TEST (0x8011b4d8-0x8011b80c): the
 * left, centre and right channels' test sound repeated until a button, the two test tones and
 * the tune played once.
 */
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

extern void	sleep(int);
extern int	snd_scall_bank(char *, int, int, int, int);
extern char	*snd_bank_name;			/* 0x802ef5b4 */
extern int	volume_call;			/* 0x802fdc08 */

/* 0x801680e0 */
char	*startup_help[] = {
	"VOL UP - MOVE UP",
	"VOL DOWN - MOVE DOWN",
	"TEST - SELECT",
	"CREDIT - EXIT",
	"",
	"",
	"5 SECONDS TO DIAG ENTRY",
	0
};

/* 0x8011b4d8: main_proc's legend of the buttons, while it waits to enter the menus */
void draw_startup_help(void)
{
	char	**p = startup_help;
	float	y = vres * 0.6F;

	set_text_justification_mode(2);
	set_text_color(0xff00ffff);
	set_string_id(0);
	set_text_font(1);
	while(*p)
	{
		set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
		oprintf(*p++);
		y -= font1_height * 2.0F;
	}
}

int	speaker_test(int);

char	help_left_channel[] = "TEST LEFT CHANNEL";
char	help_center_channel[] = "TEST CENTER CHANNEL";
char	help_right_channel[] = "TEST RIGHT CHANNEL";
char	help_100hz[] = "100 HZ TEST TONE";
char	help_1khz[] = "1 KHZ TEST TONE";
char	help_tune[] = "PLAY TUNE";
char	help_speaker_exit[] = "RETURN TO PREVIOUS MENU";

menu_item_t	speaker_items[] = {
	{ "LEFT CHANNEL", MENU_FUNC, speaker_test, help_left_channel, 0 },
	{ "CENTER CHANNEL", MENU_FUNC, speaker_test, help_center_channel, 0 },
	{ "RIGHT CHANNEL", MENU_FUNC, speaker_test, help_right_channel, 0 },
	{ "100 HZ TONE", MENU_FUNC, speaker_test, help_100hz, 0 },
	{ "1 KHZ TONE", MENU_FUNC, speaker_test, help_1khz, 0 },
	{ "PLAY TUNE", MENU_FUNC, speaker_test, help_tune, 0 },
	{ "EXIT", MENU_EXIT, 0, help_speaker_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	speaker_menu = { "SPEAKER TEST", -1.0F, -1.0F, 0, 0, speaker_items };	/* 0x80168224 */

/* the test bank's sounds for the first five items (the tune is the volume sound) */
int	speaker_calls[] = { 7, 8, 9, 1, 4 };
char	speaker_names[][15] = {
	"LEFT CHANNEL", "CENTER CHANNEL", "RIGHT CHANNEL", "100 HZ TONE", "1 KHZ TONE", "TUNE"
};

/* 0x8011b5a8: menu item n; a channel's sound is played again every 80 ticks until a button */
int speaker_test(int n)
{
	int	done, i;

	draw_title("SPEAKER TEST");
	set_text_font(1);
	set_text_color(-1);
	set_text_justification_mode(10);
	set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
	oprintf(speaker_names[n]);
	if(n < 3)
	{
		message_box("PRESS CREDIT OR TEST TO EXIT", -1.0F, 5.0F, 1.0F);
		do
		{
			snd_scall_bank("ttest", speaker_calls[n], 180, 127, 0x8000);
			i = 80;
			do
			{
				done = 0;
				if(sw_select() || sw_back())
					done = 1;
				if(done)
					break;
				i--;
				sleep(1);
			} while(i);
			snd_scall_bank("ttest", 11, 180, 127, 0x8000);
			while(done && (sw_select() || sw_back()))
				sleep(1);
		} while(!done);
	}
	else if(n == 5)
	{
		snd_scall_bank(snd_bank_name, volume_call, 180, 127, 0x8000);
		wait_test_exit();
	}
	else
	{
		snd_scall_bank("ttest", speaker_calls[n], 180, 127, 0x8000);
		wait_test_exit();
	}
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	return(0);
}
