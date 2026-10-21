/*
 * rtcset_year.c - SET RTC YEARS (0x8010ece0-0x8010efc0): the year, 1997 to 2014 in three pages of six.
 */
#include <string.h>
#include <goose/sprites.h>
#include "diag.h"

struct dostime_t {			/* as the BIOS's GETTIME.C */
	unsigned char	hour, minute, second, hsecond;
};
struct dosdate_t {
	unsigned char	day, month;
	unsigned short	year;
	unsigned char	dayofweek;
};

extern void	_gettime(struct dostime_t *);
extern void	_getdate(struct dosdate_t *);
extern void	_settime(struct dostime_t *);
extern void	_setdate(struct dosdate_t *);
extern int	graphical_printf(char *, char *, ...);
extern void	fix_date_time(void);		/* rtcset_day.c: checks that file's copy of the clock, not this one */

int	year_select(int);
int	year_page(void);
int	year_set(void);
void	year_accept_help(int);

static int	year_accept = 0;		/* the ACCEPT items are in */
static int	year_pg = 0;			/* the page shown */

char	help_year_1997[] = "SET YEAR TO 1997";
char	help_year_1998[] = "SET YEAR TO 1998";
char	help_year_1999[] = "SET YEAR TO 1999";
char	help_year_2000[] = "SET YEAR TO 2000";
char	help_year_2001[] = "SET YEAR TO 2001";
char	help_year_2002[] = "SET YEAR TO 2002";
char	help_year_2003[] = "SET YEAR TO 2003";
char	help_year_2004[] = "SET YEAR TO 2004";
char	help_year_2005[] = "SET YEAR TO 2005";
char	help_year_2006[] = "SET YEAR TO 2006";
char	help_year_2007[] = "SET YEAR TO 2007";
char	help_year_2008[] = "SET YEAR TO 2008";
char	help_year_2009[] = "SET YEAR TO 2009";
char	help_year_2010[] = "SET YEAR TO 2010";
char	help_year_2011[] = "SET YEAR TO 2011";
char	help_year_2012[] = "SET YEAR TO 2012";
char	help_year_2013[] = "SET YEAR TO 2013";
char	help_year_2014[] = "SET YEAR TO 2014";
char	help_year_years_2003_to_2014[] = "YEARS 2003 TO 2014";
char	help_year_years_2009_to_2014[] = "YEARS 2009 TO 2014";
char	help_year_years_1997_to_2014[] = "YEARS 1997 TO 2014";
char	help_year_exit[] = "RETURN TO SET REAL TIME CLOCK MENU (NO SET)";

menu_item_t	rtc_year_items[3][10] = {
	{
		{ "YEAR 1997", MENU_FUNC, year_select, help_year_1997, 0 },
		{ "YEAR 1998", MENU_FUNC, year_select, help_year_1998, 0 },
		{ "YEAR 1999", MENU_FUNC, year_select, help_year_1999, 0 },
		{ "YEAR 2000", MENU_FUNC, year_select, help_year_2000, 0 },
		{ "YEAR 2001", MENU_FUNC, year_select, help_year_2001, 0 },
		{ "YEAR 2002", MENU_FUNC, year_select, help_year_2002, 0 },
		{ "YEARS 2003 - 2014", MENU_FUNC, year_page, help_year_years_2003_to_2014, 0 },
		{ "EXIT", MENU_EXIT, 0, help_year_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "YEAR 2003", MENU_FUNC, year_select, help_year_2003, 0 },
		{ "YEAR 2004", MENU_FUNC, year_select, help_year_2004, 0 },
		{ "YEAR 2005", MENU_FUNC, year_select, help_year_2005, 0 },
		{ "YEAR 2006", MENU_FUNC, year_select, help_year_2006, 0 },
		{ "YEAR 2007", MENU_FUNC, year_select, help_year_2007, 0 },
		{ "YEAR 2008", MENU_FUNC, year_select, help_year_2008, 0 },
		{ "YEARS 2009 - 2014", MENU_FUNC, year_page, help_year_years_2009_to_2014, 0 },
		{ "EXIT", MENU_EXIT, 0, help_year_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "YEAR 2009", MENU_FUNC, year_select, help_year_2009, 0 },
		{ "YEAR 2010", MENU_FUNC, year_select, help_year_2010, 0 },
		{ "YEAR 2011", MENU_FUNC, year_select, help_year_2011, 0 },
		{ "YEAR 2012", MENU_FUNC, year_select, help_year_2012, 0 },
		{ "YEAR 2013", MENU_FUNC, year_select, help_year_2013, 0 },
		{ "YEAR 2014", MENU_FUNC, year_select, help_year_2014, 0 },
		{ "YEARS 1997 - 2014", MENU_FUNC, year_page, help_year_years_1997_to_2014, 0 },
		{ "EXIT", MENU_EXIT, 0, help_year_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	}
};

menu_t	rtc_year_menu = { "SET RTC YEARS", -1.0F, -1.0F, 0, 6, rtc_year_items[0] };

static struct dosdate_t	date;
static struct dostime_t	time;

/* 0x8010ece0: a year picked */
int year_select(int n)
{
	int	i;

	_getdate(&date);
	_gettime(&time);
	date.year = year_pg * 6 + n + 1997;
	if(!year_accept)
	{
		for(i = 0; i < 3; i++)
		{
			memcpy(&rtc_year_items[i][8], &rtc_year_items[i][7], sizeof(menu_item_t));
			rtc_year_items[i][7].name = "ACCEPT";
			rtc_year_items[i][7].kind = MENU_FUNC;
			rtc_year_items[i][7].target = year_set;
			rtc_year_items[i][7].help = 0;
			rtc_year_items[i][7].hook = year_accept_help;
		}
		year_accept = 1;
	}
	rtc_year_menu.sel = 7;
	return 0;
}

/* 0x8010ee40: the ACCEPT item's hook */
void year_accept_help(int n)
{
	char	buf[128];

	fix_date_time();
	graphical_printf(buf, "ACCEPT \x1b%dc%d\x1b%dc AND SET REAL TIME CLOCK", 0xffffff00,
		date.year, 0xff00ffff);
	message_box(buf, -1.0F, 5.0F, 1.0F);
}

/* 0x8010ee9c: ACCEPT */
int year_set(void)
{
	int	i;

	_setdate(&date);
	_settime(&time);
	rtc_year_menu.sel = 6;
	rtc_year_menu.items = rtc_year_items[0];
	for(i = 0; i < 3; i++)
	{
		memcpy(&rtc_year_items[i][7], &rtc_year_items[i][8], sizeof(menu_item_t));
		memcpy(&rtc_year_items[i][8], &rtc_year_items[i][9], sizeof(menu_item_t));
	}
	year_pg = 0;
	year_accept = 0;
	return 1;
}

/* 0x8010ef64: the next page */
int year_page(void)
{
	if(++year_pg >= 3)
		year_pg = 0;
	rtc_year_menu.items = rtc_year_items[year_pg];
	return 0;
}
