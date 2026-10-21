/*
 * title.c - a line of text in a bevelled blue box (0x80101618-0x801018c4), used for the
 * screens' titles.
 */
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

extern int	get_font_height(int);
extern void	draw_rect(float, float, float, float, float, int, int);

/* 0x80101618: s in font 0 at x, y (negative: centred), framed */
void draw_text_box(char *s, float x, float y, float z)
{
	float	w, h, x2, y2, cx, ty;

	set_text_font(0);
	w = (float)get_string_width(s);
	h = font0_height + font0_height;
	w += h;
	if(x < 0.0F)
		x = (hres - w) / 2.0F;
	if(y < 0.0F)
		y = (vres - h) / 2.0F;
	y2 = y + h;
	x2 = x + w;
	ty = (y + y2) / 2.0F - 2.0F;
	cx = (x + x2) / 2.0F;
	if(get_font_height(0) & 1)
		ty += 2.0F;
	set_text_color(0xff00ff00);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_position((int)cx, (int)ty, z);
	set_string_id(0x5400);
	oprintf("%s", s);

	z += 1.0F;
	draw_rect(x + 2.0F, y + 2.0F, x2 - 4.0F, y2 - 4.0F, z, 0xff0000c0, 0x5400);
	z += 1.0F;
	draw_rect(x + 2.0F, y, x2 - 2.0F, y2 - 4.0F, z, 0xff000080, 0x5400);
	z += 1.0F;
	draw_rect(x, y, x2 - 2.0F, y2 - 2.0F, z, 0xff0000ff, 0x5400);
}

/* 0x80101844 */
void clear_text_box(void)
{
	delete_multiple_strings(0x5400, -1);
	del1c(0x5400, -1);
}

/* 0x80101874: a screen's title, centred near the top */
void draw_title(char *s)
{
	draw_text_box(s, -1.0F, vres - (font0_height + font0_height + 4.0F), 1.0F);
}
