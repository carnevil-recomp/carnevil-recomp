/*
 * rtcset_month.c - SET RTC MONTH (0x8010dbd8-0x8010de4c): the month, in two pages of six.
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
extern char	*month_names[];

int	month_select(int);
int	month_page(void);
int	month_set(void);
void	month_accept_help(int);

static int	month_accept = 0;		/* the ACCEPT items are in */
static int	month_pg = 0;			/* the page shown */

char	help_month_january[] = "SET MONTH TO JANUARY";
char	help_month_febuary[] = "SET MONTH TO FEBUARY";
char	help_month_march[] = "SET MONTH TO MARCH";
char	help_month_april[] = "SET MONTH TO APRIL";
char	help_month_may[] = "SET MONTH TO MAY";
char	help_month_june[] = "SET MONTH TO JUNE";
char	help_month_july[] = "SET MONTH TO JULY";
char	help_month_august[] = "SET MONTH TO AUGUST";
char	help_month_september[] = "SET MONTH TO SEPTEMBER";
char	help_month_october[] = "SET MONTH TO OCTOBER";
char	help_month_november[] = "SET MONTH TO NOVEMBER";
char	help_month_december[] = "SET MONTH TO DECEMBER";
char	help_month_back_to_january_to_june_menu[] = "BACK TO JANUARY TO JUNE MENU";
char	help_month_forward_to_july_to_december_menu[] = "FORWARD TO JULY TO DECEMBER MENU";
char	help_month_exit[] = "RETURN TO SET REAL TIME CLOCK MENU (NO SET)";

menu_item_t	rtc_month_items[2][10] = {
	{
		{ "JANUARY", MENU_FUNC, month_select, help_month_january, 0 },
		{ "FEBUARY", MENU_FUNC, month_select, help_month_febuary, 0 },
		{ "MARCH", MENU_FUNC, month_select, help_month_march, 0 },
		{ "APRIL", MENU_FUNC, month_select, help_month_april, 0 },
		{ "MAY", MENU_FUNC, month_select, help_month_may, 0 },
		{ "JUNE", MENU_FUNC, month_select, help_month_june, 0 },
		{ "JULY - DECEMBER", MENU_FUNC, month_page, help_month_forward_to_july_to_december_menu, 0 },
		{ "EXIT", MENU_EXIT, 0, help_month_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "JULY", MENU_FUNC, month_select, help_month_july, 0 },
		{ "AUGUST", MENU_FUNC, month_select, help_month_august, 0 },
		{ "SEPTEMBER", MENU_FUNC, month_select, help_month_september, 0 },
		{ "OCTOBER", MENU_FUNC, month_select, help_month_october, 0 },
		{ "NOVEMBER", MENU_FUNC, month_select, help_month_november, 0 },
		{ "DECEMBER", MENU_FUNC, month_select, help_month_december, 0 },
		{ "JANUARY - JUNE", MENU_FUNC, month_page, help_month_back_to_january_to_june_menu, 0 },
		{ "EXIT", MENU_EXIT, 0, help_month_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	}
};

menu_t	rtc_month_menu = { "SET RTC MONTH", -1.0F, -1.0F, 0, 0, rtc_month_items[0] };

static struct dosdate_t	date;
static struct dostime_t	time;

/* 0x8010dbd8: a month picked */
int month_select(int n)
{
	_getdate(&date);
	_gettime(&time);
	date.month = month_pg * 6 + n + 1;
	if(!month_accept)
	{
		memcpy(&rtc_month_items[0][8], &rtc_month_items[0][7], sizeof(menu_item_t));
		memcpy(&rtc_month_items[1][8], &rtc_month_items[1][7], sizeof(menu_item_t));
		rtc_month_items[0][7].name = "ACCEPT";
		rtc_month_items[0][7].kind = MENU_FUNC;
		rtc_month_items[0][7].target = month_set;
		rtc_month_items[0][7].help = 0;
		rtc_month_items[0][7].hook = month_accept_help;
		rtc_month_items[1][7].name = "ACCEPT";
		rtc_month_items[1][7].kind = MENU_FUNC;
		rtc_month_items[1][7].target = month_set;
		rtc_month_items[1][7].help = 0;
		rtc_month_items[1][7].hook = month_accept_help;
		month_accept = 1;
	}
	return 0;
}

/* 0x8010dce8: the ACCEPT item's hook */
void month_accept_help(int n)
{
	char	buf[128];

	fix_date_time();
	graphical_printf(buf, "ACCEPT \x1b%dc%s\x1b%dc AND SET REAL TIME CLOCK", 0xffffff00,
		month_names[date.month - 1], 0xff00ffff);
	message_box(buf, -1.0F, 5.0F, 1.0F);
}

/* 0x8010dd54: ACCEPT */
int month_set(void)
{
	_setdate(&date);
	_settime(&time);
	rtc_month_menu.sel = 0;
	rtc_month_menu.items = rtc_month_items[0];
	memcpy(&rtc_month_items[0][7], &rtc_month_items[0][8], sizeof(menu_item_t));
	memcpy(&rtc_month_items[0][8], &rtc_month_items[0][9], sizeof(menu_item_t));
	memcpy(&rtc_month_items[1][7], &rtc_month_items[1][8], sizeof(menu_item_t));
	memcpy(&rtc_month_items[1][8], &rtc_month_items[1][9], sizeof(menu_item_t));
	month_pg = 0;
	month_accept = 0;
	return 1;
}

/* 0x8010de0c: JULY - DECEMBER / JANUARY - JUNE */
int month_page(void)
{
	month_pg ^= 1;
	rtc_month_menu.items = rtc_month_items[month_pg];
	return 0;
}
