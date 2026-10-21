/*
 * rtcset_minute.c - SET RTC MINUTES (0x8010e720-0x8010e9fc): the minute, in ten pages of six.
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

int	minute_select(int);
int	minute_page(void);
int	minute_set(void);
void	minute_accept_help(int);

static int	minute_accept = 0;		/* the ACCEPT items are in */
static int	minute_pg = 0;			/* the page shown */

char	help_minute_00[] = "SET MINUTE TO 00";
char	help_minute_01[] = "SET MINUTE TO 01";
char	help_minute_02[] = "SET MINUTE TO 02";
char	help_minute_03[] = "SET MINUTE TO 03";
char	help_minute_04[] = "SET MINUTE TO 04";
char	help_minute_05[] = "SET MINUTE TO 05";
char	help_minute_06[] = "SET MINUTE TO 06";
char	help_minute_07[] = "SET MINUTE TO 07";
char	help_minute_08[] = "SET MINUTE TO 08";
char	help_minute_09[] = "SET MINUTE TO 09";
char	help_minute_10[] = "SET MINUTE TO 10";
char	help_minute_11[] = "SET MINUTE TO 11";
char	help_minute_12[] = "SET MINUTE TO 12";
char	help_minute_13[] = "SET MINUTE TO 13";
char	help_minute_14[] = "SET MINUTE TO 14";
char	help_minute_15[] = "SET MINUTE TO 15";
char	help_minute_16[] = "SET MINUTE TO 16";
char	help_minute_17[] = "SET MINUTE TO 17";
char	help_minute_18[] = "SET MINUTE TO 18";
char	help_minute_19[] = "SET MINUTE TO 19";
char	help_minute_20[] = "SET MINUTE TO 20";
char	help_minute_21[] = "SET MINUTE TO 21";
char	help_minute_22[] = "SET MINUTE TO 22";
char	help_minute_23[] = "SET MINUTE TO 23";
char	help_minute_24[] = "SET MINUTE TO 24";
char	help_minute_25[] = "SET MINUTE TO 25";
char	help_minute_26[] = "SET MINUTE TO 26";
char	help_minute_27[] = "SET MINUTE TO 27";
char	help_minute_28[] = "SET MINUTE TO 28";
char	help_minute_29[] = "SET MINUTE TO 29";
char	help_minute_30[] = "SET MINUTE TO 30";
char	help_minute_31[] = "SET MINUTE TO 31";
char	help_minute_32[] = "SET MINUTE TO 32";
char	help_minute_33[] = "SET MINUTE TO 33";
char	help_minute_34[] = "SET MINUTE TO 34";
char	help_minute_35[] = "SET MINUTE TO 35";
char	help_minute_36[] = "SET MINUTE TO 36";
char	help_minute_37[] = "SET MINUTE TO 37";
char	help_minute_38[] = "SET MINUTE TO 38";
char	help_minute_39[] = "SET MINUTE TO 39";
char	help_minute_40[] = "SET MINUTE TO 40";
char	help_minute_41[] = "SET MINUTE TO 41";
char	help_minute_42[] = "SET MINUTE TO 42";
char	help_minute_43[] = "SET MINUTE TO 43";
char	help_minute_44[] = "SET MINUTE TO 44";
char	help_minute_45[] = "SET MINUTE TO 45";
char	help_minute_46[] = "SET MINUTE TO 46";
char	help_minute_47[] = "SET MINUTE TO 47";
char	help_minute_48[] = "SET MINUTE TO 48";
char	help_minute_49[] = "SET MINUTE TO 49";
char	help_minute_50[] = "SET MINUTE TO 50";
char	help_minute_51[] = "SET MINUTE TO 51";
char	help_minute_52[] = "SET MINUTE TO 52";
char	help_minute_53[] = "SET MINUTE TO 53";
char	help_minute_54[] = "SET MINUTE TO 54";
char	help_minute_55[] = "SET MINUTE TO 55";
char	help_minute_56[] = "SET MINUTE TO 56";
char	help_minute_57[] = "SET MINUTE TO 57";
char	help_minute_58[] = "SET MINUTE TO 58";
char	help_minute_59[] = "SET MINUTE TO 59";
char	help_minute_00_to_59_minutes[] = "00 TO 59 MINUTES";
char	help_minute_06_to_59_minutes[] = "06 TO 59 MINUTES";
char	help_minute_12_to_59_minutes[] = "12 TO 59 MINUTES";
char	help_minute_18_to_59_minutes[] = "18 TO 59 MINUTES";
char	help_minute_24_to_59_minutes[] = "24 TO 59 MINUTES";
char	help_minute_30_to_59_minutes[] = "30 TO 59 MINUTES";
char	help_minute_36_to_59_minutes[] = "36 TO 59 MINUTES";
char	help_minute_42_to_59_minutes[] = "42 TO 59 MINUTES";
char	help_minute_48_to_59_minutes[] = "48 TO 59 MINUTES";
char	help_minute_54_to_59_minutes[] = "54 TO 59 MINUTES";
char	help_minute_exit[] = "RETURN TO SET REAL TIME CLOCK MENU (NO SET)";

menu_item_t	rtc_minute_items[10][10] = {
	{
		{ "00 MINUTES", MENU_FUNC, minute_select, help_minute_00, 0 },
		{ "01 MINUTES", MENU_FUNC, minute_select, help_minute_01, 0 },
		{ "02 MINUTES", MENU_FUNC, minute_select, help_minute_02, 0 },
		{ "03 MINUTES", MENU_FUNC, minute_select, help_minute_03, 0 },
		{ "04 MINUTES", MENU_FUNC, minute_select, help_minute_04, 0 },
		{ "05 MINUTES", MENU_FUNC, minute_select, help_minute_05, 0 },
		{ "06 - 59 MINUTES", MENU_FUNC, minute_page, help_minute_06_to_59_minutes, 0 },
		{ "EXIT", MENU_EXIT, 0, help_minute_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "06 MINUTES", MENU_FUNC, minute_select, help_minute_06, 0 },
		{ "07 MINUTES", MENU_FUNC, minute_select, help_minute_07, 0 },
		{ "08 MINUTES", MENU_FUNC, minute_select, help_minute_08, 0 },
		{ "09 MINUTES", MENU_FUNC, minute_select, help_minute_09, 0 },
		{ "10 MINUTES", MENU_FUNC, minute_select, help_minute_10, 0 },
		{ "11 MINUTES", MENU_FUNC, minute_select, help_minute_11, 0 },
		{ "12 - 59 MINUTES", MENU_FUNC, minute_page, help_minute_12_to_59_minutes, 0 },
		{ "EXIT", MENU_EXIT, 0, help_minute_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "12 MINUTES", MENU_FUNC, minute_select, help_minute_12, 0 },
		{ "13 MINUTES", MENU_FUNC, minute_select, help_minute_13, 0 },
		{ "14 MINUTES", MENU_FUNC, minute_select, help_minute_14, 0 },
		{ "15 MINUTES", MENU_FUNC, minute_select, help_minute_15, 0 },
		{ "16 MINUTES", MENU_FUNC, minute_select, help_minute_16, 0 },
		{ "17 MINUTES", MENU_FUNC, minute_select, help_minute_17, 0 },
		{ "18 - 59 MINUTES", MENU_FUNC, minute_page, help_minute_18_to_59_minutes, 0 },
		{ "EXIT", MENU_EXIT, 0, help_minute_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "18 MINUTES", MENU_FUNC, minute_select, help_minute_18, 0 },
		{ "19 MINUTES", MENU_FUNC, minute_select, help_minute_19, 0 },
		{ "20 MINUTES", MENU_FUNC, minute_select, help_minute_20, 0 },
		{ "21 MINUTES", MENU_FUNC, minute_select, help_minute_21, 0 },
		{ "22 MINUTES", MENU_FUNC, minute_select, help_minute_22, 0 },
		{ "23 MINUTES", MENU_FUNC, minute_select, help_minute_23, 0 },
		{ "24 - 59 MINUTES", MENU_FUNC, minute_page, help_minute_24_to_59_minutes, 0 },
		{ "EXIT", MENU_EXIT, 0, help_minute_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "24 MINUTES", MENU_FUNC, minute_select, help_minute_24, 0 },
		{ "25 MINUTES", MENU_FUNC, minute_select, help_minute_25, 0 },
		{ "26 MINUTES", MENU_FUNC, minute_select, help_minute_26, 0 },
		{ "27 MINUTES", MENU_FUNC, minute_select, help_minute_27, 0 },
		{ "28 MINUTES", MENU_FUNC, minute_select, help_minute_28, 0 },
		{ "29 MINUTES", MENU_FUNC, minute_select, help_minute_29, 0 },
		{ "30 - 59 MINUTES", MENU_FUNC, minute_page, help_minute_30_to_59_minutes, 0 },
		{ "EXIT", MENU_EXIT, 0, help_minute_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "30 MINUTES", MENU_FUNC, minute_select, help_minute_30, 0 },
		{ "31 MINUTES", MENU_FUNC, minute_select, help_minute_31, 0 },
		{ "32 MINUTES", MENU_FUNC, minute_select, help_minute_32, 0 },
		{ "33 MINUTES", MENU_FUNC, minute_select, help_minute_33, 0 },
		{ "34 MINUTES", MENU_FUNC, minute_select, help_minute_34, 0 },
		{ "35 MINUTES", MENU_FUNC, minute_select, help_minute_35, 0 },
		{ "36 - 59 MINUTES", MENU_FUNC, minute_page, help_minute_36_to_59_minutes, 0 },
		{ "EXIT", MENU_EXIT, 0, help_minute_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "36 MINUTES", MENU_FUNC, minute_select, help_minute_36, 0 },
		{ "37 MINUTES", MENU_FUNC, minute_select, help_minute_37, 0 },
		{ "38 MINUTES", MENU_FUNC, minute_select, help_minute_38, 0 },
		{ "39 MINUTES", MENU_FUNC, minute_select, help_minute_39, 0 },
		{ "40 MINUTES", MENU_FUNC, minute_select, help_minute_40, 0 },
		{ "41 MINUTES", MENU_FUNC, minute_select, help_minute_41, 0 },
		{ "42 - 59 MINUTES", MENU_FUNC, minute_page, help_minute_42_to_59_minutes, 0 },
		{ "EXIT", MENU_EXIT, 0, help_minute_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "42 MINUTES", MENU_FUNC, minute_select, help_minute_42, 0 },
		{ "43 MINUTES", MENU_FUNC, minute_select, help_minute_43, 0 },
		{ "44 MINUTES", MENU_FUNC, minute_select, help_minute_44, 0 },
		{ "45 MINUTES", MENU_FUNC, minute_select, help_minute_45, 0 },
		{ "46 MINUTES", MENU_FUNC, minute_select, help_minute_46, 0 },
		{ "47 MINUTES", MENU_FUNC, minute_select, help_minute_47, 0 },
		{ "48 - 59 MINUTES", MENU_FUNC, minute_page, help_minute_48_to_59_minutes, 0 },
		{ "EXIT", MENU_EXIT, 0, help_minute_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "48 MINUTES", MENU_FUNC, minute_select, help_minute_48, 0 },
		{ "49 MINUTES", MENU_FUNC, minute_select, help_minute_49, 0 },
		{ "50 MINUTES", MENU_FUNC, minute_select, help_minute_50, 0 },
		{ "51 MINUTES", MENU_FUNC, minute_select, help_minute_51, 0 },
		{ "52 MINUTES", MENU_FUNC, minute_select, help_minute_52, 0 },
		{ "53 MINUTES", MENU_FUNC, minute_select, help_minute_53, 0 },
		{ "54 - 59 MINUTES", MENU_FUNC, minute_page, help_minute_54_to_59_minutes, 0 },
		{ "EXIT", MENU_EXIT, 0, help_minute_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "54 MINUTES", MENU_FUNC, minute_select, help_minute_54, 0 },
		{ "55 MINUTES", MENU_FUNC, minute_select, help_minute_55, 0 },
		{ "56 MINUTES", MENU_FUNC, minute_select, help_minute_56, 0 },
		{ "57 MINUTES", MENU_FUNC, minute_select, help_minute_57, 0 },
		{ "58 MINUTES", MENU_FUNC, minute_select, help_minute_58, 0 },
		{ "59 MINUTES", MENU_FUNC, minute_select, help_minute_59, 0 },
		{ "00 - 59 MINUTES", MENU_FUNC, minute_page, help_minute_00_to_59_minutes, 0 },
		{ "EXIT", MENU_EXIT, 0, help_minute_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	}
};

menu_t	rtc_minute_menu = { "SET RTC MINUTES", -1.0F, -1.0F, 0, 6, rtc_minute_items[0] };

static struct dosdate_t	date;
static struct dostime_t	time;

/* 0x8010e720: a minute picked */
int minute_select(int n)
{
	int	i;

	_getdate(&date);
	_gettime(&time);
	time.minute = minute_pg * 6 + n;
	if(!minute_accept)
	{
		for(i = 0; i < 10; i++)
		{
			memcpy(&rtc_minute_items[i][8], &rtc_minute_items[i][7], sizeof(menu_item_t));
			rtc_minute_items[i][7].name = "ACCEPT";
			rtc_minute_items[i][7].kind = MENU_FUNC;
			rtc_minute_items[i][7].target = minute_set;
			rtc_minute_items[i][7].help = 0;
			rtc_minute_items[i][7].hook = minute_accept_help;
		}
		minute_accept = 1;
	}
	rtc_minute_menu.sel = 7;
	return 0;
}

/* 0x8010e87c: the ACCEPT item's hook */
void minute_accept_help(int n)
{
	char	buf[128];

	fix_date_time();
	graphical_printf(buf, "ACCEPT \x1b%dc%d\x1b%dc AND SET REAL TIME CLOCK", 0xffffff00,
		time.minute, 0xff00ffff);
	message_box(buf, -1.0F, 5.0F, 1.0F);
}

/* 0x8010e8d8: ACCEPT */
int minute_set(void)
{
	int	i;

	_setdate(&date);
	_settime(&time);
	rtc_minute_menu.sel = 6;
	rtc_minute_menu.items = rtc_minute_items[0];
	for(i = 0; i < 10; i++)
	{
		memcpy(&rtc_minute_items[i][7], &rtc_minute_items[i][8], sizeof(menu_item_t));
		memcpy(&rtc_minute_items[i][8], &rtc_minute_items[i][9], sizeof(menu_item_t));
	}
	minute_pg = 0;
	minute_accept = 0;
	return 1;
}

/* 0x8010e9a0: the next page */
int minute_page(void)
{
	if(++minute_pg >= 10)
		minute_pg = 0;
	rtc_minute_menu.items = rtc_minute_items[minute_pg];
	return 0;
}
