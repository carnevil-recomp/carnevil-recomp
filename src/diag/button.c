/*
 * button.c - a dialog's button: its word in a 50-pixel grey bevelled box (0x801036b8-0x80103994).
 */
#include <string.h>
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

/* 0x801036b8: OK and YES are green, NO, CANCEL and ABORT red, anything else grey; x, y
   negative: centred */
void draw_button(char *s, float x, float y, float z)
{
	float	w, h, cx, cy;
	int	color;

	set_text_font(2);
	w = 50.0F;
	h = font2_height + font2_height;
	if(x < 0.0F)
		x = (hres - w) / 2.0F;
	if(y < 0.0F)
		y = (vres - h) / 2.0F;
	cx = (x + (x + w)) / 2.0F;
	cy = (y + (y + h)) / 2.0F;
	if(!strcmp(s, "OK") || !strcmp(s, "YES"))
		color = 0xff00ff00;
	else if(!strcmp(s, "NO") || !strcmp(s, "CANCEL") || !strcmp(s, "ABORT"))
		color = 0xffff0000;
	else
		color = 0xff404040;
	set_text_color(color);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_position((int)cx, (int)cy, z);
	set_string_id(0x4900);
	z += 1.0F;
	oprintf("%s", s);

	draw_rect(x + 2.0F, y + 2.0F, x + w - 4.0F, y + h - 4.0F, z, 0xff606060, 0x4900);
	z += 1.0F;
	draw_rect(x + 2.0F, y, x + w - 2.0F, y + h - 4.0F, z, 0xff404040, 0x4900);
	z += 1.0F;
	draw_rect(x, y, x + w - 2.0F, y + h - 2.0F, z, 0xff808080, 0x4900);
}

/* 0x80103964 */
void clear_buttons(void)
{
	delete_multiple_strings(0x4900, -1);
	del1c(0x4900, -1);
}
