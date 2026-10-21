/*
 * rtcset_day.c - SET RTC DAY (0x8010d888-0x8010dbd4): the day of the week. Picking a day adds
 * an ACCEPT item above EXIT that sets the clock; this file also has fix_date_time, which the
 * other SET RTC files call (on this file's copy of the clock).
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
extern char	*day_names[];

int	day_select(int);
int	day_set(void);
void	day_accept_help(int);

static int	day_accept = 0;		/* the ACCEPT item is in */

char	help_day_sunday[] = "SET DAY TO SUNDAY";
char	help_day_monday[] = "SET DAY TO MONDAY";
char	help_day_tuesday[] = "SET DAY TO TUESDAY";
char	help_day_wednesday[] = "SET DAY TO WEDNESDAY";
char	help_day_thursday[] = "SET DAY TO THURSDAY";
char	help_day_friday[] = "SET DAY TO FRIDAY";
char	help_day_saturday[] = "SET DAY TO SATURDAY";
char	help_day_exit[] = "RETURN TO SET REAL TIME CLOCK MENU (NO SET)";

menu_item_t	rtc_day_items[11] = {
	{ "SUNDAY", MENU_FUNC, day_select, help_day_sunday, 0 },
	{ "MONDAY", MENU_FUNC, day_select, help_day_monday, 0 },
	{ "TUESDAY", MENU_FUNC, day_select, help_day_tuesday, 0 },
	{ "WEDNESDAY", MENU_FUNC, day_select, help_day_wednesday, 0 },
	{ "THURSDAY", MENU_FUNC, day_select, help_day_thursday, 0 },
	{ "FRIDAY", MENU_FUNC, day_select, help_day_friday, 0 },
	{ "SATURDAY", MENU_FUNC, day_select, help_day_saturday, 0 },
	{ "EXIT", MENU_EXIT, 0, help_day_exit, 0 },
	{ 0, -1, 0, 0, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	rtc_day_menu = { "SET RTC DAY", -1.0F, -1.0F, 0, 0, rtc_day_items };

static struct dosdate_t	date;
static struct dostime_t	time;

/* 0x8010d888: a day picked */
int day_select(int n)
{
	_getdate(&date);
	_gettime(&time);
	date.dayofweek = n;
	if(!day_accept)
	{
		memcpy(&rtc_day_items[8], &rtc_day_items[7], sizeof(menu_item_t));
		rtc_day_items[7].name = "ACCEPT";
		rtc_day_items[7].kind = MENU_FUNC;
		rtc_day_items[7].target = day_set;
		rtc_day_items[7].help = 0;
		rtc_day_items[7].hook = day_accept_help;
		day_accept = 1;
	}
	return 0;
}

/* 0x8010d948: put what the clock can not hold back in range */
inline void fix_date_time(void)
{
	if(date.dayofweek >= 7)
		date.dayofweek = 0;
	if(!date.month || date.month >= 13)
		date.month = 1;
	if(!date.day || date.day >= 32)
		date.day = 1;
	if(date.year < 1980 || date.year >= 2100)
		date.year = 1980;
	if(time.hour >= 24)
		time.hour = 0;
	if(time.minute >= 60)
		time.minute = 0;
	if(time.second >= 60)
		time.second = 0;
}

/* 0x8010da20: the ACCEPT item's hook */
void day_accept_help(int n)
{
	char	buf[128];

	fix_date_time();
	graphical_printf(buf, "ACCEPT \x1b%dc%s\x1b%dc AND SET REAL TIME CLOCK", 0xffffff00,
		day_names[date.dayofweek], 0xff00ffff);
	message_box(buf, -1.0F, 5.0F, 1.0F);
}

/* 0x8010db54: ACCEPT */
int day_set(void)
{
	_setdate(&date);
	_settime(&time);
	memcpy(&rtc_day_items[7], &rtc_day_items[8], sizeof(menu_item_t));
	memcpy(&rtc_day_items[8], &rtc_day_items[9], sizeof(menu_item_t));
	day_accept = 0;
	rtc_day_menu.sel = 0;
	return 1;
}
