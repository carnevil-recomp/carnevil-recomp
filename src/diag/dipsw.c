/*
 * dipsw.c - DIPSWITCH INPUTS TEST (0x80107ab0-0x8010845c): the two banks of eight, U9 (bits 0-7
 * of the dip word) and U8 (8-15), each switch showing its position, and beside each field the
 * setting it selects.
 */
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

extern int	get_dip_coin_current(void);
extern void	sleep(int);
extern int	snd_scall_bank(char *, int, int, int, int);
extern void	draw_hline(float, float, float, float, int, int);
extern void	draw_vline(float, float, float, float, int, int);
extern int	(*sw_exit)(void);		/* 0x80167d80: nothing on CarnEvil */

/* a field of one or more switches and the names of its settings */
typedef struct dip_field {
	int	bank;			/* 0 U9, 1 U8 */
	int	bit;			/* its first switch */
	int	bits;
	char	**names;		/* by value, or 0: UNUSED */
	int	pad[2];
} dip_field_t;

char	*coinage_source_names[] = { "DIP COINAGE", "CMOS COINAGE" };
char	*coinage_names[] = {
	"USA 1", "USA 2", "USA 3", "USA 4", "USA 5", "USA 6", "USA 7", "USA ECA",
	"FRANCE 1", "FRANCE 2", "FRANCE 3", "FRANCE 4", "FRANCE 5", "FRANCE 6", "FRANCE 7", "FRANCE ECA",
	"GERMAN 1", "GERMAN 2", "GERMAN 3", "GERMAN 4", "GERMAN 5", "GERMAN 6", "GERMAN 7", "GERMAN ECA",
	"UK 1", "UK 2", "UK 3", "UK 4", "UK 5", "UK 6", "UK ECA", "FREE PLAY"
};
char	*resolution_names[] = { "NOT USED", "LOW RES", "NOT USED", "NOT USED" };
char	*powerup_test_names[] = { "RUN PU TESTS", "BYPASS PU TESTS" };
char	*mode_names[] = { "GAME MODE", "TEST MODE" };
char	*test_loop_names[] = { "NO PU TEST LP", "PU TEST LOOP" };
char	*clock_names[] = { "51 MHZ (FACTORY)", "49 MHZ", "47 MHZ", "45 MHZ" };

dip_field_t	dip_fields[] = {
	{ 0, 0, 1, coinage_source_names },
	{ 0, 1, 5, coinage_names },
	{ 0, 6, 1, 0 },
	{ 0, 7, 1, test_loop_names },
	{ 1, 0, 1, 0 },
	{ 1, 1, 2, resolution_names },
	{ 1, 3, 2, clock_names },
	{ 1, 5, 1, 0 },
	{ 1, 6, 1, powerup_test_names },
	{ 1, 7, 1, mode_names }
};

static float	bank_x1[2], bank_y1[2], bank_x2[2], bank_y2[2];

/* 0x80107ab0: a field's bracket from its switches out to x, and the setting its value
   (the dip bits from its first switch up) selects */
void draw_dip_field(dip_field_t *f, int value, float x, float y)
{
	float	w, mx;
	int	v;

	w = hres / 512.0F * 20.0F;
	v = value & (0xff >> (8 - f->bits));
	set_text_justification_mode(8);
	set_text_color(-1);
	set_text_font(2);
	if(f->bits == 1)
	{
		y += font1_height * 0.5F;
		draw_hline(x + 2.0F, x + w, y, 1.0F, -1, 0);
	}
	else
	{
		mx = x + w / 2.0F + 1.0F;
		draw_hline(x + 2.0F, mx, y + font1_height * 0.5F, 1.0F, -1, 0);
		draw_hline(x + 2.0F, mx, y + (f->bits * 2 - 1) * font1_height - font1_height * 0.5F, 1.0F, -1, 0);
		draw_vline(mx, y + font1_height * 0.5F, y + (f->bits * 2 - 1) * font1_height - font1_height * 0.5F, 1.0F, -1, 0);
		y += (f->bits * 2 - 1) * font1_height * 0.5F;
		draw_hline(mx, x + w, y, 1.0F, -1, 0);
	}
	x += w + 2.0F;
	set_text_position((int)x, (int)y, 1.0F);
	if(f->names)
		oprintf("%s", f->names[v]);
	else
		oprintf("UNUSED");
}

/* 0x80107d48: both banks with every switch's position */
void draw_dip_banks(void)
{
	float	w, x, y;
	int	bank, i, k, dips;

	w = hres / 7.0F;
	bank_x1[0] = w;
	bank_x2[0] = w + w;
	bank_y1[0] = vres / 2.0F - (font1_height + font1_height) * 8.0F / 2.0F;
	bank_y2[0] = bank_y1[0] + (font1_height + font1_height) * 8.0F;
	bank_x1[1] = hres - w * 3.0F;
	bank_x2[1] = bank_x1[1] + w;
	bank_y1[1] = bank_y1[0];
	bank_y2[1] = bank_y2[0];
	draw_rect(bank_x1[0], bank_y1[0], bank_x2[0], bank_y2[0], 3.0F, 0xffff0000, 0x4f00);
	draw_rect(bank_x1[1], bank_y1[1], bank_x2[1], bank_y2[1], 3.0F, 0xffff0000, 0x4f00);

	/* the slots */
	for(bank = 0; bank < 2; bank++)
	{
		x = bank_x1[bank] + w * 0.125F;
		y = bank_y1[bank] + font1_height / 2.0F;
		for(i = 0; i < 8; i++)
		{
			draw_rect(x, y, x + w * 0.75F, y + font1_height, 2.0F, 0xff606060, 0);
			y += font1_height + font1_height;
		}
	}

	set_text_font(1);
	set_text_justification_mode(6);
	set_text_color(0xffffff00);
	set_text_position((int)((bank_x1[0] + bank_x2[0]) / 2.0F), (int)(bank_y1[0] - 4.0F), 1.0F);
	oprintf("U9");
	set_text_position((int)((bank_x1[1] + bank_x2[1]) / 2.0F), (int)(bank_y1[1] - 4.0F), 1.0F);
	oprintf("U8");
	set_text_justification_mode(2);
	set_text_position((int)bank_x1[0], (int)(bank_y2[0] + 4.0F), 1.0F);
	oprintf("OFF");
	set_text_position((int)bank_x1[1], (int)(bank_y2[1] + 4.0F), 1.0F);
	oprintf("OFf");
	set_text_justification_mode(2);
	set_text_position((int)bank_x2[0], (int)(bank_y2[0] + 4.0F), 1.0F);
	oprintf("ON");
	set_text_position((int)bank_x2[1], (int)(bank_y2[1] + 4.0F), 1.0F);
	oprintf("ON");

	/* the switches: a white block at OFF for a set bit, green at ON for a clear one */
	dips = (unsigned)get_dip_coin_current() >> 16;
	for(bank = 0, k = 0; bank < 2; bank++)
	{
		x = bank_x1[bank] + w * 0.125F;
		y = bank_y1[bank] + font1_height / 2.0F;
		for(i = 0; i < 8; i++)
		{
			if(!(dips & 1))
				draw_rect(x, y, x + w * 0.375F, y + font1_height, 1.0F, -1, 0);
			else
				draw_rect(x + w * 0.375F, y, x + (w * 0.375F + w * 0.375F), y + font1_height, 1.0F, 0xff00ff00, 0);
			set_text_position((int)(bank_x1[bank] - 4.0F), (int)(y + font1_height * 0.5F), 1.0F);
			set_text_justification_mode(9);
			set_text_color(0xff00ffff);
			set_text_font(1);
			oprintf("%d", i + 1);
			if(dip_fields[k].bank == bank && dip_fields[k].bit == (i & 7))
			{
				draw_dip_field(&dip_fields[k], dips, bank_x2[bank], y);
				k++;
			}
			dips >>= 1;
			y += font1_height + font1_height;
		}
	}
}

/* 0x80108308 */
int dip_switch_tests(void)
{
	int	done = 0, dips, shown;

	dips = (unsigned)get_dip_coin_current() >> 16;
	do
	{
		shown = dips;
		draw_title("DIPSWITCH INPUTS TEST");
		message_box("PRESS CREDIT OR TEST TO EXIT", -1.0F, 5.0F, 1.0F);
		draw_dip_banks();
		while((unsigned)get_dip_coin_current() >> 16 == shown)
		{
			if(sw_exit() || sw_select() || sw_back())
			{
				done = 1;
				break;
			}
			sleep(1);
		}
		if(!done)
			snd_scall_bank("ttest", 13, 180, 127, 0x8000);
		dips = (unsigned)get_dip_coin_current() >> 16;
		delete_multiple_strings(0, 0);
		del1c(0, 0);
	} while(!done);
	snd_scall_bank("ttest", 11, 180, 127, 0x8000);
	return 0;
}
