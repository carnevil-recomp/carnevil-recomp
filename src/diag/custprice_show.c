/*
 * custprice_show.c - SHOW CURRENT PRICING (0x801110b8-0x80111c88): the pricing that the CMOS
 * continent, country and price adjustments pick, from the country tables or, for the CUSTOM
 * continent, from the .prc files on the disk.
 */
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include <goose/adjust.h>
#include "diag.h"
#include "pricing.h"

#define	CUSTOM_CONTINENT	6

extern int	get_dip_coin_current(void);
extern int	get_adjustment(int, int *);
extern int	graphical_printf(char *, char *, ...);
extern void	draw_text_box(char *, float, float, float);
extern void	load_custom_pricing(int *, pricing_t **);	/* custprice.c: the .prc files */
extern void	free_custom_pricing(int, pricing_t *);

/* pricing.c: each country's modes */
extern pricing_t	usa_pricing[], canada_pricing[], antilles_pricing[], austria_pricing[],
	belgium_pricing[], denmark_pricing[], finland_pricing[], france_pricing[],
	france_eca_pricing[], germany_pricing[], hungary_pricing[], italy_pricing[],
	netherlands_pricing[], norway_pricing[], spain_pricing[], sweden_pricing[],
	switzerland_pricing[], uk_pricing[], japan_pricing[], australia_pricing[],
	new_zealand_pricing[];

void	show_pricing(int);

static pricing_t	*cur_pricing;		/* 0x80173220: the continent's or country's modes */
static int	num_custom;			/* 0x80173224: the custom modes loaded */

/* 0x801110b8 */
int show_current_pricing(void)
{
	char	buf[64];
	int	continent, country, price;
	int	err = 0;

	if(!((get_dip_coin_current() >> 16) & 1))
	{
		set_text_font(1);
		set_text_color(0xffffff00);
		set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F + 8.0F), 1.0F);
		oprintf("DIPSWITCH COINAGE ENABLED");
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F - 8.0F), 1.0F);
		oprintf("CMOS PRICING NOT AVAILABLE");
	}
	else
	{
		if(get_adjustment(CONTINENT_ADJ, &continent))
			return 0;
		if(get_adjustment(COUNTRY_ADJ, &country))
			return 0;
		if(get_adjustment(PRICE_ADJ, &price))
			return 0;
		if(continent < 0 && country < 0 && price < 0)
			return 0;
		switch(continent)
		{
			case 0:
				switch(country)
				{
					case 0:
						cur_pricing = usa_pricing;
						break;
					case 1:
						cur_pricing = canada_pricing;
						break;
					default:
						err = 1;
						break;
				}
				break;
			case 1:
				if(country)
					err = 1;
				else
					cur_pricing = antilles_pricing;
				break;
			case 2:
				switch(country)
				{
					case 0:
						cur_pricing = austria_pricing;
						break;
					case 1:
						cur_pricing = belgium_pricing;
						break;
					case 2:
						cur_pricing = denmark_pricing;
						break;
					case 3:
						cur_pricing = finland_pricing;
						break;
					case 4:
						cur_pricing = france_pricing;
						break;
					case 5:
						cur_pricing = france_eca_pricing;
						break;
					case 6:
						cur_pricing = germany_pricing;
						break;
					case 7:
						cur_pricing = hungary_pricing;
						break;
					default:
						err = 1;
						break;
				}
				break;
			case 3:
				switch(country)
				{
					case 0:
						cur_pricing = italy_pricing;
						break;
					case 1:
						cur_pricing = netherlands_pricing;
						break;
					case 2:
						cur_pricing = norway_pricing;
						break;
					case 3:
						cur_pricing = spain_pricing;
						break;
					case 4:
						cur_pricing = sweden_pricing;
						break;
					case 5:
						cur_pricing = switzerland_pricing;
						break;
					case 6:
						cur_pricing = uk_pricing;
						break;
					default:
						err = 1;
						break;
				}
				break;
			case 4:
				if(country)
					err = 1;
				else
					cur_pricing = japan_pricing;
				break;
			case 5:
				switch(country)
				{
					case 0:
						cur_pricing = australia_pricing;
						break;
					case 1:
						cur_pricing = new_zealand_pricing;
						break;
					default:
						err = 1;
						break;
				}
				break;
			case CUSTOM_CONTINENT:
				load_custom_pricing(&num_custom, &cur_pricing);
				break;
			default:
				err = 1;
				break;
		}
		if(!err)
			show_pricing(price);
		else
		{
			graphical_printf(buf, "\x1b%dcERROR IN CMOS DATA : PRICING NOT VALID", 0xffff0000);
			message_box(buf, -1.0F, -1.0F, 1.0F);
		}
		if(continent == CUSTOM_CONTINENT)
			free_custom_pricing(num_custom, cur_pricing);
	}
	wait_test_exit();
	return 0;
}

/* 0x80111580: the adjustments of pricing n of the table */
void show_pricing(int n)
{
	char		buf[128];
	pricing_t	*p = &cur_pricing[n];
	float		x = hres * 0.2F, y = vres - 60.0F;

	graphical_printf(buf, "CURRENT PRICING \x1b%dc%s", 0xffffff00, p->name);
	draw_text_box(buf, -1.0F, vres - 40.0F, 1.0F);
	set_text_font(1);

	set_text_position((int)x, (int)y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("LEFT SLOT UNITS");
	set_text_position((int)(hres * 0.8F), (int)y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->left_slot_units);
	y -= font1_height * 1.5F;

	set_text_position((int)x, (int)y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("RIGHT SLOT UNITS");
	set_text_position((int)(hres * 0.8F), (int)y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->right_slot_units);
	y -= font1_height * 1.5F;

	set_text_position((int)x, (int)y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("CENTER SLOT UNITS");
	set_text_position((int)(hres * 0.8F), (int)y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->center_slot_units);
	y -= font1_height * 1.5F;

	set_text_position((int)x, (int)y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("EXTRA SLOT UNITS");
	set_text_position((int)(hres * 0.8F), (int)y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->extra_slot_units);
	y -= font1_height * 1.5F;

	set_text_position((int)x, (int)y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("BILL VALIDTOR UNITS");
	set_text_position((int)(hres * 0.8F), (int)y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->bill_validator_units);
	y -= font1_height * 1.5F;

	set_text_position((int)x, (int)y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("UNITS PER CREDIT");
	set_text_position((int)(hres * 0.8F), (int)y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->units_per_credit);
	y -= font1_height * 1.5F;

	set_text_position((int)x, (int)y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("UNITS PER BONUS");
	set_text_position((int)(hres * 0.8F), (int)y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->units_per_bonus);
	y -= font1_height * 1.5F;

	set_text_position((int)x, (int)y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("MINIMUM UNITS");
	set_text_position((int)(hres * 0.8F), (int)y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->minimum_units);
	y -= font1_height * 1.5F;

	set_text_position((int)x, (int)y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("CREDITS TO START");
	set_text_position((int)(hres * 0.8F), (int)y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->credits_to_start);
	y -= font1_height * 1.5F;

	set_text_position((int)x, (int)y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("CREDITS TO CONTINUE");
	set_text_position((int)(hres * 0.8F), (int)y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->credits_to_continue);
	y -= font1_height * 1.5F;

	set_text_position((int)x, (int)y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("MAX CREDITS");
	set_text_position((int)(hres * 0.8F), (int)y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->maximum_credits);
	y -= font1_height * 1.5F;

	set_text_position((int)x, (int)y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("COINS PER BILL");
	set_text_position((int)(hres * 0.8F), (int)y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->coins_per_bill);
}
