/*
 * custprice.c - CUSTOM PRICING (0x80111c88-0x80114848), of the PRICING menu: pricing modes
 * kept as NAME.PRC files on the disk (the pricing_t then its 80 character info text), to add,
 * delete, edit and select, and the editor and text entry they use. Selecting one sets the
 * CUSTOM continent; SELECT CURRENCY sets a mode's currency.
 * Built at -O, not DIAG's usual -O2: no scheduling, strength reduction or cse across jumps.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dir.h>
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include <goose/adjust.h>
#include "diag.h"
#include "pricing.h"

#define	CUSTOM_CONTINENT	6
#define	INFO_LEN		80	/* the 20 PRICING_INFO_STRING_ADJ words */

extern int	get_dip_coin_current(void);
extern int	get_adjustment(int, int *);
extern int	set_adjustment(int, int);
extern int	graphical_printf(char *, char *, ...);
extern int	dialog(char *, float, float, float, int);
extern void	draw_text_box(char *, float, float, float);
extern void	sleep(int);
extern int	snd_scall_bank(char *, int, int, int, int);
extern float	fsqrt(float);
extern int	set_pricing(int);		/* pricing.c */
extern int	pricing_continent;		/* pricing_menu.c */
extern int	(*sw_vol_up)(void);		/* 0x80167d8c */
extern int	(*sw_vol_down)(void);		/* 0x80167d90 */

int	choose_custom_pricing(int);
int	delete_all_custom_pricing(void);
int	delete_custom_pricing(int);
int	add_custom_pricing(void);
int	edit_custom_pricing(int);
int	select_custom_pricing(int);
int	set_currency(int);

static int	custom_modified;		/* 0x80173228: a .prc file was written or deleted */
static int	currency;			/* 0x8017322c: the edited mode's */

int	num_prc = 0;				/* the .prc files in prc_items */

/* DELETE, EDIT and SELECT pick the file from prc_menu */
char	*prc_actions[] = { "ADD", "DELETE", "EDIT", "DELETE ALL", "SELECT" };
int	(*prc_action_funcs[])(int) = { 0, delete_custom_pricing, edit_custom_pricing, 0, select_custom_pricing };

char	help_custom_add[] = "ADD A NEW CUSTOM PRICING ENTRY";
char	help_custom_delete[] = "DELETE AN EXISTING CUSTOM PRICING ENTRY";
char	help_custom_edit[] = "EDIT AN EXISTING CUSTOM PRICING ENTRY";
char	help_custom_delete_all[] = "DELETE ALL CUSTOM PRICING ENTRIES";
char	help_custom_select[] = "SELECT AN EXISTING CUSTOM PRICING ENTRY";
char	help_custom_exit[] = "RETURN TO PRICING MENU";

/* without .prc files */
menu_item_t	custom_items_none[] = {
	{ "ADD", MENU_FUNC, add_custom_pricing, help_custom_add, 0 },
	{ "EXIT", MENU_EXIT, 0, help_custom_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_item_t	custom_items[] = {
	{ "ADD", MENU_FUNC, add_custom_pricing, help_custom_add, 0 },
	{ "DELETE", MENU_FUNC, choose_custom_pricing, help_custom_delete, 0 },
	{ "EDIT", MENU_FUNC, choose_custom_pricing, help_custom_edit, 0 },
	{ "DELETE ALL", MENU_FUNC, delete_all_custom_pricing, help_custom_delete_all, 0 },
	{ "SELECT", MENU_FUNC, select_custom_pricing, help_custom_select, 0 },
	{ "EXIT", MENU_EXIT, 0, help_custom_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	custom_menu = { "CUSTOM PRICING", -1.0F, -1.0F, 0, 0, custom_items_none };

/* a .prc file each (the name without .prc), EXIT and the end */
menu_item_t	prc_items[10] = {
	{ "EXIT", MENU_EXIT, 0, 0, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	prc_menu = { 0, -1.0F, -1.0F, 0, 0, prc_items };

/* in currency_names' order (pricing.c) */
menu_item_t	currency_items[] = {
	{ "DOLLAR", MENU_FUNC, set_currency, 0, 0 },
	{ "POUND", MENU_FUNC, set_currency, 0, 0 },
	{ "GUILDER", MENU_FUNC, set_currency, 0, 0 },
	{ "SCHILLING", MENU_FUNC, set_currency, 0, 0 },
	{ "FRANC", MENU_FUNC, set_currency, 0, 0 },
	{ "KRONE", MENU_FUNC, set_currency, 0, 0 },
	{ "MARKKA", MENU_FUNC, set_currency, 0, 0 },
	{ "DUETCHMARK", MENU_FUNC, set_currency, 0, 0 },
	{ "LIRE", MENU_FUNC, set_currency, 0, 0 },
	{ "FORINT", MENU_FUNC, set_currency, 0, 0 },
	{ "HFI", MENU_FUNC, set_currency, 0, 0 },
	{ "PESETA", MENU_FUNC, set_currency, 0, 0 },
	{ "YEN", MENU_FUNC, set_currency, 0, 0 },
	{ "DONE", MENU_EXIT, 0, 0, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	currency_menu = { "SELECT CURRENCY", -1.0F, -1.0F, 0, 0, currency_items };

void	build_prc_menu(void);
void	free_prc_menu(void);

/* 0x80111c88: the CUSTOM PRICING menu */
int custom_pricing(void)
{
	int	continent;
	int	ret;

	custom_modified = 0;
	get_adjustment(CONTINENT_ADJ, &continent);
	build_prc_menu();
	ret = do_menu(&custom_menu);
	if(custom_modified && continent == CUSTOM_CONTINENT)
	{
		del1c(0, 0);
		delete_multiple_strings(0, 0);
		message_box("CUSTOM PRICING MODIFIED - PRICING MODE RESET", -1.0F, -1.0F, 1.0F);
		set_adjustment(CONTINENT_ADJ, 0);
		set_adjustment(COUNTRY_ADJ, 0);
		set_adjustment(PRICE_ADJ, 0);
		while(sw_select() || sw_back())
			sleep(1);
		wait_test_exit();
	}
	return ret;
}

/* 0x80111d94: DELETE, EDIT (item n): the file to do it to */
int choose_custom_pricing(int n)
{
	char	buf[64];
	int	i;

	for(i = 0; i < num_prc; i++)
	{
		graphical_printf(buf, "%s ENTRY \x1b%dc%s", prc_actions[n], 0xffffff00, prc_items[i].name);
		prc_items[i].help = malloc(strlen(buf) + 1);
		if(prc_items[i].help)
			graphical_printf(prc_items[i].help, "%s", buf);
		prc_items[i].target = prc_action_funcs[n];
		prc_items[i].kind = MENU_FUNC;
	}
	graphical_printf(buf, "%s CUSTOM PRICE ENTRY", prc_actions[n]);
	prc_menu.title = buf;
	prc_menu.sel = 0;
	do_menu(&prc_menu);
	for(i = 0; i < num_prc; i++)
	{
		if(prc_items[i].help)
			free(prc_items[i].help);
	}
	return 0;
}

/* 0x80111f40: prc_items from the .prc files (up to 9), and the menu with or without them */
void build_prc_menu(void)
{
	struct ffblk	ff;
	char		*p;
	unsigned int	i;

	if(findfirst("*.prc", &ff, 0))
	{
		custom_menu.items = custom_items_none;
		num_prc = 0;
	}
	else
	{
		i = 0;
		custom_menu.items = custom_items;
		do
		{
			if((p = strchr(ff.ff_name, '.')))
				*p = 0;
			prc_items[i].name = malloc(strlen(ff.ff_name) + 1);
			if(!prc_items[i].name)
			{
				fprintf(stderr, "Can not allocate memory for custom price menu item string\r\n");
				return;
			}
			memcpy(prc_items[i].name, ff.ff_name, strlen(ff.ff_name) + 1);
			prc_items[i].kind = MENU_FUNC;
			prc_items[i].target = 0;
			prc_items[i].help = 0;
			prc_items[i].hook = 0;
			i++;
		} while(i < 9 && !findnext(&ff));
		num_prc = i;
		prc_items[i].name = "EXIT";
		prc_items[i].kind = MENU_EXIT;
		prc_items[i].target = 0;
		prc_items[i].help = "RETURN TO CUSTOM PRICING MENU";
		prc_items[i].hook = 0;
		prc_items[i + 1].name = 0;
		prc_items[i + 1].kind = -1;
		prc_items[i + 1].target = 0;
		prc_items[i + 1].help = 0;
		prc_items[i + 1].hook = 0;
	}
	custom_menu.sel = 0;
	custom_menu.y = custom_menu.x = -1.0F;
}

/* 0x8011217c */
void free_prc_menu(void)
{
	int	i;

	for(i = 0; i < num_prc; i++)
	{
		if(prc_items[i].name)
			free(prc_items[i].name);
	}
	num_prc = 0;
	prc_items[0].name = "EXIT";
	prc_items[0].kind = MENU_EXIT;
	prc_items[0].target = 0;
	prc_items[0].help = "RETURN TO CUSTOM PRICING MENU";
	prc_items[0].hook = 0;
	prc_items[1].name = 0;
	prc_items[1].kind = -1;
	prc_items[1].target = 0;
	prc_items[1].help = 0;
	prc_items[1].hook = 0;
}

/* 0x80112238: DELETE ALL */
int delete_all_custom_pricing(void)
{
	struct ffblk	ff;
	char		buf[64];

	graphical_printf(buf, "\x1b%dcDELETE ALL CUSTOM PRICING", 0xffff0000);
	if(dialog(buf, -1.0F, -1.0F, 1.0F, 3) != 1)
		return 0;
	if(!findfirst("*.prc", &ff, 0))
	{
		do
			remove(ff.ff_name);
		while(!findnext(&ff));
	}
	free_prc_menu();
	build_prc_menu();
	custom_modified = 1;
	return 0;
}

/* 0x801122ec: DELETE, of file n */
int delete_custom_pricing(int n)
{
	char	buf[64];
	int	ret;

	graphical_printf(buf, "\x1b%dcDELETE \x1b%dc%s\x1b%dc - ARE YOU SURE ?", 0xff00ffff, 0xffffff00,
		prc_items[n].name, 0xff00ffff);
	ret = dialog(buf, -1.0F, -1.0F, 1.0F, 3);
	if(ret != 1)
	{
		graphical_printf(buf, "DELETE OF \x1b%dc%s\x1b%dc ABORTED", 0xffffff00, prc_items[n].name, 0xffffffff);
		message_box(buf, -1.0F, -1.0F, 1.0F);
		wait_test_exit();
		return 0;
	}
	graphical_printf(buf, "%s.PRC", prc_items[n].name);
	remove(buf);
	graphical_printf(buf, "CUSTOM PRICING \x1b%dc%s\x1b%dc DELETED", 0xffffff00, prc_items[n].name, 0xff00ffff);
	message_box(buf, -1.0F, -1.0F, 1.0F);
	wait_test_exit();
	free_prc_menu();
	build_prc_menu();
	custom_modified = ret;
	return -1;
}

static int	unused;				/* 0x80173230: never referenced (DIAG.EXE's bss has the room) */
static char	entry[128];			/* 0x80173238: text_entry's text */

/* 0x8011246c: a line of up to max characters picked from chars, laid out in a square (an odd
   number of columns); its last two are the delete and done glyphs. 0 if left empty. */
char *text_entry(char *title, char *chars, int max, char *init)
{
	int	len, delay, n, cols, sel;
	float	h, w, x, y;

	len = 0;
	delay = 30;
	if(max < 0 || max > 128)
		max = 127;
	if(!init)
		entry[0] = 0;
	else
	{
		strcpy(entry, init);
		len = strlen(init);
	}
	draw_text_box(title, -1.0F, vres - 40.0F, 1.0F);
	message_box("VOL UP/DOWN TO MOVE - BUTTON TO SELECT", -1.0F, 5.0F, 1.0F);
	h = font0_height + 8.0F;
	set_text_font(0);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_string_id(0x4f00);
	w = get_string_width("M") + 8.0F;
	n = strlen(chars);
	cols = fsqrt(n);
	if((float)cols / 2.0F == cols / 2)
		cols++;
	for(sel = 0; sel < n; sel++)
	{
		x = hres / 2.0F + ((float)(sel % cols) - (cols - 5) / 2) * w - (w + w);
		y = vres / 2.0F - (sel / cols) * h + h * 2.5F;
		set_text_position((int)x, (int)y, 1.0F);
		oprintf("%c", chars[sel]);
	}
	sel = 0;
	for(;;)
	{
		del1c(0x4f01, -1);
		delete_multiple_strings(0x4f01, -1);
		if(len >= max)
		{
			set_text_position((int)(hres / 2.0F), (int)(vres * 0.8F), 1.0F);
			set_text_color(0xffffffff);
			set_text_font(1);
			set_text_justification_mode(HJUST_CENTER|VJUST_TOP);
			set_string_id(0x4f01);
			oprintf("CHARACTER LIMIT REACHED");
		}
		x = hres / 2.0F + ((float)(sel % cols) - (cols - 5) / 2) * w - (w + w);
		y = vres / 2.0F - (sel / cols) * h + h * 2.5F;
		x -= w / 2.0F;
		y -= h / 2.0F;
		x -= 1.0F;
		y += 1.0F;
		draw_thick_box(x, y, x + w, y + h, 1.0F, 2.0F, 0xffffffff, 0x4f01, 0);
		set_text_position((int)(hres / 2.0F), (int)(vres * 0.15F), 1.0F);
		set_text_color(0xffffffff);
		set_text_font(0);
		set_text_justification_mode(HJUST_CENTER);
		set_string_id(0x4f01);
		oprintf("%s", entry);
		sleep(1);
		while(sw_select() || sw_back())
			sleep(1);
		while(--delay && (sw_up() || sw_down() || sw_right() || sw_left()))
			sleep(1);
		if(sw_up() || sw_down() || sw_left() || sw_right() || sw_select() || sw_back())
			delay = 5;
		else
			delay = 30;
		while(!sw_up() && !sw_down() && !sw_left() && !sw_right() && !sw_select() && !sw_back())
			sleep(1);
		if(sw_up())
		{
			if(--sel < 0)
				sel = 0;
			else
				snd_scall_bank("ttest", 12, 180, 127, 0x8000);
		}
		else if(sw_down())
		{
			if(++sel > n - 1)
				sel = n - 1;
			else
				snd_scall_bank("ttest", 12, 180, 127, 0x8000);
		}
		else if(sw_select() || sw_back())
		{
			if(sel == n - 1)
			{
				snd_scall_bank("ttest", 11, 180, 127, 0x8000);
				break;
			}
			if(sel == n - 2)
			{
				if(--len < 0)
					len = 0;
				entry[len] = 0;
				snd_scall_bank("ttest", 10, 180, 127, 0x8000);
			}
			else if(sel < n - 2)
			{
				entry[len] = chars[sel];
				if(++len >= max)
					len = max;
				entry[len] = 0;
				snd_scall_bank("ttest", 10, 180, 127, 0x8000);
			}
		}
	}
	del1c(0, 0);
	delete_multiple_strings(0, 0);
	if(!strlen(entry))
		return 0;
	return entry;
}

/* an editor item: its name and value, where they are shown, and the value's range */
typedef struct price_field {
	char	*name;
	float	x, y;			/* the name */
	float	vx, vy;			/* the value */
	int	min, max;
	int	val;
	int	unused1;
	int	type;			/* FIELD_NUMBER, FIELD_YESNO */
	int	unused2;
} price_field_t;
#define	FIELD_NUMBER	0
#define	FIELD_YESNO	2

/* pricing_t's first 18, two to a line */
price_field_t	price_fields[] = {
	{ "LEFT UNITS", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "RIGHT UNITS", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "CENTER UNITS", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "EXTRA UNITS", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "DBV UNITS", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "UNITS/CREDIT", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "UNITS/BONUS", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "MIN UNITS", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "CREDITS TO START", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "CREDITS TO CONT.", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "MAX CREDITS", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "COINS/BILL", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "SHOW FRACTIONS", 0.0F, 0.0F, 0.0F, 0.0F, 0, 1, 0, 0, FIELD_YESNO },
	{ "LEFT COUNT", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "RIGHT COUNT", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "CENTER COUNT", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "EXTRA COUNT", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 },
	{ "DBV COUNT", 0.0F, 0.0F, 0.0F, 0.0F, 0, 100 }
};
#define	NUM_PRICE_FIELDS	(sizeof(price_fields) / sizeof(price_field_t))

static char	info_lines[4][32];		/* 0x801732b8: the info text's lines (three are shown) */

/* 0x80112c90: p's info text, a line each from y down */
void show_price_info(float y, pricing_t *p)
{
	char	*d, *s;
	int	i, n;
	float	lh;

	n = 1;
	d = info_lines[0];
	info_lines[0][0] = 0;
	info_lines[1][0] = 0;
	info_lines[2][0] = 0;
	lh = get_font_height(0) * 1.2F;
	set_text_font(0);
	for(s = p->info; *s; s++)
	{
		if(*s == '\n')
		{
			*d = 0;
			d = info_lines[n++];
		}
		else
			*d++ = *s;
	}
	for(i = 0; i < n; i++)
	{
		set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
		oprintf("\x1b%dj\x1b%dc%s", HJUST_CENTER|VJUST_CENTER, 0xffffffff, info_lines[i]);
		y -= lh;
	}
}

static char	info_text[256];			/* 0x80173338: the info text typed */
static char	price_info[INFO_LEN];		/* 0x80173438: the edited mode's info */

/* 0x80112de4: edit p (named name) - its 18 values, then its currency and info text */
void edit_pricing(int editing, char *name, pricing_t *p)
{
	char	buf[64];
	char	*s;
	int	*ip;
	int	i, sel, mod, delay, done;
	float	y;

	mod = 0;
	sel = 0;
	delay = 30;
	done = 0;
	y = ((font1_height + font1_height) * 9.0F + vres) / 2.0F;
	if(price_fields[0].x == 0.0F)
	{
		for(i = 0; i < NUM_PRICE_FIELDS; i += 2)
		{
			price_fields[i].x = hres * 0.05F;
			price_fields[i].y = y;
			price_fields[i].vx = hres / 2.0F - hres * 0.05F;
			price_fields[i].vy = y;
			price_fields[i + 1].x = hres / 2.0F;
			price_fields[i + 1].y = y;
			price_fields[i + 1].vx = hres - hres * 0.05F;
			price_fields[i + 1].vy = y;
			y -= font1_height + font1_height;
		}
	}
	ip = (int *)p;
	for(i = 0; i < NUM_PRICE_FIELDS; i++)
		price_fields[i].val = *ip++;
	if(editing)
		graphical_printf(buf, "EDITING %s", name);
	else
		graphical_printf(buf, "CREATING %s", name);
	draw_text_box(buf, -1.0F, vres - 40.0F, 1.0F);
	set_text_font(1);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	set_string_id(0x4f00);
	for(i = 0; i < NUM_PRICE_FIELDS; i++)
	{
		set_text_position((int)price_fields[i].x, (int)price_fields[i].y, 1.0F);
		oprintf("%s", price_fields[i].name);
	}
	for(;;)
	{
		del1c(0x4f01, -1);
		delete_multiple_strings(0x4f01, -1);
		set_text_color(0xffffffff);
		set_text_justification_mode(HJUST_RIGHT);
		set_string_id(0x4f01);
		for(i = 0; i < NUM_PRICE_FIELDS; i++)
		{
			set_text_position((int)price_fields[i].vx, (int)price_fields[i].vy, 1.0F);
			if(price_fields[i].type == FIELD_NUMBER)
				oprintf("%d", price_fields[i].val);
			else if(price_fields[i].type == FIELD_YESNO)
				oprintf(!price_fields[i].val ? "NO" : "YES");
		}
		if(!mod)
		{
			draw_thick_box(price_fields[sel].x - 6.0F, price_fields[sel].y - font1_height / 2.0F,
				get_string_width(price_fields[sel].name) + price_fields[sel].x + 6.0F,
				font1_height + font1_height + price_fields[sel].y - 3.0F, 1.0F, 2.0F, 0xffffffff, 0x4f01, 0);
			set_text_font(1);
			set_text_color(0xff00ffff);
			set_text_justification_mode(HJUST_CENTER);
			set_string_id(0x4f01);
			if(vres < 384.0F)
				y = vres * 0.2F;
			else
				y = vres * 0.25F;
			set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
			oprintf("VOL UP/DOWN TO SELECT AN ITEM TO MODIFY");
			y -= font1_height * 1.5F;
			set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
			oprintf("PRESS ANY BUTTON TO MODIFY THE ITEM");
			y -= font1_height * 1.5F;
			set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
			oprintf("\x1b%dcPRESS \x1b%dcANY\x1b%dc START BUTTON TO SAVE AND EXIT", 0xff00ff00, 0xffffffff, 0xff00ff00);
		}
		else
		{
			draw_thick_box(price_fields[sel].vx - get_string_width("000") - 6.0F, price_fields[sel].vy - font1_height / 2.0F,
				price_fields[sel].vx + 3.0F,
				font1_height + font1_height + price_fields[sel].vy - 3.0F, 1.0F, 2.0F, 0xffffffff, 0x4f01, 0);
			set_text_font(1);
			set_text_color(0xff00ffff);
			set_text_justification_mode(HJUST_CENTER);
			set_string_id(0x4f01);
			if(vres < 384.0F)
				y = vres * 0.2F;
			else
				y = vres * 0.25F;
			set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
			oprintf("VOL UP/DOWN TO MODIFY \x1b%dc%s", 0xff00ff00, price_fields[sel].name);
			y -= font1_height * 1.5F;
			set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
			oprintf("\x1b%dcPRESS ANY BUTTON WHEN DONE", 0xff00ffff);
		}
		sleep(1);
		if(mod && (get_dip_coin_current() & 0x24))
			break;
		while(sw_select() || sw_back())
			sleep(1);
		while(--delay && (sw_up() || sw_down() || sw_right() || sw_left()))
			sleep(1);
		if(sw_up() || sw_down() || sw_left() || sw_right() || sw_select() || sw_back())
			delay = 5;
		else
			delay = 30;
		while(!sw_up() && !sw_down() && !sw_left() && !sw_right() && !sw_select() && !sw_back())
		{
			if(get_dip_coin_current() & 0x24)
			{
				done = 1;
				break;
			}
			sleep(1);
		}
		if(done)
			break;
		if(!mod)
		{
			if(sw_select() || sw_back())
			{
				snd_scall_bank("ttest", 11, 180, 127, 0x8000);
				mod = 1;
				continue;
			}
			if(sw_down())
			{
				sel += 2;
				if(sel >= NUM_PRICE_FIELDS)
				{
					if(sel & 1)
						sel = NUM_PRICE_FIELDS - 1;
					else
					{
						sel = 1;
						snd_scall_bank("ttest", 12, 180, 127, 0x8000);
					}
				}
				else
					snd_scall_bank("ttest", 12, 180, 127, 0x8000);
			}
			else if(sw_up())
			{
				sel -= 2;
				if(sel >= NUM_PRICE_FIELDS)
				{
					if(sel == -1)
					{
						sel = NUM_PRICE_FIELDS - 2;
						snd_scall_bank("ttest", 12, 180, 127, 0x8000);
					}
					else
						sel += 2;
				}
				else
					snd_scall_bank("ttest", 12, 180, 127, 0x8000);
			}
			else if(sw_left())
			{
				sel++;
				if(sel > NUM_PRICE_FIELDS)
					sel = NUM_PRICE_FIELDS;
				else
					snd_scall_bank("ttest", 12, 180, 127, 0x8000);
			}
			else if(sw_right())
			{
				if(--sel < 0)
					sel = 0;
				else
					snd_scall_bank("ttest", 12, 180, 127, 0x8000);
			}
		}
		else
		{
			if(sw_select() || sw_back())
			{
				snd_scall_bank("ttest", 11, 180, 127, 0x8000);
				mod = 0;
				continue;
			}
			if(sw_vol_up())
			{
				price_fields[sel].val++;
				if(price_fields[sel].val > price_fields[sel].max)
					price_fields[sel].val = price_fields[sel].max;
				else
					snd_scall_bank("ttest", 12, 180, 127, 0x8000);
			}
			else if(sw_vol_down())
			{
				price_fields[sel].val--;
				if(price_fields[sel].val < price_fields[sel].min)
					price_fields[sel].val = price_fields[sel].min;
				else
					snd_scall_bank("ttest", 12, 180, 127, 0x8000);
			}
		}
	}
	del1c(0, 0);
	delete_multiple_strings(0, 0);
	currency = p->currency;
	currency_menu.sel = currency;
	do_menu(&currency_menu);
	del1c(0, 0);
	delete_multiple_strings(0, 0);
	p->currency = currency;
	draw_title("CURRENT PRICING PROMPT");
	show_price_info(vres * 0.8F, p);
	i = dialog("DO YOU WANT TO CHANGE ?", -1.0F, -1.0F, 1.0F, 3);
	del1c(0, 0);
	delete_multiple_strings(0, 0);
	if(i == 1)
	{
		s = text_entry("PRICING - LINE 1", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/$  ac", 32, info_lines[0]);
		graphical_printf(info_text, "%s", s);
		s = text_entry("PRICING - LINE 2", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/$  ac", 32, info_lines[1]);
		if(strlen(s))
		{
			strcat(info_text, "\n");
			strcat(info_text, s);
			s = text_entry("PRICING - LINE 3", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/$  ac", 32, info_lines[2]);
			if(strlen(s))
			{
				strcat(info_text, "\n");
				strcat(info_text, s);
			}
		}
		strcpy(price_info, info_text);
	}
	p->name = 0;
	p->coins = 0;
	ip = (int *)p;
	for(i = 0; i < NUM_PRICE_FIELDS; i++)
		*ip++ = price_fields[i].val;
	while(get_dip_coin_current() & 0x24)
		sleep(1);
}

static char	prc_name[32];			/* 0x80173488: the new mode's */

/* 0x80113c5c: ADD - a new mode, from the current pricing, saved as NAME.PRC */
int add_custom_pricing(void)
{
	char		buf[64];
	pricing_t	pr;
	struct ffblk	ff;
	int		continent, country, price;
	char		*s;
	FILE		*fp;
	pricing_t	*t;
	int		*ip;
	unsigned int	i;
	int		j, err;

again:
	if(!(s = text_entry("CUSTOM PRICE NAME", "ABCDEFGHIJKLMNOPQRSTUVWXYZ  ac", 8, 0)))
	{
		if(dialog("NO NAME SPECIFIED - DO YOU WANT TO ABORT ?", -1.0F, -1.0F, 1.0F, 3) != 1)
			goto again;
		return 0;
	}
	strcpy(prc_name, s);
	graphical_printf(buf, "%s.prc", prc_name);
	if(!findfirst(buf, &ff, 0))
	{
		graphical_printf(buf, "\x1b%dc%s\x1b%dc ALREADY EXISTS - EDIT ?", 0xffffff00, prc_name, 0xff00ffff);
		if(dialog(buf, -1.0F, -1.0F, 1.0F, 3) == 1)
		{
			edit_pricing(1, prc_name, &pr);
			graphical_printf(buf, "DO YOU WANT TO SAVE \x1b%dc%s\x1b%dc ?", 0xffffff00, prc_name, 0xff00ffff);
			if(dialog(buf, -1.0F, -1.0F, 1.0F, 3) == 1)
			{
				graphical_printf(buf, "%s.prc", prc_name);
				if((fp = fopen(buf, "wb")))
				{
					if(fwrite(&pr, 1, sizeof(pr), fp) != sizeof(pr))
					{
						graphical_printf(buf, "\x1b%dcERROR WRITING PRICING", 0xffff0000);
						fclose(fp);
						remove(buf);
					}
					else
					{
						graphical_printf(buf, "\x1b%dc%s\x1b%dc SAVED", 0xffffff00, prc_name, 0xffffffff);
						fclose(fp);
					}
				}
				else
					graphical_printf(buf, "\x1b%dcERROR OPENING PRICING", 0xffff0000);
			}
			else
				graphical_printf(buf, "\x1b%dc%s\x1b%dc NOT SAVED", 0xffffff00, prc_name, 0xffffffff);
			message_box(buf, -1.0F, -1.0F, 1.0F);
			wait_test_exit();
			return 0;
		}
		if(dialog("ENTER A DIFFERENT NAME ?", -1.0F, -1.0F, 1.0F, 3) != 2)
			goto again;
		return 0;
	}
	if(get_adjustment(CONTINENT_ADJ, &continent))
	{
		memset(&pr, 0, sizeof(pr));
		price_info[0] = 0;
	}
	else if(continent == CUSTOM_CONTINENT)
	{
		err = 0;
		ip = (int *)&pr;
		for(i = 0; i < sizeof(pr) / sizeof(int); i++, ip++)
			err |= get_adjustment(LEFT_SLOT_UNITS_ADJ + i, ip);
		ip = (int *)price_info;
		for(j = 0; j < 20; j++, ip++)
			err |= get_adjustment(PRICING_INFO_STRING_ADJ + j, ip);
		if(err)
		{
			memset(&pr, 0, sizeof(pr));
			price_info[0] = 0;
		}
	}
	else
	{
		memset(&pr, 0, sizeof(pr));
		price_info[0] = 0;
		if(!get_adjustment(COUNTRY_ADJ, &country) && !get_adjustment(PRICE_ADJ, &price))
		{
			t = continent_pricing[continent][country];
			memcpy(&pr, &t[price], sizeof(pr));
			strcpy(price_info, t[price].info);
		}
	}
	pr.info = price_info;
	while(sw_8() || sw_select() || sw_back())
		sleep(1);
	edit_pricing(0, prc_name, &pr);
	graphical_printf(buf, "DO YOU WANT TO SAVE \x1b%dc%s\x1b%dc ?", 0xffffff00, prc_name, 0xff00ffff);
	if(dialog(buf, -1.0F, -1.0F, 1.0F, 3) == 1)
	{
		graphical_printf(buf, "%s.prc", prc_name);
		if((fp = fopen(buf, "wb")))
		{
			if(fwrite(&pr, 1, sizeof(pr), fp) != sizeof(pr))
			{
				graphical_printf(buf, "\x1b%dcERROR WRITING PRICING", 0xffff0000);
				fclose(fp);
				remove(buf);
			}
			else if(fwrite(pr.info, 1, INFO_LEN, fp) != INFO_LEN)
			{
				graphical_printf(buf, "\x1b%dcERROR WRITING PRICING", 0xffff0000);
				fclose(fp);
				remove(buf);
			}
			else
			{
				graphical_printf(buf, "\x1b%dc%s\x1b%dc SAVED", 0xffffff00, prc_name, 0xffffffff);
				fclose(fp);
				free_prc_menu();
				build_prc_menu();
				custom_modified = 1;
			}
		}
		else
			graphical_printf(buf, "\x1b%dcERROR OPENING PRICING", 0xffff0000);
	}
	else
		graphical_printf(buf, "\x1b%dc%s\x1b%dc NOT SAVED", 0xffffff00, prc_name, 0xffffffff);
	message_box(buf, -1.0F, -1.0F, 1.0F);
	wait_test_exit();
	return 0;
}

/* 0x801141d8: EDIT, of file n */
int edit_custom_pricing(int n)
{
	pricing_t	pr;
	char		buf[64];
	FILE		*fp;

	graphical_printf(buf, "%s.prc", prc_items[n].name);
	if(!(fp = fopen(buf, "rb")))
	{
		graphical_printf(buf, "\x1b%dcERROR OPENING PRICING FILE %s", 0xffff0000, prc_items[n].name);
		message_box(buf, -1.0F, -1.0F, 1.0F);
		wait_test_exit();
		return 0;
	}
	if(fread(&pr, 1, sizeof(pr), fp) != sizeof(pr))
	{
		fclose(fp);
		graphical_printf(buf, "\x1b%dERROR READING PRICING FILE %s", 0xffff0000, prc_items[n].name);
		wait_test_exit();
		return 0;
	}
	pr.info = price_info;
	if(fread(pr.info, 1, INFO_LEN, fp) != INFO_LEN)
	{
		fclose(fp);
		graphical_printf(buf, "\x1b%dcERROR READING PRICING FILE %s", 0xffff0000, prc_items[n].name);
		wait_test_exit();
		return 0;
	}
	fclose(fp);
	while(sw_8())
		sleep(1);
	edit_pricing(1, prc_items[n].name, &pr);
	graphical_printf(buf, "DO YOU WANT TO SAVE \x1b%dc%s\x1b%dc ?", 0xffffff00, prc_items[n].name, 0xff00ffff);
	if(dialog(buf, -1.0F, -1.0F, 1.0F, 3) == 1)
	{
		graphical_printf(buf, "%s.prc", prc_items[n].name);
		if((fp = fopen(buf, "wb")))
		{
			if(fwrite(&pr, 1, sizeof(pr), fp) != sizeof(pr))
			{
				graphical_printf(buf, "\x1b%dcERROR WRITING PRICING", 0xffff0000);
				fclose(fp);
				remove(buf);
			}
			else if(fwrite(pr.info, 1, INFO_LEN, fp) != INFO_LEN)
			{
				graphical_printf(buf, "\x1b%dcERROR WRITING PRICING", 0xffff0000);
				fclose(fp);
				remove(buf);
			}
			else
			{
				graphical_printf(buf, "\x1b%dc%s\x1b%dc SAVED", 0xffffff00, prc_items[n].name, 0xffffffff);
				fclose(fp);
				free_prc_menu();
				build_prc_menu();
				custom_modified = 1;
			}
		}
		else
			graphical_printf(buf, "\x1b%dcERROR OPENING PRICING", 0xffff0000);
	}
	else
		graphical_printf(buf, "\x1b%dc%s\x1b%dc NOT SAVED", 0xffffff00, prc_items[n].name, 0xffffffff);
	message_box(buf, -1.0F, -1.0F, 1.0F);
	wait_test_exit();
	return -1;
}

/* 0x80114554: every .prc file's mode, into a table (for the CUSTOM continent) */
void load_custom_pricing(int *count, pricing_t **tbl)
{
	struct ffblk	ff;
	char		name[32];
	FILE		*fp;
	pricing_t	*p;
	char		*s;
	int		n;

	if(findfirst("*.prc", &ff, 0))
	{
		*count = 0;
		return;
	}
	n = 1;
	while(!findnext(&ff))
		n++;
	if(!(p = malloc(n * sizeof(pricing_t))))
		return;
	*tbl = p;
	findfirst("*.prc", &ff, 0);
	do
	{
		if(!(fp = fopen(ff.ff_name, "rb")))
		{
			n--;
			continue;
		}
		if(fread(p, 1, sizeof(pricing_t), fp) != sizeof(pricing_t) || !(p->info = malloc(INFO_LEN))
			|| fread(p->info, 1, INFO_LEN, fp) != INFO_LEN)
		{
			fclose(fp);
			n--;
			continue;
		}
		fclose(fp);
		graphical_printf(name, "%s", ff.ff_name);
		if((s = strchr(name, '.')))
			*s = 0;
		if((p->name = malloc(strlen(name) + 1)))
			strcpy(p->name, name);
		p++;
	} while(!findnext(&ff));
	*count = n;
}

/* 0x801146fc */
void free_custom_pricing(int count, pricing_t *tbl)
{
	int	i;

	if(!tbl)
		return;
	for(i = 0; i < count; i++)
	{
		if(tbl[i].name)
			free(tbl[i].name);
		if(tbl[i].info)
			free(tbl[i].info);
	}
	free(tbl);
}

/* 0x801147ac: SELECT - the CUSTOM continent */
int select_custom_pricing(int n)
{
	pricing_continent = CUSTOM_CONTINENT;
	set_pricing(0);
	custom_modified = 0;
	return -1;
}

/* 0x801147e0: a currency_items item */
int set_currency(int n)
{
	char	buf[64];

	currency = n;
	graphical_printf(buf, "CURRENCY TYPE SET TO \x1b%dc%s", 0xffffffff, currency_items[n].name);
	message_box(buf, -1.0F, -1.0F, 1.0F);
	wait_test_exit();
	return -1;
}
