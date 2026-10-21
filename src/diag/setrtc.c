/*
 * setrtc.c - the SET REAL TIME CLOCK menu (0x8010d668-0x8010d884): a submenu per field, and
 * the hook that shows the highlighted field's current value.
 */
#include <string.h>
#include "diag.h"

struct dostime_t {
	unsigned char	hour, minute, second, hsecond;
};
struct dosdate_t {
	unsigned char	day, month;
	unsigned short	year;
	unsigned char	dayofweek;
};

extern void	_gettime(struct dostime_t *);
extern void	_getdate(struct dosdate_t *);
extern int	graphical_printf(char *, char *, ...);
extern char	*day_names[], *month_names[];
extern menu_t	rtc_day_menu, rtc_month_menu, rtc_year_menu, rtc_hour_menu, rtc_minute_menu, rtc_second_menu;
extern int	set_rtc_date(void);

void	rtc_field_help(int);

char	help_set_rtc_exit[] = "RETURN TO REAL TIME CLOCK MENU";

menu_item_t	set_rtc_items[] = {
	{ "DAY OF WEEK", MENU_SUB, &rtc_day_menu, 0, rtc_field_help },
	{ "MONTH", MENU_SUB, &rtc_month_menu, 0, rtc_field_help },
	{ "DATE", MENU_FUNC, set_rtc_date, 0, rtc_field_help },
	{ "YEAR", MENU_SUB, &rtc_year_menu, 0, rtc_field_help },
	{ "HOUR", MENU_SUB, &rtc_hour_menu, 0, rtc_field_help },
	{ "MINUTE", MENU_SUB, &rtc_minute_menu, 0, rtc_field_help },
	{ "SECOND", MENU_SUB, &rtc_second_menu, 0, rtc_field_help },
	{ "EXIT", MENU_EXIT, 0, help_set_rtc_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	set_rtc_menu = { "SET REAL TIME CLOCK", -1.0F, -1.0F, 0, 0, set_rtc_items };

static struct dosdate_t	date;
static struct dostime_t	time;

/* 0x8010d668: the highlighted field and its current value */
void rtc_field_help(int item)
{
	char	buf[128], value[128];

	_gettime(&time);
	_getdate(&date);
	if(date.dayofweek >= 7 || !date.month || date.month >= 13 || time.hour >= 24 ||
		time.minute >= 60 || time.second >= 60 || date.year < 1980 || date.year >= 2100)
		graphical_printf(buf, "%s \x1b%dc(CURRENT VALUE IS:  \x1b%dcINVALID\x1b%dc)", set_rtc_items[item].name, 0xffffff00, 0xffff0000, 0xffffff00);
	else
	{
		graphical_printf(buf, "%s \x1b%dc(CURRENT VALUE IS:  ", set_rtc_items[item].name, 0xffffff00);
		switch(item)
		{
			case 0:
				graphical_printf(value, "%s)", day_names[date.dayofweek]);
				break;
			case 1:
				graphical_printf(value, "%s)", month_names[date.month - 1]);
				break;
			case 2:
				graphical_printf(value, "%d)", date.day);
				break;
			case 3:
				graphical_printf(value, "%d)", date.year);
				break;
			case 4:
				graphical_printf(value, "%d)", time.hour);
				break;
			case 5:
				graphical_printf(value, "%d)", time.minute);
				break;
			case 6:
				graphical_printf(value, "%d)", time.second);
				break;
		}
		strcat(buf, value);
	}
	message_box(buf, -1.0F, 5.0F, 1.0F);
}
