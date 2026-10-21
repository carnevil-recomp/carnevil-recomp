/*
 * rtcset_hour.c - SET RTC HOUR (0x8010e428-0x8010e718): the hour, in four pages of six.
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

int	hour_select(int);
int	hour_page(void);
int	hour_set(void);
void	hour_accept_help(int);

static int	hour_accept = 0;		/* the ACCEPT items are in */
static int	hour_pg = 0;			/* the page shown */

char	help_hour_12_am[] = "SET HOUR TO 12 AM";
char	help_hour_1_am[] = "SET HOUR TO 1 AM";
char	help_hour_2_am[] = "SET HOUR TO 2 AM";
char	help_hour_3_am[] = "SET HOUR TO 3 AM";
char	help_hour_4_am[] = "SET HOUR TO 4 AM";
char	help_hour_5_am[] = "SET HOUR TO 5 AM";
char	help_hour_6_am[] = "SET HOUR TO 6 AM";
char	help_hour_7_am[] = "SET HOUR TO 7 AM";
char	help_hour_8_am[] = "SET HOUR TO 8 AM";
char	help_hour_9_am[] = "SET HOUR TO 9 AM";
char	help_hour_10_am[] = "SET HOUR TO 10 AM";
char	help_hour_11_am[] = "SET HOUR TO 11 AM";
char	help_hour_12_pm[] = "SET HOUR TO 12 PM";
char	help_hour_1_pm[] = "SET HOUR TO 1 PM";
char	help_hour_2_pm[] = "SET HOUR TO 2 PM";
char	help_hour_3_pm[] = "SET HOUR TO 3 PM";
char	help_hour_4_pm[] = "SET HOUR TO 4 PM";
char	help_hour_5_pm[] = "SET HOUR TO 5 PM";
char	help_hour_6_pm[] = "SET HOUR TO 6 PM";
char	help_hour_7_pm[] = "SET HOUR TO 7 PM";
char	help_hour_8_pm[] = "SET HOUR TO 8 PM";
char	help_hour_9_pm[] = "SET HOUR TO 9 PM";
char	help_hour_10_pm[] = "SET HOUR TO 10 PM";
char	help_hour_11_pm[] = "SET HOUR TO 11 PM";
char	help_hour_6_am_to_11_pm[] = "6 AM TO 11 PM";
char	help_hour_12_pm_to_11_pm[] = "12 PM TO 11 PM";
char	help_hour_6pm_to_11_pm[] = "6PM TO 11 PM";
char	help_hour_12_am_to_11_pm[] = "12 AM TO 11 PM";
char	help_hour_exit[] = "RETURN TO SET REAL TIME CLOCK MENU (NO SET)";

/* 12 AM and 12 PM have each other's help line */
menu_item_t	rtc_hour_items[4][10] = {
	{
		{ "12 AM", MENU_FUNC, hour_select, help_hour_12_pm, 0 },
		{ "1 AM", MENU_FUNC, hour_select, help_hour_1_am, 0 },
		{ "2 AM", MENU_FUNC, hour_select, help_hour_2_am, 0 },
		{ "3 AM", MENU_FUNC, hour_select, help_hour_3_am, 0 },
		{ "4 AM", MENU_FUNC, hour_select, help_hour_4_am, 0 },
		{ "5 AM", MENU_FUNC, hour_select, help_hour_5_am, 0 },
		{ "6 AM - 11 PM", MENU_FUNC, hour_page, help_hour_6_am_to_11_pm, 0 },
		{ "EXIT", MENU_EXIT, 0, help_hour_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "6 AM", MENU_FUNC, hour_select, help_hour_6_am, 0 },
		{ "7 AM", MENU_FUNC, hour_select, help_hour_7_am, 0 },
		{ "8 AM", MENU_FUNC, hour_select, help_hour_8_am, 0 },
		{ "9 AM", MENU_FUNC, hour_select, help_hour_9_am, 0 },
		{ "10 AM", MENU_FUNC, hour_select, help_hour_10_am, 0 },
		{ "11 AM", MENU_FUNC, hour_select, help_hour_11_am, 0 },
		{ "12 PM - 11 PM", MENU_FUNC, hour_page, help_hour_12_pm_to_11_pm, 0 },
		{ "EXIT", MENU_EXIT, 0, help_hour_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "12 PM", MENU_FUNC, hour_select, help_hour_12_am, 0 },
		{ "1 PM", MENU_FUNC, hour_select, help_hour_1_pm, 0 },
		{ "2 PM", MENU_FUNC, hour_select, help_hour_2_pm, 0 },
		{ "3 PM", MENU_FUNC, hour_select, help_hour_3_pm, 0 },
		{ "4 PM", MENU_FUNC, hour_select, help_hour_4_pm, 0 },
		{ "5 PM", MENU_FUNC, hour_select, help_hour_5_pm, 0 },
		{ "6 PM - 11 PM", MENU_FUNC, hour_page, help_hour_6pm_to_11_pm, 0 },
		{ "EXIT", MENU_EXIT, 0, help_hour_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "6 PM", MENU_FUNC, hour_select, help_hour_6_pm, 0 },
		{ "7 PM", MENU_FUNC, hour_select, help_hour_7_pm, 0 },
		{ "8 PM", MENU_FUNC, hour_select, help_hour_8_pm, 0 },
		{ "9 PM", MENU_FUNC, hour_select, help_hour_9_pm, 0 },
		{ "10 PM", MENU_FUNC, hour_select, help_hour_10_pm, 0 },
		{ "11 PM", MENU_FUNC, hour_select, help_hour_11_pm, 0 },
		{ "12 AM - 5 AM", MENU_FUNC, hour_page, help_hour_12_am_to_11_pm, 0 },
		{ "EXIT", MENU_EXIT, 0, help_hour_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	}
};

menu_t	rtc_hour_menu = { "SET RTC HOUR", -1.0F, -1.0F, 0, 6, rtc_hour_items[0] };

static struct dosdate_t	date;
static struct dostime_t	time;

/* 0x8010e428: a hour picked */
int hour_select(int n)
{
	int	i;

	_getdate(&date);
	_gettime(&time);
	time.hour = hour_pg * 6 + n;
	if(!hour_accept)
	{
		for(i = 0; i < 4; i++)
		{
			memcpy(&rtc_hour_items[i][8], &rtc_hour_items[i][7], sizeof(menu_item_t));
			rtc_hour_items[i][7].name = "ACCEPT";
			rtc_hour_items[i][7].kind = MENU_FUNC;
			rtc_hour_items[i][7].target = hour_set;
			rtc_hour_items[i][7].help = 0;
			rtc_hour_items[i][7].hook = hour_accept_help;
		}
		hour_accept = 1;
	}
	return 0;
}

/* 0x8010e570: the ACCEPT item's hook */
void hour_accept_help(int n)
{
	char	buf[128];

	fix_date_time();
	graphical_printf(buf, "ACCEPT \x1b%dc%d %s\x1b%dc AND SET REAL TIME CLOCK", 0xffffff00,
		time.hour, time.hour >= 12 ? "PM" : "AM", 0xff00ffff);
	message_box(buf, -1.0F, 5.0F, 1.0F);
}

/* 0x8010e5f4: ACCEPT */
int hour_set(void)
{
	int	i;

	_setdate(&date);
	_settime(&time);
	rtc_hour_menu.sel = 6;
	rtc_hour_menu.items = rtc_hour_items[0];
	for(i = 0; i < 4; i++)
	{
		memcpy(&rtc_hour_items[i][7], &rtc_hour_items[i][8], sizeof(menu_item_t));
		memcpy(&rtc_hour_items[i][8], &rtc_hour_items[i][9], sizeof(menu_item_t));
	}
	hour_pg = 0;
	hour_accept = 0;
	return 1;
}

/* 0x8010e6bc: the next page */
int hour_page(void)
{
	if(++hour_pg >= 4)
		hour_pg = 0;
	rtc_hour_menu.items = rtc_hour_items[hour_pg];
	return 0;
}
