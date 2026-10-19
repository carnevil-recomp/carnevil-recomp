/*
 * menu.c - DIAG.EXE's menu engine (0x80102568-0x801036b4): draws a menu as a bevelled box of
 * flat sprites with the items in it and the highlighted item's help line in a box at the
 * bottom of the screen, and runs it on the coin-door buttons: volume down/up move, test
 * chooses, service credit goes back. Holding a button repeats after 30 frames, then every 2.
 */
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

extern int	get_font_height(int);
extern void	sleep(int);
extern int	snd_scall_bank(char *, int, int, int, int);
extern void	func_8012a0a0(int);
extern void	func_80102534(void), func_80101844(void), func_80103964(void), func_80104068(void);

static void	draw_help(menu_item_t *, float, float);

/* a flat-coloured box of the given image's size, under a sprite id */
#define FLAT_SPRITE(ii, x, y, z, sid, color) \
	{ sprite_info_t *s = beginobj(ii, x, y, z, 0); s->id = sid; s->state.constant_color = color; }

static image_info_t	box_ii = { 0, 256.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F };
static image_info_t	frame_ii = { 0, 256.0F, 256.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F };
static image_info_t	bar_ii = { 0, 256.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F };
static image_info_t	help_ii = { 0, 256.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F };
static image_info_t	help_frame_ii = { 0, 256.0F, 256.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F };

/* 0x80102568: run a menu until an item exits it; a submenu runs inside, recursively */
int do_menu(menu_t *m)
{
	menu_item_t	*item;
	float		spacing = 2.0F;
	float		y, cx;
	int		max, w, i, done = 0;
	int		delay = 30;
	int		light = 0xff808080, dark = 0xff404040;

	do
	{
		set_text_font(0);
		box_ii.w = (float)get_string_width(m->title) + 8.0F;
		set_text_font(1);
		for(;;)
		{
			box_ii.h = font0_height + font0_height;
			item = m->items;
			m->count = 0;
			if(item->name)
			{
				max = get_string_width(item->name);
				while(item->name)
				{
					box_ii.h += font1_height * spacing;
					m->count++;
					item++;
					if(item->name && (w = get_string_width(item->name)) > max)
						max = w;
				}
			}
			if(!(vres * 0.875F < box_ii.h))
				break;
			spacing -= 0.1F;
		}
		max += 28;
		if((int)box_ii.w < max)
			box_ii.w = (float)max;
		box_ii.h += font0_height * 0.5F;
		if(m->x < 0.0F)
			m->x = (hres - box_ii.w) / 2.0F;
		if(m->y == -1.0F)
		{
			m->y = (vres + box_ii.h) / 2.0F;
			m->y -= box_ii.h;
			if(m->y < vres * 0.125F)
				m->y += vres * 0.125F - m->y;
			if(m->y + box_ii.h >= vres)
				m->y = vres - box_ii.h - 1.0F;
		}
		else if(m->y < -1.0F)
			m->y = vres - box_ii.h;

		bar_ii.w = box_ii.w - 20.0F;
		bar_ii.h = font1_height * spacing;

		/* the box, then its bevel: light at the top and left, dark below and right, then
		   the inner groove under the title */
		FLAT_SPRITE(&box_ii, m->x, m->y, 3.0F, 0x4d00, 0xff606060);

		frame_ii.w = box_ii.w + 4.0F;
		frame_ii.h = 2.0F;
		FLAT_SPRITE(&frame_ii, m->x - 2.0F, m->y + box_ii.h, 3.0F, 0x4d00, light);

		frame_ii.w = 2.0F;
		frame_ii.h = box_ii.h + 2.0F;
		FLAT_SPRITE(&frame_ii, m->x - 2.0F, m->y - 2.0F, 3.0F, 0x4d00, light);

		frame_ii.w = box_ii.w + 2.0F;
		frame_ii.h = 2.0F;
		FLAT_SPRITE(&frame_ii, m->x, m->y - 2.0F, 3.0F, 0x4d00, dark);

		frame_ii.w = 2.0F;
		frame_ii.h = box_ii.h + 2.0F;
		FLAT_SPRITE(&frame_ii, m->x + box_ii.w, m->y - 2.0F, 3.0F, 0x4d00, dark);

		frame_ii.w = box_ii.w - 16.0F;
		frame_ii.h = 2.0F;
		FLAT_SPRITE(&frame_ii, m->x + 8.0F, m->y + box_ii.h - (font0_height + font0_height), 3.0F, 0x4d00, dark);

		frame_ii.w = 2.0F;
		frame_ii.h = box_ii.h - (font0_height + font0_height) - font0_height * 0.5F + 2.0F;
		FLAT_SPRITE(&frame_ii, m->x + 8.0F, m->y + font0_height * 0.5F - 2.0F, 3.0F, 0x4d00, dark);

		frame_ii.w = box_ii.w - 18.0F;
		frame_ii.h = 2.0F;
		FLAT_SPRITE(&frame_ii, m->x + 10.0F, m->y + font0_height * 0.5F - 2.0F, 3.0F, 0x4d00, light);

		frame_ii.w = 2.0F;
		frame_ii.h = box_ii.h - (font0_height + font0_height) - font0_height * 0.5F + 2.0F;
		FLAT_SPRITE(&frame_ii, m->x + 8.0F + (box_ii.w - 18.0F), m->y + font0_height * 0.5F - 2.0F, 3.0F, 0x4d00, light);

		if(m->sel < 0)
			m->sel = 0;

		/* the title */
		y = m->y + box_ii.h - font0_height;
		if(get_font_height(0) & 1)
			y += 2.0F;
		set_text_font(0);
		set_string_id(0x4d00);
		set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
		cx = (m->x + (m->x + box_ii.w)) / 2.0F;
		set_text_color(0xff00ffff);
		set_text_position((int)cx, (int)y, 1.0F);
		oprintf(m->title);

		/* the items */
		y = m->y + box_ii.h - font0_height * spacing - font1_height;
		for(i = 0, item = m->items; item->name; item++)
		{
			if(i == m->sel)
			{
				FLAT_SPRITE(&bar_ii, m->x + 10.0F, y - bar_ii.h / 2.0F, 2.0F, 0x4d00, 0xff000080);
				if(item->hook)
					item->hook(i);
				if(item->help)
					draw_help(item, m->y, cx);
				set_text_color(0xffffff00);
			}
			else
				set_text_color(0xff00ff00);
			i++;
			set_text_font(1);
			set_text_position((int)((m->x + (m->x + box_ii.w)) / 2.0F), (int)y, 1.0F);
			set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
			oprintf(item->name);
			y -= font1_height * spacing;
		}

		/* wait for the buttons: let go of test and service first, then auto-repeat */
		sleep(5);
		while(sw_select() || sw_back())
			sleep(1);
		while(--delay && (sw_up() || sw_down()))
			sleep(1);
		if(sw_up() || sw_down() || sw_select() || sw_back())
			delay = 2;
		else
			delay = 30;
		while(!sw_up() && !sw_down() && !sw_select() && !sw_back())
			sleep(1);

		if(sw_up())
		{
			if(--m->sel < 0)
			{
				m->sel = m->count - 1;
				snd_scall_bank("ttest", 12, 180, 127, 0x8000);
			}
			else
				snd_scall_bank("ttest", 12, 180, 127, 0x8000);
		}
		else if(sw_down())
		{
			if(++m->sel >= m->count)
			{
				m->sel = 0;
				snd_scall_bank("ttest", 13, 180, 127, 0x8000);
			}
			else
				snd_scall_bank("ttest", 13, 180, 127, 0x8000);
		}
		else if(sw_select())
		{
			if(m->items[m->sel].kind == MENU_SUB)
			{
				snd_scall_bank("ttest", 10, 180, 127, 0x8000);
				clear_menu();
				done = do_menu((menu_t *)m->items[m->sel].target);
			}
			else if(m->items[m->sel].kind == MENU_EXIT)
			{
				snd_scall_bank("ttest", 11, 180, 127, 0x8000);
				m->sel = 0;
				return 0;
			}
			else if(m->items[m->sel].kind == MENU_FUNC)
			{
				snd_scall_bank("ttest", 10, 180, 127, 0x8000);
				if(m->items[m->sel].target)
				{
					while(sw_select())
						sleep(1);
					clear_menu();
					func_8012a0a0(0xff000000);
					done = ((int (*)(void))m->items[m->sel].target)();
					func_8012a0a0(0xff000000);
				}
			}
		}
		else if(sw_back())
		{
			snd_scall_bank("ttest", 11, 180, 127, 0x8000);
			m->sel = 0;
			return 0;
		}
		clear_menu();
	} while(!done);
	return 0;
}

/* 0x80103168 */
void clear_menu_box(void)
{
	delete_multiple_strings(0x4d00, -1);
	del1c(0x4d00, -1);
}

/* 0x80103198 */
void clear_menu_help(void)
{
	delete_multiple_strings(0x4800, -1);
	del1c(0x4800, -1);
}

/* 0x801031c8: the menu and everything a test may have left on the screen */
void clear_menu(void)
{
	delete_multiple_strings(0x4d00, -1);
	del1c(0x4d00, -1);
	delete_multiple_strings(0x4800, -1);
	del1c(0x4800, -1);
	func_80102534();
	func_80101844();
	func_80103964();
	func_80104068();
}

/* 0x80103230: the highlighted item's help line, in a box at the bottom of the screen centred
   under the menu (its bottom edge 2 pixels up), bevelled like the menu */
static void draw_help(menu_item_t *item, float menu_y, float cx)
{
	sprite_info_t	*s;
	float		x, tx;

	set_text_font(2);
	if((float)get_string_width(item->help) < hres - 32.0F)
	{
		help_ii.w = (float)get_string_width(item->help) + 32.0F;
		help_ii.h = font0_height + (font2_height + font2_height);
	}
	else
	{
		help_ii.w = 392.0F;
		help_ii.h = menu_y - 4.0F;
	}
	x = cx - help_ii.w / 2.0F;
	if(x < 2.0F)
		x = 2.0F;
	tx = (x + (x + help_ii.w)) / 2.0F;

	FLAT_SPRITE(&help_ii, x, 2.0F, 3.0F, 0x4800, 0xff0000c0);

	help_frame_ii.w = help_ii.w + 4.0F;
	help_frame_ii.h = 2.0F;
	FLAT_SPRITE(&help_frame_ii, x - 2.0F, help_ii.h + 2.0F, 3.0F, 0x4800, 0xff0000ff);

	help_frame_ii.w = 2.0F;
	help_frame_ii.h = help_ii.h + 2.0F;
	FLAT_SPRITE(&help_frame_ii, x - 2.0F, 0.0F, 3.0F, 0x4800, 0xff0000ff);

	help_frame_ii.w = help_ii.w + 2.0F;
	help_frame_ii.h = 2.0F;
	FLAT_SPRITE(&help_frame_ii, x, 0.0F, 3.0F, 0x4800, 0xff000080);

	help_frame_ii.w = 2.0F;
	help_frame_ii.h = help_ii.h + 2.0F;
	FLAT_SPRITE(&help_frame_ii, x + help_ii.w, 0.0F, 3.0F, 0x4800, 0xff000080);

	help_frame_ii.w = help_ii.w - 16.0F;
	help_frame_ii.h = 2.0F;
	FLAT_SPRITE(&help_frame_ii, x + 8.0F, help_ii.h + 2.0F - font0_height * 0.5F, 3.0F, 0x4800, 0xff000080);

	help_frame_ii.w = 2.0F;
	help_frame_ii.h = help_ii.h - font0_height * 0.5F - font0_height * 0.5F + 2.0F;
	FLAT_SPRITE(&help_frame_ii, x + 8.0F, font0_height * 0.5F + 2.0F - 2.0F, 3.0F, 0x4800, 0xff000080);

	help_frame_ii.w = help_ii.w - 18.0F;
	help_frame_ii.h = 2.0F;
	FLAT_SPRITE(&help_frame_ii, x + 10.0F, font0_height * 0.5F + 2.0F - 2.0F, 3.0F, 0x4800, 0xff0000ff);

	help_frame_ii.w = 2.0F;
	help_frame_ii.h = help_ii.h - font0_height * 0.5F - font0_height * 0.5F + 2.0F;
	FLAT_SPRITE(&help_frame_ii, x + 8.0F + (help_ii.w - 18.0F), font0_height * 0.5F + 2.0F - 2.0F, 3.0F, 0x4800, 0xff0000ff);

	set_text_position((int)tx, (int)((help_ii.h + 4.0F) / 2.0F), 1.0F);
	set_text_color(0xff00ffff);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_string_id(0x4800);
	oprintf(item->help);
}
