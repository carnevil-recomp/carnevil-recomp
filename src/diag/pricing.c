/*
 * pricing.c - the per-country pricing modes (0x8010f650-0x8011059c): each country submenu's
 * item runs set_pricing(), which lists that country's modes; highlighting one shows it
 * (show_price) and choosing it writes it to the adjustments (choose_price). The tables are
 * GOOSE's pricing adjustments in adjust.h order, LEFT_SLOT_UNITS_ADJ to CURRENCY_ADJ, then the
 * credit text for PRICING_INFO_STRING_ADJ (its first 80 bytes, as 20 words), the name and the
 * coins each slot takes.
 */
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include <goose/adjust.h>
#include "diag.h"

extern int	get_dip_coin_current(void);
extern int	set_adjustment(int, int);
extern int	graphical_printf(char *, char *, ...);
extern void	draw_text_box(char *, float, float, float);

extern int	pricing_continent;		/* pricing_menu.c: the highlighted continent */
extern int	pricing_country;		/* asia_pricing.c: the highlighted country */

typedef struct pricing {
	int	left_slot_units;
	int	right_slot_units;
	int	center_slot_units;
	int	extra_slot_units;
	int	bill_validator_units;
	int	units_per_credit;
	int	units_per_bonus;
	int	minimum_units;
	int	credits_to_start;
	int	credits_to_continue;
	int	maximum_credits;
	int	coins_per_bill;
	int	show_fractions;
	int	left_chute_count;
	int	right_chute_count;
	int	center_chute_count;
	int	extra_chute_count;
	int	bill_validator_count;
	int	currency;
	char	*info;			/* PRICING_INFO_STRING_ADJ.. */
	char	*name;
	char	*coins;			/* what each slot takes, tab separated */
} pricing_t;

extern void	load_custom_pricing(int *, pricing_t **);	/* 0x80114554: reads the custom modes */
extern void	free_custom_pricing(int, pricing_t *);		/* 0x801146fc */

int	choose_price(int);
void	show_price(int);

char	*currency_names[] = {
	"DOLLAR", "POUND", "GUILDER", "SHILLING", "F", "KR", "MARKKA", "DM", "LIRE", "FORINT",
	"HFI", "PESETA", "Y"
};
char	*currency_symbols[12] = { "$", "L" };

char	usa_name_1[] = "USA 1";
char	usa_name_2[] = "USA 2";
char	usa_name_3[] = "USA 3";
char	usa_name_4[] = "USA 4";
char	usa_name_5[] = "USA 5";
char	usa_name_6[] = "USA 6";
char	usa_name_7[] = "USA 7";
char	usa_name_8[] = "USA 8";
char	usa_name_9[] = "USA 9";
char	usa_name_10[] = "USA 10";
char	usa_name_11[] = "USA 11";
char	usa_name_12[] = "USA 12";
char	usa_name_13[] = "USA 13";
char	usa_name_14[] = "USA ECA";

pricing_t	usa_pricing[] = {
	{ 1, 1, 0, 0, 4, 1, 0, 0, 2, 2, 32, 4, 1, 1, 1, 0, 0, 4, 0,
	  "1 CREDIT / 1 COIN", usa_name_1, ".25\t.25\t-\t-\t$1.00" },
	{ 1, 1, 0, 0, 4, 1, 0, 0, 2, 1, 32, 4, 1, 1, 1, 0, 0, 4, 0,
	  "1 CREDIT / 1 COIN", usa_name_2, ".25\t.25\t-\t-\t$1.00" },
	{ 1, 1, 0, 0, 4, 1, 0, 0, 1, 1, 32, 4, 1, 1, 1, 0, 0, 4, 0,
	  "1 CREDIT / 1 COIN", usa_name_3, ".25\t.25\t-\t-\t$1.00" },
	{ 1, 1, 0, 0, 4, 2, 4, 0, 1, 1, 32, 4, 1, 1, 1, 0, 0, 4, 0,
	  "1 CREDIT / 2 COINS\n3 CREDITS / 4 COINS", usa_name_4, ".25\t.25\t-\t-\t$1.00" },
	{ 3, 3, 0, 0, 12, 4, 12, 6, 2, 1, 32, 4, 0, 1, 1, 0, 0, 4, 0,
	  "1 CREDIT / 2 COINS\n4 CREDITS / 4 COINS", usa_name_5, ".25\t.25\t-\t-\t$1.00" },
	{ 1, 1, 0, 0, 4, 2, 0, 0, 1, 1, 32, 4, 1, 1, 1, 0, 0, 4, 0,
	  "1 CREDIT / 2 COINS", usa_name_6, ".25\t.25\t-\t-\t$1.00" },
	{ 1, 1, 0, 0, 4, 2, 4, 0, 1, 1, 32, 4, 0, 1, 1, 0, 0, 4, 0,
	  "1 CREDIT / 2 COINS\n3 CREDITS / $1.00", usa_name_7, ".25\t.25\t-\t-\t$1.00" },
	{ 3, 3, 0, 0, 12, 4, 12, 6, 2, 2, 32, 4, 0, 1, 1, 0, 0, 4, 0,
	  "1 CREDIT / 2 COINS\n4 CREDITS / $1.00", usa_name_8, ".25\t.25\t-\t-\t$1.00" },
	{ 1, 1, 0, 0, 4, 1, 0, 0, 3, 2, 32, 4, 0, 1, 1, 0, 0, 4, 0,
	  "1 CREDIT / 1 COIN", usa_name_9, ".25\t.25\t-\t-\t$1.00" },
	{ 1, 1, 0, 0, 4, 1, 0, 0, 3, 3, 32, 4, 0, 1, 1, 0, 0, 4, 0,
	  "1 CREDIT / 1 COIN", usa_name_10, ".25\t.25\t-\t-\t$1.00" },
	{ 1, 1, 0, 0, 4, 1, 0, 0, 4, 2, 32, 4, 1, 1, 1, 0, 0, 4, 0,
	  "1 CREDIT / 1 COIN", usa_name_11, ".25\t.25\t-\t-\t$1.00" },
	{ 1, 1, 0, 0, 4, 1, 0, 0, 4, 3, 32, 4, 1, 1, 1, 0, 0, 4, 0,
	  "1 CREDIT / 1 COIN", usa_name_12, ".25\t.25\t-\t-\t$1.00" },
	{ 1, 1, 0, 0, 4, 1, 0, 0, 4, 4, 32, 4, 1, 1, 1, 0, 0, 4, 0,
	  "1 CREDIT / 1 COIN", usa_name_13, ".25\t.25\t-\t-\t$1.00" },
	{ 20, 5, 2, 1, 20, 5, 0, 0, 2, 2, 32, 20, 1, 20, 5, 2, 1, 20, 0,
	  "1 CREDIT / 25 CENTS\n4 CREDITS / $1.00", usa_name_14, "$1.00\t.25\t.10\t.05\t$1.00" }
};

char	canada_name_1[] = "CANADA";

pricing_t	canada_pricing[] = {
	{ 1, 4, 0, 0, 4, 2, 4, 0, 2, 2, 32, 1, 1, 1, 4, 0, 0, 0, 0,
	  "1 CREDIT / 2 x 25 CENTS\n3 CREDITS / $1.00", canada_name_1, ".25\t$1.00\t-\t-\t$1.00" }
};

char	antilles_name_1[] = "ANTILLES";

pricing_t	antilles_pricing[] = {
	{ 1, 4, 0, 0, 0, 1, 0, 0, 2, 2, 32, 4, 0, 1, 4, 0, 0, 0, 2,
	  "1 CREDIT / 25 CENTS\n4 CREDITS / 1 GUILDER", antilles_name_1, ".25\t1 GUILDER\t-\t-\t-" }
};

char	austria_name_1[] = "AUSTRIA 1";
char	austria_name_2[] = "AUSTRIA 2";

pricing_t	austria_pricing[] = {
	{ 1, 2, 0, 0, 0, 1, 0, 0, 2, 2, 32, 1, 1, 5, 10, 0, 0, 0, 3,
	  "1 CREDIT / 5 SCHILLING\n2 CREDITS / 10 SCHILLING", austria_name_1, "5 SCHILLING\t10 SCHILLING\t-\t-\t-" },
	{ 1, 2, 0, 0, 0, 2, 4, 0, 2, 2, 32, 1, 1, 5, 10, 0, 0, 0, 3,
	  "1 CREDIT / 2 x 5 SCHILLING\n3 CREDITS / 2 x 10 SCHILLING", austria_name_2, "5 SCHILLING\t10 SCHILLING\t-\t-\t-" }
};

char	belgium_name_1[] = "BELGIUM 1";
char	belgium_name_2[] = "BELGIUM 2";
char	belgium_name_3[] = "BELGIUM 3";
char	belgium_name_4[] = "BELGIUM ECA";

pricing_t	belgium_pricing[] = {
	{ 1, 1, 0, 0, 0, 1, 0, 0, 2, 2, 32, 1, 1, 20, 20, 0, 0, 0, 4,
	  "1 CREDIT / 20F", belgium_name_1, "20 F\t20 F\t-\t-\t-" },
	{ 3, 3, 0, 0, 0, 1, 0, 0, 1, 2, 32, 1, 1, 20, 20, 0, 0, 0, 3,
	  "3 CREDITS / 20F", belgium_name_2, "20 F\t20 F\t-\t-\t-" },
	{ 2, 2, 0, 0, 0, 1, 0, 0, 2, 2, 32, 1, 1, 20, 20, 0, 0, 0, 3,
	  "2 CREDITS / 20F", belgium_name_3, "20 F\t20 F\t-\t-\t-" },
	{ 10, 4, 1, 0, 0, 4, 0, 0, 2, 2, 32, 1, 1, 10, 4, 1, 0, 0, 3,
	  "1 CREDIT / 20F", belgium_name_4, "50 F\t20 F\t5 F\t-\t-" }
};

char	uk_name_1[] = "U.K. 1 ECA";
char	uk_name_2[] = "U.K. 2 ECA";
char	uk_name_3[] = "U.K. 3 ECA";
char	uk_name_4[] = "U.K. 4";
char	uk_name_5[] = "U.K. 5";
char	uk_name_6[] = "U.K. 6 ECA";
char	uk_name_7[] = "U.K. 7 ECA";

pricing_t	uk_pricing[] = {
	{ 10, 5, 2, 1, 0, 5, 10, 0, 1, 1, 32, 10, 0, 10, 5, 2, 1, 0, 1,
	  "1 CREDIT / 50P\n3 CREDITS / L1.00", uk_name_1, "L1.00\t50 P\t20 P\t10 P\t-" },
	{ 10, 5, 2, 1, 0, 5, 0, 0, 1, 1, 32, 10, 0, 10, 5, 2, 1, 0, 1,
	  "1 CREDIT / 50P\n2 CREDITS / L1.00", uk_name_2, "L1.00\t50 P\t20 P\t10 P\t-" },
	{ 20, 10, 4, 2, 0, 5, 20, 0, 1, 1, 32, 10, 0, 10, 5, 2, 1, 0, 1,
	  "1 CREDIT / 30P\n2 CREDITS / 50P\n5 CREDITS / L1.00", uk_name_3, "L1.00\t50 P\t20 P\t10 P\t-" },
	{ 10, 5, 0, 0, 0, 5, 10, 0, 1, 1, 32, 10, 0, 10, 5, 0, 0, 0, 1,
	  "1 CREDIT / 50P\n3 CREDITS / L1.00", uk_name_4, "L1.00\t50 P\t-\t-\t-" },
	{ 10, 5, 0, 0, 0, 5, 0, 0, 1, 1, 32, 10, 0, 10, 5, 0, 0, 0, 1,
	  "1 CREDIT / 50P\n2 CREDITS / L1.00", uk_name_5, "L1.00\t50 P\t-\t-\t-" },
	{ 20, 10, 4, 2, 0, 5, 0, 0, 1, 1, 32, 10, 0, 10, 5, 2, 1, 0, 1,
	  "1 CREDIT / 30P\n2 CREDITS / 50P\n4 CREDITS / L1.00", uk_name_6, "L1.00\t50 P\t20 P\t10 P\t-" },
	{ 30, 15, 6, 3, 0, 10, 0, 30, 1, 1, 32, 10, 0, 10, 5, 2, 1, 0, 1,
	  "3 CREDITS / L1.00", uk_name_7, "L1.00\t50 P\t20 P\t10 P\t-" }
};

char	denmark_name_1[] = "DENMARK";

pricing_t	denmark_pricing[] = {
	{ 3, 15, 0, 0, 0, 5, 30, 0, 2, 2, 32, 1, 0, 1, 5, 0, 0, 0, 5,
	  "1 CREDIT / 2 KR\n3 CREDITS / 5 KR\n7 CREDITS / 10 KR", denmark_name_1, "1 KR\t5 KR\t-\t-\t-" }
};

char	finland_name_1[] = "FINLAND";

pricing_t	finland_pricing[] = {
	{ 1, 1, 0, 0, 0, 1, 0, 0, 2, 2, 32, 1, 1, 1, 1, 0, 0, 0, 6,
	  "1 CREDIT / 1 MARKKA", finland_name_1, "1 MARKKA\t1 MARKKA\t-\t-\t-" }
};

char	france_name_1[] = "FRANCE 1";
char	france_name_2[] = "FRANCE 2";
char	france_name_3[] = "FRANCE 3";
char	france_name_4[] = "FRANCE 4";
char	france_name_5[] = "FRANCE 5";
char	france_name_6[] = "FRANCE 6";
char	france_name_7[] = "FRANCE 7";
char	france_name_8[] = "FRANCE 8";
char	france_name_9[] = "FRANCE 9";
char	france_name_10[] = "FRANCE 10";
char	france_name_11[] = "FRANCE 11";
char	france_name_12[] = "FRANCE 12";
char	france_name_13[] = "FRANCE ECA";

pricing_t	france_pricing[] = {
	{ 2, 4, 0, 0, 0, 1, 4, 0, 2, 2, 32, 1, 0, 5, 10, 0, 0, 0, 4,
	  "2 CREDITS / 5F\n5 CREDITS / 10F", france_name_1, "5 F\t10 F\t-\t-\t-" },
	{ 2, 4, 0, 0, 0, 1, 0, 0, 2, 1, 32, 1, 0, 5, 10, 0, 0, 0, 4,
	  "2 CREDITS / 5F\n4 CREDITS / 10F", france_name_2, "5 F\t10 F\t-\t-\t-" },
	{ 1, 2, 0, 0, 0, 1, 2, 0, 2, 1, 32, 1, 0, 5, 10, 0, 0, 0, 4,
	  "1 CREDIT / 5F\n3 CREDITS / 10F", france_name_3, "5 F\t10 F\t-\t-\t-" },
	{ 1, 2, 0, 0, 0, 1, 0, 0, 2, 1, 32, 1, 0, 5, 10, 0, 0, 0, 4,
	  "1 CREDIT / 5F\n2 CREDITS / 10F", france_name_4, "5 F\t10 F\t-\t-\t-" },
	{ 5, 10, 0, 0, 0, 2, 20, 0, 2, 1, 32, 1, 0, 5, 10, 0, 0, 0, 4,
	  "2 CREDITS / 5F\n5 CREDITS / 10F\n11 CREDITS / 2 x 10F", france_name_5, "5 F\t10 F\t-\t-\t-" },
	{ 2, 4, 0, 0, 0, 1, 8, 0, 2, 1, 32, 1, 0, 5, 10, 0, 0, 0, 4,
	  "2 CREDITS / 5F\n4 CREDITS / 10F\n9 CREDITS / 2 x 10F", france_name_6, "5 F\t10 F\t-\t-\t-" },
	{ 3, 6, 0, 0, 0, 2, 12, 0, 2, 1, 32, 1, 0, 5, 10, 0, 0, 0, 4,
	  "1 CREDIT / 5F\n3 CREDITS / 10F\n7 CREDITS / 2 x 10F", france_name_7, "5 F\t10 F\t-\t-\t-" },
	{ 1, 2, 0, 0, 0, 1, 4, 0, 2, 1, 32, 1, 0, 5, 10, 0, 0, 0, 4,
	  "1 CREDIT / 5F\n2 CREDITS / 10F\n5 CREDITS / 2 x 10F", france_name_8, "5 F\t10 F\t-\t-\t-" },
	{ 2, 10, 0, 0, 0, 5, 0, 0, 2, 1, 32, 1, 0, 1, 5, 0, 0, 0, 4,
	  "1 CREDIT / 3 x 1F\n2 CREDITS / 5F", france_name_9, "1 F\t5 F\t-\t-\t-" },
	{ 3, 15, 0, 0, 0, 5, 0, 0, 2, 1, 32, 1, 0, 1, 5, 0, 0, 0, 4,
	  "1 CREDIT / 2 x 1F\n3 CREDITS / 5F", france_name_10, "1 F\t5 F\t-\t-\t-" },
	{ 2, 10, 0, 0, 0, 5, 20, 0, 2, 1, 32, 1, 0, 1, 5, 0, 0, 0, 4,
	  "1 CREDIT / 3 x 1F\n2 CREDITS / 5F\n5 CREDITS / 10F", france_name_11, "1 F\t5 F\t-\t-\t-" },
	{ 3, 15, 0, 0, 0, 5, 30, 0, 2, 1, 32, 1, 0, 1, 5, 0, 0, 0, 4,
	  "1 CREDIT / 2 x 1F\n3 CREDITS / 5F\n7 CREDITS / 2 x 5F", france_name_12, "1 F\t5 F\t-\t-\t-" },
	{ 1, 5, 10, 20, 0, 10, 30, 0, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "1 CREDIT / 10F\n2 CREDITS / 20F\n4 CREDITS / 30F", france_name_13, "1 F\t5 F\t10 F\t20 F\t-" }
};

char	france_eca_name_1[] = "FRANCE ECA 1";
char	france_eca_name_2[] = "FRANCE ECA 2";
char	france_eca_name_3[] = "FRANCE ECA 3";
char	france_eca_name_4[] = "FRANCE ECA 4";
char	france_eca_name_5[] = "FRANCE ECA 5";
char	france_eca_name_6[] = "FRANCE ECA 6";
char	france_eca_name_7[] = "FRANCE ECA 7";
char	france_eca_name_8[] = "FRANCE ECA 8";
char	france_eca_name_9[] = "FRANCE ECA 9";
char	france_eca_name_10[] = "FRANCE ECA 10";
char	france_eca_name_11[] = "FRANCE ECA 11";
char	france_eca_name_12[] = "FRANCE ECA 12";
char	france_eca_name_13[] = "FRANCE ECA 13";

pricing_t	france_eca_pricing[] = {
	{ 1, 5, 10, 20, 0, 2, 0, 5, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "2 CREDITS / 5F\n5 CREDITS / 10F", france_eca_name_1, "1 F\t5 F\t10 F\t20 F\t-" },
	{ 2, 10, 20, 40, 0, 5, 0, 10, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "2 CREDITS / 5F\n4 CREDITS / 10F", france_eca_name_2, "1 F\t5 F\t10 F\t20 F\t-" },
	{ 1, 5, 10, 20, 0, 5, 10, 0, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "1 CREDIT / 5F\n3 CREDITS / 10F", france_eca_name_3, "1 F\t5 F\t10 F\t20 F\t-" },
	{ 1, 5, 10, 20, 0, 5, 0, 0, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "1 CREDIT / 5F\n2 CREDITS / 10F", france_eca_name_4, "1 F\t5 F\t10 F\t20 F\t-" },
	{ 1, 5, 10, 20, 0, 2, 20, 5, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "2 CREDITS / 5F\n5 CREDITS / 10F\n11 CREDITS / 20F", france_eca_name_5, "1 F\t5 F\t10 F\t20 F\t-" },
	{ 2, 10, 20, 40, 0, 5, 40, 10, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "2 CREDITS / 5F\n4 CREDITS / 10F\n9 CREDITS / 20F", france_eca_name_6, "1 F\t5 F\t10 F\t20 F\t-" },
	{ 3, 15, 30, 60, 0, 10, 60, 15, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "1 CREDIT / 5F\n3 CREDITS / 10F\n7 CREDITS / 20F", france_eca_name_7, "1 F\t5 F\t10 F\t20 F\t-" },
	{ 1, 5, 10, 20, 0, 5, 20, 0, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "1 CREDIT / 5F\n2 CREDITS / 10F\n5 CREDITS / 20F", france_eca_name_8, "1 F\t5 F\t10 F\t20 F\t-" },
	{ 2, 10, 20, 40, 0, 5, 0, 0, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "1 CREDIT / 3 x 1F\n2 CREDITS / 5F", france_eca_name_9, "1 F\t5 F\t10 F\t20 F\t-" },
	{ 3, 15, 30, 60, 0, 5, 0, 0, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "1 CREDIT / 2 x 1F\n3 CREDITS / 5F", france_eca_name_10, "1 F\t5 F\t10 F\t20 F\t-" },
	{ 2, 10, 20, 40, 0, 5, 20, 0, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "1 CREDIT / 3 x 1F\n2 CREDITS / 5F\n5 CREDITS / 10F", france_eca_name_11, "1 F\t5 F\t10 F\t20 F\t-" },
	{ 3, 15, 30, 60, 0, 5, 30, 9, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "1 CREDIT / 3 x 1F\n3 CREDITS / 5F\n7 CREDITS / 10F", france_eca_name_12, "1 F\t5 F\t10 F\t20 F\t-" },
	{ 1, 5, 10, 20, 0, 10, 30, 0, 1, 1, 32, 1, 0, 1, 5, 10, 20, 0, 4,
	  "1 CREDIT / 10F\n2 CREDITS / 20F\n4 CREDITS / 30F", france_eca_name_13, "1 F\t5 F\t10 F\t20 F\t-" }
};

char	germany_name_1[] = "GERMAN 1";
char	germany_name_2[] = "GERMAN 2";
char	germany_name_3[] = "GERMAN 3";
char	germany_name_4[] = "GERMAN 4";
char	germany_name_5[] = "GERMAN 5";
char	germany_name_6[] = "GERMAN ECA";

pricing_t	germany_pricing[] = {
	{ 6, 30, 0, 0, 0, 5, 0, 0, 2, 2, 32, 1, 0, 1, 5, 0, 0, 0, 7,
	  "1 CREDIT / 1DM\n6 CREDITS / 5DM", germany_name_1, "1 DM\t5 DM\t-\t-\t-" },
	{ 6, 30, 0, 0, 0, 5, 30, 0, 2, 1, 32, 1, 0, 1, 5, 0, 0, 0, 7,
	  "1 CREDIT / 1DM\n7 CREDITS / 5DM", germany_name_2, "1 DM\t5 DM\t-\t-\t-" },
	{ 7, 35, 0, 0, 0, 5, 35, 0, 2, 1, 32, 1, 0, 1, 5, 0, 0, 0, 7,
	  "1 CREDIT / 1DM\n8 CREDITS / 5DM", germany_name_3, "1 DM\t5 DM\t-\t-\t-" },
	{ 1, 5, 0, 0, 0, 1, 0, 0, 2, 1, 32, 1, 0, 1, 5, 0, 0, 0, 7,
	  "1 CREDIT / 1DM\n5 CREDITS / 5DM", germany_name_4, "1 DM\t5 DM\t-\t-\t-" },
	{ 6, 30, 0, 0, 0, 5, 0, 0, 2, 1, 32, 1, 0, 1, 5, 0, 0, 0, 7,
	  "1 CREDIT / 1DM\n6 CREDITS / 5DM", germany_name_5, "1 DM\t5 DM\t-\t-\t-" },
	{ 60, 24, 12, 0, 0, 10, 0, 0, 2, 2, 32, 1, 0, 5, 2, 1, 0, 0, 7,
	  "1 CREDIT / 1DM\n2 CREDITS / 2DM\n6 CREDITS / 5DM", germany_name_6, "5 DM\t2 DM\t1 DM\t-\t-" }
};

char	italy_name_1[] = "ITALY";

pricing_t	italy_pricing[] = {
	{ 1, 1, 0, 0, 0, 1, 0, 0, 2, 2, 32, 1, 0, 1, 1, 0, 0, 0, 8,
	  "1 CREDIT / 500 LIRE", italy_name_1, "500 L\t500 L\t-\t-\t-" }
};

char	hungary_name_1[] = "HUNGARY";

pricing_t	hungary_pricing[] = {
	{ 1, 2, 0, 0, 0, 2, 4, 0, 2, 2, 32, 1, 0, 10, 20, 0, 0, 0, 9,
	  "1 CREDIT / 2 x 10 FORINT\n3 CREDITS / 2 x 20 FORINT", hungary_name_1, "10 FORINT\t20 FORINT\t-\t-\t-" }
};

char	netherlands_name_1[] = "NETHERLANDS";

pricing_t	netherlands_pricing[] = {
	{ 1, 3, 0, 0, 0, 1, 0, 0, 2, 2, 32, 1, 0, 2, 5, 0, 0, 0, 10,
	  "1 CREDIT / 1 HFI\n3 CREDITS / 2.5 HFI", netherlands_name_1, "1 HFI\t2.5 HFI\t-\t-\t-" }
};

char	norway_name_1[] = "NORWAY";

pricing_t	norway_pricing[] = {
	{ 3, 3, 0, 0, 0, 5, 0, 0, 2, 2, 32, 1, 1, 1, 1, 0, 0, 0, 5,
	  "1 CREDIT / 2 x 1 KRONE\n3 CREDITS / 5 x 1 KRONE", norway_name_1, "1 KR\t1 KR\t-\t-\t-" }
};

char	spain_name_1[] = "SPAIN 1";
char	spain_name_2[] = "SPAIN 2";

pricing_t	spain_pricing[] = {
	{ 1, 5, 0, 0, 0, 1, 5, 0, 2, 2, 32, 1, 0, 1, 5, 0, 0, 0, 11,
	  "1 CREDIT / 100 PESETA\n6 CREDITS / 500 PESETA", spain_name_1, "100 P\t500 P\t-\t-\t-" },
	{ 1, 5, 0, 0, 0, 1, 0, 0, 2, 2, 32, 1, 0, 1, 5, 0, 0, 0, 11,
	  "1 CREDIT / 100 PESETA\n5 CREDITS / 500 PESETA", spain_name_2, "100 P\t500 P\t-\t-\t-" }
};

char	sweden_name_1[] = "SWEDEN";

pricing_t	sweden_pricing[] = {
	{ 2, 10, 0, 0, 0, 5, 0, 0, 2, 2, 32, 1, 1, 1, 5, 0, 0, 0, 4,
	  "1 CREDIT / 3 x 1 KRONA\n2 CREDITS / 5 KRONA", sweden_name_1, "1 KR\t5 KR\t-\t-\t-" }
};

char	switzerland_name_1[] = "SWISS 1";
char	switzerland_name_2[] = "SWISS 2";
char	switzerland_name_3[] = "SWISS 3";

pricing_t	switzerland_pricing[] = {
	{ 6, 30, 0, 0, 0, 5, 0, 0, 2, 2, 32, 1, 0, 1, 5, 0, 0, 0, 4,
	  "1 CREDIT / 1F\n6 CREDITS / 5F", switzerland_name_1, "1 F\t5 F\t-\t-\t-" },
	{ 6, 30, 0, 0, 0, 5, 30, 0, 2, 2, 32, 1, 0, 1, 5, 0, 0, 0, 4,
	  "1 CREDIT / 1F\n7 CREDITS / 5F", switzerland_name_2, "1 F\t5 F\t-\t-\t-" },
	{ 7, 35, 0, 0, 0, 5, 35, 0, 2, 2, 32, 1, 0, 1, 5, 0, 0, 0, 4,
	  "1 CREDIT / 1F\n8 CREDITS / 5F", switzerland_name_3, "1 F\t5 F\t-\t-\t-" }
};

char	australia_name_1[] = "AUSTRALIA 1";
char	australia_name_2[] = "AUSTRALIA 2";

pricing_t	australia_pricing[] = {
	{ 2, 10, 0, 0, 0, 5, 0, 0, 2, 2, 32, 5, 0, 1, 5, 0, 0, 0, 0,
	  "1 CREDIT / 3 x 20 CENTS\n2 CREDITS / $1.00", australia_name_1, ".20\t$1.00\t-\t-\t-" },
	{ 1, 5, 0, 0, 0, 5, 0, 0, 1, 1, 32, 5, 0, 1, 5, 0, 0, 0, 0,
	  "1 CREDIT / 5 x 20 CENTS\n1 CREDIT / $1.00", australia_name_2, ".20\t$1.00\t-\t-\t-" }
};

char	new_zealand_name_1[] = "NEW ZEALAND 1";
char	new_zealand_name_2[] = "NEW ZEALAND 2";

pricing_t	new_zealand_pricing[] = {
	{ 1, 1, 0, 0, 0, 3, 0, 0, 1, 1, 32, 5, 1, 1, 1, 0, 0, 0, 0,
	  "1 CREDIT / 3 x 20 CENTS", new_zealand_name_1, ".20\t.20\t-\t-\t-" },
	{ 1, 1, 0, 0, 0, 2, 0, 0, 1, 1, 32, 5, 1, 1, 1, 0, 0, 0, 0,
	  "1 CREDIT / 2 x 20 CENTS", new_zealand_name_2, ".20\t.20\t-\t-\t-" }
};

char	japan_name_1[] = "JAPAN 1";
char	japan_name_2[] = "JAPAN 2";

pricing_t	japan_pricing[] = {
	{ 1, 1, 0, 0, 0, 1, 0, 0, 2, 2, 32, 1, 1, 1, 1, 0, 0, 0, 12,
	  "1 CREDIT / 100 YEN", japan_name_1, "100 Y\t100 Y\t-\t-\t-" },
	{ 2, 2, 0, 0, 0, 1, 0, 0, 2, 2, 32, 1, 1, 1, 1, 0, 0, 0, 12,
	  "2 CREDITS / 100 YEN", japan_name_2, "100 Y\t100 Y\t-\t-\t-" }
};

pricing_t	*country_pricing[] = {
	usa_pricing, canada_pricing,
	antilles_pricing,
	austria_pricing, belgium_pricing, denmark_pricing, finland_pricing, france_pricing,
	france_eca_pricing, germany_pricing, hungary_pricing,
	italy_pricing, netherlands_pricing, norway_pricing, spain_pricing, sweden_pricing,
	switzerland_pricing, uk_pricing,
	japan_pricing,
	australia_pricing, new_zealand_pricing
};

/* each continent's countries, in its submenu's order */
pricing_t	**continent_pricing[] = {
	&country_pricing[0], &country_pricing[2], &country_pricing[3], &country_pricing[11],
	&country_pricing[18], &country_pricing[19]
};

static menu_item_t	price_items[16];	/* up to 14 modes, EXIT and the end */
static pricing_t	*prices;		/* the chosen country's modes */
menu_t	price_menu = { "", 51.0F, -2.0F, 0, 0, price_items };
int	price_chosen = 0;

/* 0x8010f650: a mode's item - write it to the adjustments, each tried twice */
int choose_price(int n)
{
	char	buf[64];
	int	err, i;
	int	*p;

	err = 0;
	if(!((get_dip_coin_current() >> 16) & 1))
		return 0;
	if(set_adjustment(CONTINENT_ADJ, pricing_continent) && set_adjustment(CONTINENT_ADJ, pricing_continent))
		err |= 1;
	if(pricing_continent != 6 && set_adjustment(COUNTRY_ADJ, pricing_country) && set_adjustment(COUNTRY_ADJ, pricing_country))
		err |= 2;
	if(set_adjustment(PRICE_ADJ, n))
	{
		if(set_adjustment(PRICE_ADJ, n))
			err |= 4;
	}
	else
	{
		/* the whole record, so its three pointers go to PRICING_INFO_STRING_ADJ..+2 until
		   the info text below overwrites them */
		p = (int *)&prices[n];
		i = 0;
		while(i < sizeof(pricing_t) / sizeof(int))
		{
			if(set_adjustment(LEFT_SLOT_UNITS_ADJ + i, *p) && set_adjustment(LEFT_SLOT_UNITS_ADJ + i, *p))
				err |= 8 << i;
			i++;
			p++;
		}
		p = (int *)prices[n].info;
		i = 0;
		while(i < 20)
		{
			if(set_adjustment(PRICING_INFO_STRING_ADJ + i, *p))
				err |= 8 << i;
			i++;
			p++;
		}
	}
	if(!err)
	{
		graphical_printf(buf, "\x1b%dc%s\x1b%dc PRICING SET", 0xffffff00, prices[n].name, 0xff00ffff);
		price_chosen = -1;
	}
	else
		graphical_printf(buf, "\x1b%dcERROR SETTING PRICING", 0xffff0000);
	message_box(buf, -1.0F, -1.0F, 1.0F);
	wait_test_exit();
	return price_chosen;
}

/* 0x8010f8a4: a country's item - list its pricing modes */
int set_pricing(int country)
{
	pricing_t	*p;
	int		count, i;

	count = 0;
	price_chosen = 0;
	price_menu.sel = 0;
	switch(pricing_continent)
	{
		case 0:
			switch(country)
			{
				case 0:
				default:
					count = 14;
					prices = usa_pricing;
					price_menu.title = "USA";
					break;
				case 1:
					count = 1;
					prices = canada_pricing;
					price_menu.title = "CANADIAN";
					break;
			}
			break;
		case 1:
			switch(country)
			{
				case 0:
					count = 1;
					prices = antilles_pricing;
					price_menu.title = "ANTILLIAN";
					break;
				default:
					count = 14;
					prices = usa_pricing;
					price_menu.title = "USA";
					break;
			}
			break;
		case 2:
			switch(country)
			{
				case 0:
					count = 2;
					prices = austria_pricing;
					price_menu.title = "AUSTRIA";
					break;
				case 1:
					count = 4;
					prices = belgium_pricing;
					price_menu.title = "BELGIUM";
					break;
				case 2:
					count = 1;
					prices = denmark_pricing;
					price_menu.title = "DENMARK";
					break;
				case 3:
					count = 1;
					prices = finland_pricing;
					price_menu.title = "FINLAND";
					break;
				case 4:
					count = 13;
					prices = france_pricing;
					price_menu.title = "FRANCE";
					break;
				case 5:
					count = 13;
					prices = france_eca_pricing;
					price_menu.title = "FRANCE ECA";
					break;
				case 6:
					count = 6;
					prices = germany_pricing;
					price_menu.title = "GERMANY";
					break;
				case 7:
					count = 1;
					prices = hungary_pricing;
					price_menu.title = "HUNGARY";
					break;
				default:
					count = 14;
					prices = usa_pricing;
					price_menu.title = "USA";
					break;
			}
			break;
		case 3:
			switch(country)
			{
				case 0:
					count = 1;
					prices = italy_pricing;
					price_menu.title = "ITALY";
					break;
				case 1:
					count = 1;
					prices = netherlands_pricing;
					price_menu.title = "NETHERLANDS";
					break;
				case 2:
					count = 1;
					prices = norway_pricing;
					price_menu.title = "NORWAY";
					break;
				case 3:
					count = 2;
					prices = spain_pricing;
					price_menu.title = "SPAIN";
					break;
				case 4:
					count = 1;
					prices = sweden_pricing;
					price_menu.title = "SWEDEN";
					break;
				case 5:
					count = 3;
					prices = switzerland_pricing;
					price_menu.title = "SWITZERLAND";
					break;
				case 6:
					count = 7;
					prices = uk_pricing;
					price_menu.title = "U.K.";
					break;
				default:
					count = 14;
					prices = usa_pricing;
					price_menu.title = "USA";
					break;
			}
			break;
		case 4:
			switch(country)
			{
				case 0:
					count = 2;
					prices = japan_pricing;
					price_menu.title = "JAPAN";
					break;
				default:
					count = 14;
					prices = usa_pricing;
					price_menu.title = "USA";
					break;
			}
			break;
		case 5:
			switch(country)
			{
				case 0:
					count = 2;
					prices = australia_pricing;
					price_menu.title = "AUSTRALIA";
					break;
				case 1:
					count = 2;
					prices = new_zealand_pricing;
					price_menu.title = "NEW ZEALAND";
					break;
				default:
					count = 14;
					prices = usa_pricing;
					price_menu.title = "USA";
					break;
			}
			break;
		case 6:
			load_custom_pricing(&count, &prices);
			price_menu.title = "CUSTOM";
			break;
		default:
			count = 14;
			prices = usa_pricing;
			price_menu.title = "USA";
			break;
	}
	p = prices;
	for(i = 0; i < count; p++, i++)
	{
		price_items[i].name = p->name;
		price_items[i].kind = MENU_FUNC;
		price_items[i].target = choose_price;
		price_items[i].help = 0;
		price_items[i].hook = show_price;
	}
	price_items[i].name = "EXIT";
	price_items[i].kind = MENU_EXIT;
	price_items[i].help = "RETURN TO PREVIOUS MENU";
	price_items[i].target = 0;
	price_items[i].hook = 0;
	price_items[i + 1].name = 0;
	price_items[i + 1].kind = -1;
	price_items[i + 1].target = 0;
	price_items[i + 1].help = 0;
	price_items[i + 1].hook = 0;
	price_menu.x = hres * 0.1F;
	price_menu.y = -2.0F;
	do_menu(&price_menu);
	if(pricing_continent == 6)
		free_custom_pricing(count, prices);
	return price_chosen;
}

/* 0x8010fe60: a mode's hook - show it while it is highlighted */
void show_price(int n)
{
	char		buf[128];
	pricing_t	*p;
	float		x, y;

	p = &prices[n];
	x = hres / 2.0F;
	y = vres - 60.0F;
	draw_text_box(p->name, x, vres - 40.0F, 1.0F);
	set_text_font(1);

	set_text_position(x, y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("LEFT SLOT UNITS");
	set_text_position(hres * 0.9F, y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->left_slot_units);
	y -= font1_height * 1.5F;

	set_text_position(x, y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("RIGHT SLOT UNITS");
	set_text_position(hres * 0.9F, y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->right_slot_units);
	y -= font1_height * 1.5F;

	set_text_position(x, y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("CENTER SLOT UNITS");
	set_text_position(hres * 0.9F, y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->center_slot_units);
	y -= font1_height * 1.5F;

	set_text_position(x, y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("EXTRA SLOT UNITS");
	set_text_position(hres * 0.9F, y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->extra_slot_units);
	y -= font1_height * 1.5F;

	set_text_position(x, y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("BILL VALIDTOR UNITS");
	set_text_position(hres * 0.9F, y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->bill_validator_units);
	y -= font1_height * 1.5F;

	set_text_position(x, y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("UNITS PER CREDIT");
	set_text_position(hres * 0.9F, y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->units_per_credit);
	y -= font1_height * 1.5F;

	set_text_position(x, y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("UNITS PER BONUS");
	set_text_position(hres * 0.9F, y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->units_per_bonus);
	y -= font1_height * 1.5F;

	set_text_position(x, y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("MINIMUM UNITS");
	set_text_position(hres * 0.9F, y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->minimum_units);
	y -= font1_height * 1.5F;

	set_text_position(x, y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("CREDITS TO START");
	set_text_position(hres * 0.9F, y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->credits_to_start);
	y -= font1_height * 1.5F;

	set_text_position(x, y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("CREDITS TO CONTINUE");
	set_text_position(hres * 0.9F, y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->credits_to_continue);
	y -= font1_height * 1.5F;

	set_text_position(x, y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("MAX CREDITS");
	set_text_position(hres * 0.9F, y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->maximum_credits);
	y -= font1_height * 1.5F;

	set_text_position(x, y, 1.0F);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_LEFT);
	oprintf("COINS PER BILL");
	set_text_position(hres * 0.9F, y, 1.0F);
	set_text_color(0xffffffff);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("%d", p->coins_per_bill);

	if(!((get_dip_coin_current() >> 16) & 1))
		graphical_printf(buf, "\x1b%dcDIPSWITCH COINAGE MUST BE DISABLED TO SELECT PRICING", 0xffffff00);
	else
		graphical_printf(buf, "PRESS ANY BUTTON TO SELECT \x1b%dc%s", 0xffffff00, p->name);
	message_box(buf, -1.0F, 5.0F, 1.0F);
}
