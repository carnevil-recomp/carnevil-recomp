/*
 * draw.c - flat-coloured lines, boxes and the message box, all as sprites
 * (0x80101968-0x80102564). Each returns its sprite (0 if none), coloured and under the given id.
 */
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

double	fabs(double);

static const image_info_t	hline_ii = { 0, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F };
static const image_info_t	vline_ii = { 0, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F };

/* 0x80101968: from x1 to x2 at y */
inline sprite_info_t *draw_hline(float x1, float x2, float y, float z, int color, int id)
{
	image_info_t	ii = hline_ii;
	sprite_info_t	*s;

	ii.w = fabs(x1 - x2);
	if(x2 < x1)
		x1 = x2;
	s = beginobj(&ii, x1, y, z, 0);
	if(s)
	{
		s->state.constant_color = color;
		s->id = id;
	}
	return s;
}

/* 0x80101a24: from y1 to y2 at x */
inline sprite_info_t *draw_vline(float x, float y1, float y2, float z, int color, int id)
{
	image_info_t	ii = vline_ii;
	sprite_info_t	*s;

	ii.h = fabs(y1 - y2);
	if(y2 < y1)
		y1 = y2;
	s = beginobj(&ii, x, y1, z, 0);
	if(s)
	{
		s->state.constant_color = color;
		s->id = id;
	}
	return s;
}

/* 0x80101ae0: a vertical or horizontal line (anything else draws nothing). Its corners come
   x1, x2, y1, y2, though draw_box passes them x1, y1, x2, y2 */
sprite_info_t *draw_line(float x1, float x2, float y1, float y2, float z, int color, int id)
{
	if(x1 == x2)
		return draw_vline(x1, y1, y2, z, color, id);
	if(y1 == y2)
		return draw_hline(x1, x2, y1, z, color, id);
	return 0;
}

/* 0x80101c18: draw_hline, thick pixels high */
inline sprite_info_t *draw_thick_hline(float x1, float x2, float y, float z, float thick, int color, int id)
{
	image_info_t	ii = hline_ii;
	sprite_info_t	*s;

	ii.h = thick;
	ii.w = fabs(x1 - x2);
	if(x2 < x1)
		x1 = x2;
	s = beginobj(&ii, x1, y, z, 0);
	if(s)
	{
		s->state.constant_color = color;
		s->id = id;
	}
	return s;
}

/* 0x80101cdc: draw_vline, thick pixels wide */
inline sprite_info_t *draw_thick_vline(float x, float y1, float y2, float z, float thick, int color, int id)
{
	image_info_t	ii = vline_ii;
	sprite_info_t	*s;

	ii.w = thick;
	ii.h = fabs(y1 - y2);
	if(y2 < y1)
		y1 = y2;
	s = beginobj(&ii, x, y1, z, 0);
	if(s)
	{
		s->state.constant_color = color;
		s->id = id;
	}
	return s;
}

/* 0x80101da0: a thick vertical or horizontal line from x1, y1 to x2, y2 (by whole pixels) */
sprite_info_t *draw_thick_line(float x1, float y1, float x2, float y2, float z, float thick, int color, int id)
{
	if((int)x1 == (int)x2)
		return draw_thick_vline(x1, y1, y2, z, thick, color, id);
	if((int)y1 == (int)y2)
		return draw_thick_hline(x1, x2, y1, z, thick, color, id);
	return 0;
}

/* 0x80101ef8: a box's outline; its four sprites go to sp[0..3] if sp is given */
void draw_box(float x1, float y1, float x2, float y2, float z, int color, int id, sprite_info_t **sp)
{
	if(sp)
	{
		*sp++ = draw_line(x1, y1, x2, y1, z, color, id);
		*sp++ = draw_line(x1, y2, x2, y2, z, color, id);
		*sp++ = draw_line(x1, y1, x1, y2, z, color, id);
		*sp++ = draw_line(x2, y1, x2, y2, z, color, id);
	}
	else
	{
		draw_line(x1, y1, x2, y1, z, color, id);
		draw_line(x1, y2, x2, y2, z, color, id);
		draw_line(x1, y1, x1, y2, z, color, id);
		draw_line(x2, y1, x2, y2, z, color, id);
	}
}

/* 0x80102088: draw_box with thick sides, inside the corners */
void draw_thick_box(float x1, float y1, float x2, float y2, float z, float thick, int color, int id, sprite_info_t **sp)
{
	if(sp)
	{
		*sp++ = draw_thick_line(x1, y1, x2, y1, z, thick, color, id);
		*sp++ = draw_thick_line(x1, y2 - thick, x2, y2 - thick, z, thick, color, id);
		*sp++ = draw_thick_line(x1, y1, x1, y2, z, thick, color, id);
		*sp++ = draw_thick_line(x2 - thick, y1, x2 - thick, y2, z, thick, color, id);
	}
	else
	{
		draw_thick_line(x1, y1, x2, y1, z, thick, color, id);
		draw_thick_line(x1, y2 - thick, x2, y2 - thick, z, thick, color, id);
		draw_thick_line(x1, y1, x1, y2, z, thick, color, id);
		draw_thick_line(x2 - thick, y1, x2 - thick, y2, z, thick, color, id);
	}
}

/* 0x80102254: a filled rectangle from x1, y1 to x2, y2 inclusive */
sprite_info_t *draw_rect(float x1, float y1, float x2, float y2, float z, int color, int id)
{
	image_info_t	ii = { 0, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F };
	sprite_info_t	*s;

	ii.w = x2 - x1 + 1.0F;
	ii.h = y2 - y1 + 1.0F;
	s = beginobj(&ii, x1, y1, z, 0);
	if(s)
	{
		s->id = id;
		s->state.constant_color = color;
	}
	return s;
}

/* 0x80102318: s in font 2, cyan, in a bevelled blue box at x, y (negative: centred) */
void message_box(char *s, float x, float y, float z)
{
	float	fh, w, h, x2, y2, cx, cy;

	set_text_font(2);
	fh = font2_height;
	w = (float)get_string_width(s) + (fh + fh);
	h = fh * 3.0F;
	if(x < 0.0F)
		x = (hres - w) / 2.0F;
	x2 = x + w;
	if(y < 0.0F)
		y = (vres - h) / 2.0F;
	cx = (x + x2) / 2.0F;
	y2 = y + h;
	cy = (y + y2) / 2.0F;
	set_text_color(0xff00ffff);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_position((int)cx, (int)cy, z);
	z += 1.0F;
	set_string_id(0x4900);
	oprintf("%s", s);

	draw_rect(x + 2.0F, y + 2.0F, x2 - 4.0F, y2 - 4.0F, z, 0xff0000c0, 0x4900);
	z += 1.0F;
	draw_rect(x + 2.0F, y, x2 - 2.0F, y2 - 4.0F, z, 0xff000080, 0x4900);
	z += 1.0F;
	draw_rect(x, y, x2 - 2.0F, y2 - 2.0F, z, 0xff0000ff, 0x4900);
}

/* 0x80102534 */
void clear_message_box(void)
{
	delete_multiple_strings(0x4900, -1);
	del1c(0x4900, -1);
}
