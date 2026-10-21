/*
 * dialog.c - a question in a box with two buttons (0x80103998-): YES or OK on the left, NO or
 * CANCEL on the right, a white frame round the chosen one. Volume down/up (or left/right)
 * move it, test or service credit answer.
 */
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

#define DIALOG_YES	1		/* flags: the left button says YES, not OK */
#define DIALOG_CANCEL	4		/* the right button says CANCEL, not NO */

extern void	sleep(int);
extern int	snd_scall_bank(char *, int, int, int, int);
extern void	draw_button(char *, float, float, float);
extern void	clear_buttons(void);

void	clear_dialog(void);

/* 0x80103998: 1 for the left button, 2 for the right */
int dialog(char *s, float x, float y, float z, int flags)
{
	float	w, h, x2, y2, cx, ty;
	int	choice = 1, done;

	z += 5.0F;
	set_text_font(1);
	w = font2_height + font2_height + (float)get_string_width(s);
	h = font2_height + font1_height * 4.0F;
	if(x < 0.0F)
		x = (hres - w) / 2.0F;
	if(y < 0.0F)
		y = (vres - h) / 2.0F;
	x2 = x + w;
	cx = (x + x2) / 2.0F;
	y2 = y + h;
	ty = y2 - 8.0F - font1_height / 2.0F;
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_position((int)cx, (int)ty, z);
	z += 1.0F;
	set_string_id(0x4400);
	oprintf("%s", s);

	draw_rect(x + 2.0F, y + 2.0F, x2 - 4.0F, y2 - 4.0F, z, 0xff606060, 0x4400);
	z += 1.0F;
	draw_rect(x + 2.0F, y, x2 - 2.0F, y2 - 4.0F, z, 0xff404040, 0x4400);
	z += 1.0F;
	draw_rect(x, y, x2 - 2.0F, y2 - 2.0F, z, 0xff808080, 0x4400);

	if(flags & DIALOG_YES)
		draw_button("YES", x + w * 0.25F - 27.0F, y + font1_height * 0.5F, 1.0F);
	else
		draw_button("OK", x + w * 0.25F - 27.0F, y + font1_height * 0.5F, 1.0F);
	if(flags & DIALOG_CANCEL)
		draw_button("CANCEL", x + w * 0.75F - 27.0F, y + font1_height * 0.5F, 1.0F);
	else
		draw_button("NO", x + w * 0.75F - 27.0F, y + font1_height * 0.5F, 1.0F);

	while(sw_select() || sw_back())
		sleep(1);
	done = 0;
	do
	{
		float	bx = x + w * (choice != 1 ? 0.75F : 0.25F);
		float	by = y + font1_height * 0.5F;

		draw_thick_box(bx - 29.0F, by - 2.0F, bx + 25.0F, by + font1_height + vres / 384.0F * 8.0F,
			1.0F, 2.0F, 0xffffffff, 0x4401, 0);
		while(sw_left() || sw_right() || sw_up() || sw_down())
			sleep(1);
		for(;;)
		{
			if(sw_select() || sw_back())
			{
				done = 1;
				snd_scall_bank("ttest", 11, 180, 127, 0x8000);
				break;
			}
			if(sw_left() || sw_up())
			{
				if(choice != 2)
				{
					snd_scall_bank("ttest", 12, 180, 127, 0x8000);
					choice = 2;
				}
				break;
			}
			if(sw_right() || sw_down())
			{
				if(choice != 1)
				{
					snd_scall_bank("ttest", 13, 180, 127, 0x8000);
					choice = 1;
				}
				break;
			}
			sleep(1);
		}
		del1c(0x4401, -1);
	} while(!done);
	clear_dialog();
	while(sw_select() || sw_back())
		sleep(1);
	return choice;
}

/* 0x80104068 */
void clear_dialog(void)
{
	delete_multiple_strings(0x4400, -1);
	del1c(0x4400, -1);
	clear_buttons();
}
