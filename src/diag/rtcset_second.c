/*
 * rtcset_second.c - SET RTC SECONDS (0x8010ea00-0x8010ecdc): the second, in ten pages of six.
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

int	second_select(int);
int	second_page(void);
int	second_set(void);
void	second_accept_help(int);

static int	second_accept = 0;		/* the ACCEPT items are in */
static int	second_pg = 0;			/* the page shown */

char	help_second_00[] = "SET SECOND TO 00";
char	help_second_01[] = "SET SECOND TO 01";
char	help_second_02[] = "SET SECOND TO 02";
char	help_second_03[] = "SET SECOND TO 03";
char	help_second_04[] = "SET SECOND TO 04";
char	help_second_05[] = "SET SECOND TO 05";
char	help_second_06[] = "SET SECOND TO 06";
char	help_second_07[] = "SET SECOND TO 07";
char	help_second_08[] = "SET SECOND TO 08";
char	help_second_09[] = "SET SECOND TO 09";
char	help_second_10[] = "SET SECOND TO 10";
char	help_second_11[] = "SET SECOND TO 11";
char	help_second_12[] = "SET SECOND TO 12";
char	help_second_13[] = "SET SECOND TO 13";
char	help_second_14[] = "SET SECOND TO 14";
char	help_second_15[] = "SET SECOND TO 15";
char	help_second_16[] = "SET SECOND TO 16";
char	help_second_17[] = "SET SECOND TO 17";
char	help_second_18[] = "SET SECOND TO 18";
char	help_second_19[] = "SET SECOND TO 19";
char	help_second_20[] = "SET SECOND TO 20";
char	help_second_21[] = "SET SECOND TO 21";
char	help_second_22[] = "SET SECOND TO 22";
char	help_second_23[] = "SET SECOND TO 23";
char	help_second_24[] = "SET SECOND TO 24";
char	help_second_25[] = "SET SECOND TO 25";
char	help_second_26[] = "SET SECOND TO 26";
char	help_second_27[] = "SET SECOND TO 27";
char	help_second_28[] = "SET SECOND TO 28";
char	help_second_29[] = "SET SECOND TO 29";
char	help_second_30[] = "SET SECOND TO 30";
char	help_second_31[] = "SET SECOND TO 31";
char	help_second_32[] = "SET SECOND TO 32";
char	help_second_33[] = "SET SECOND TO 33";
char	help_second_34[] = "SET SECOND TO 34";
char	help_second_35[] = "SET SECOND TO 35";
char	help_second_36[] = "SET SECOND TO 36";
char	help_second_37[] = "SET SECOND TO 37";
char	help_second_38[] = "SET SECOND TO 38";
char	help_second_39[] = "SET SECOND TO 39";
char	help_second_40[] = "SET SECOND TO 40";
char	help_second_41[] = "SET SECOND TO 41";
char	help_second_42[] = "SET SECOND TO 42";
char	help_second_43[] = "SET SECOND TO 43";
char	help_second_44[] = "SET SECOND TO 44";
char	help_second_45[] = "SET SECOND TO 45";
char	help_second_46[] = "SET SECOND TO 46";
char	help_second_47[] = "SET SECOND TO 47";
char	help_second_48[] = "SET SECOND TO 48";
char	help_second_49[] = "SET SECOND TO 49";
char	help_second_50[] = "SET SECOND TO 50";
char	help_second_51[] = "SET SECOND TO 51";
char	help_second_52[] = "SET SECOND TO 52";
char	help_second_53[] = "SET SECOND TO 53";
char	help_second_54[] = "SET SECOND TO 54";
char	help_second_55[] = "SET SECOND TO 55";
char	help_second_56[] = "SET SECOND TO 56";
char	help_second_57[] = "SET SECOND TO 57";
char	help_second_58[] = "SET SECOND TO 58";
char	help_second_59[] = "SET SECOND TO 59";
char	help_second_00_to_59_seconds[] = "00 TO 59 SECONDS";
char	help_second_06_to_59_seconds[] = "06 TO 59 SECONDS";
char	help_second_12_to_59_seconds[] = "12 TO 59 SECONDS";
char	help_second_18_to_59_seconds[] = "18 TO 59 SECONDS";
char	help_second_24_to_59_seconds[] = "24 TO 59 SECONDS";
char	help_second_30_to_59_seconds[] = "30 TO 59 SECONDS";
char	help_second_36_to_59_seconds[] = "36 TO 59 SECONDS";
char	help_second_42_to_59_seconds[] = "42 TO 59 SECONDS";
char	help_second_48_to_59_seconds[] = "48 TO 59 SECONDS";
char	help_second_54_to_59_seconds[] = "54 TO 59 SECONDS";
char	help_second_exit[] = "RETURN TO SET REAL TIME CLOCK MENU (NO SET)";

menu_item_t	rtc_second_items[10][10] = {
	{
		{ "00 SECONDS", MENU_FUNC, second_select, help_second_00, 0 },
		{ "01 SECONDS", MENU_FUNC, second_select, help_second_01, 0 },
		{ "02 SECONDS", MENU_FUNC, second_select, help_second_02, 0 },
		{ "03 SECONDS", MENU_FUNC, second_select, help_second_03, 0 },
		{ "04 SECONDS", MENU_FUNC, second_select, help_second_04, 0 },
		{ "05 SECONDS", MENU_FUNC, second_select, help_second_05, 0 },
		{ "06 - 59 SECONDS", MENU_FUNC, second_page, help_second_06_to_59_seconds, 0 },
		{ "EXIT", MENU_EXIT, 0, help_second_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "06 SECONDS", MENU_FUNC, second_select, help_second_06, 0 },
		{ "07 SECONDS", MENU_FUNC, second_select, help_second_07, 0 },
		{ "08 SECONDS", MENU_FUNC, second_select, help_second_08, 0 },
		{ "09 SECONDS", MENU_FUNC, second_select, help_second_09, 0 },
		{ "10 SECONDS", MENU_FUNC, second_select, help_second_10, 0 },
		{ "11 SECONDS", MENU_FUNC, second_select, help_second_11, 0 },
		{ "12 - 59 SECONDS", MENU_FUNC, second_page, help_second_12_to_59_seconds, 0 },
		{ "EXIT", MENU_EXIT, 0, help_second_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "12 SECONDS", MENU_FUNC, second_select, help_second_12, 0 },
		{ "13 SECONDS", MENU_FUNC, second_select, help_second_13, 0 },
		{ "14 SECONDS", MENU_FUNC, second_select, help_second_14, 0 },
		{ "15 SECONDS", MENU_FUNC, second_select, help_second_15, 0 },
		{ "16 SECONDS", MENU_FUNC, second_select, help_second_16, 0 },
		{ "17 SECONDS", MENU_FUNC, second_select, help_second_17, 0 },
		{ "18 - 59 SECONDS", MENU_FUNC, second_page, help_second_18_to_59_seconds, 0 },
		{ "EXIT", MENU_EXIT, 0, help_second_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "18 SECONDS", MENU_FUNC, second_select, help_second_18, 0 },
		{ "19 SECONDS", MENU_FUNC, second_select, help_second_19, 0 },
		{ "20 SECONDS", MENU_FUNC, second_select, help_second_20, 0 },
		{ "21 SECONDS", MENU_FUNC, second_select, help_second_21, 0 },
		{ "22 SECONDS", MENU_FUNC, second_select, help_second_22, 0 },
		{ "23 SECONDS", MENU_FUNC, second_select, help_second_23, 0 },
		{ "24 - 59 SECONDS", MENU_FUNC, second_page, help_second_24_to_59_seconds, 0 },
		{ "EXIT", MENU_EXIT, 0, help_second_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "24 SECONDS", MENU_FUNC, second_select, help_second_24, 0 },
		{ "25 SECONDS", MENU_FUNC, second_select, help_second_25, 0 },
		{ "26 SECONDS", MENU_FUNC, second_select, help_second_26, 0 },
		{ "27 SECONDS", MENU_FUNC, second_select, help_second_27, 0 },
		{ "28 SECONDS", MENU_FUNC, second_select, help_second_28, 0 },
		{ "29 SECONDS", MENU_FUNC, second_select, help_second_29, 0 },
		{ "30 - 59 SECONDS", MENU_FUNC, second_page, help_second_30_to_59_seconds, 0 },
		{ "EXIT", MENU_EXIT, 0, help_second_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "30 SECONDS", MENU_FUNC, second_select, help_second_30, 0 },
		{ "31 SECONDS", MENU_FUNC, second_select, help_second_31, 0 },
		{ "32 SECONDS", MENU_FUNC, second_select, help_second_32, 0 },
		{ "33 SECONDS", MENU_FUNC, second_select, help_second_33, 0 },
		{ "34 SECONDS", MENU_FUNC, second_select, help_second_34, 0 },
		{ "35 SECONDS", MENU_FUNC, second_select, help_second_35, 0 },
		{ "36 - 59 SECONDS", MENU_FUNC, second_page, help_second_36_to_59_seconds, 0 },
		{ "EXIT", MENU_EXIT, 0, help_second_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "36 SECONDS", MENU_FUNC, second_select, help_second_36, 0 },
		{ "37 SECONDS", MENU_FUNC, second_select, help_second_37, 0 },
		{ "38 SECONDS", MENU_FUNC, second_select, help_second_38, 0 },
		{ "39 SECONDS", MENU_FUNC, second_select, help_second_39, 0 },
		{ "40 SECONDS", MENU_FUNC, second_select, help_second_40, 0 },
		{ "41 SECONDS", MENU_FUNC, second_select, help_second_41, 0 },
		{ "42 - 59 SECONDS", MENU_FUNC, second_page, help_second_42_to_59_seconds, 0 },
		{ "EXIT", MENU_EXIT, 0, help_second_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "42 SECONDS", MENU_FUNC, second_select, help_second_42, 0 },
		{ "43 SECONDS", MENU_FUNC, second_select, help_second_43, 0 },
		{ "44 SECONDS", MENU_FUNC, second_select, help_second_44, 0 },
		{ "45 SECONDS", MENU_FUNC, second_select, help_second_45, 0 },
		{ "46 SECONDS", MENU_FUNC, second_select, help_second_46, 0 },
		{ "47 SECONDS", MENU_FUNC, second_select, help_second_47, 0 },
		{ "48 - 59 SECONDS", MENU_FUNC, second_page, help_second_48_to_59_seconds, 0 },
		{ "EXIT", MENU_EXIT, 0, help_second_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "48 SECONDS", MENU_FUNC, second_select, help_second_48, 0 },
		{ "49 SECONDS", MENU_FUNC, second_select, help_second_49, 0 },
		{ "50 SECONDS", MENU_FUNC, second_select, help_second_50, 0 },
		{ "51 SECONDS", MENU_FUNC, second_select, help_second_51, 0 },
		{ "52 SECONDS", MENU_FUNC, second_select, help_second_52, 0 },
		{ "53 SECONDS", MENU_FUNC, second_select, help_second_53, 0 },
		{ "54 - 59 SECONDS", MENU_FUNC, second_page, help_second_54_to_59_seconds, 0 },
		{ "EXIT", MENU_EXIT, 0, help_second_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	},
	{
		{ "54 SECONDS", MENU_FUNC, second_select, help_second_54, 0 },
		{ "55 SECONDS", MENU_FUNC, second_select, help_second_55, 0 },
		{ "56 SECONDS", MENU_FUNC, second_select, help_second_56, 0 },
		{ "57 SECONDS", MENU_FUNC, second_select, help_second_57, 0 },
		{ "58 SECONDS", MENU_FUNC, second_select, help_second_58, 0 },
		{ "59 SECONDS", MENU_FUNC, second_select, help_second_59, 0 },
		{ "00 - 59 SECONDS", MENU_FUNC, second_page, help_second_00_to_59_seconds, 0 },
		{ "EXIT", MENU_EXIT, 0, help_second_exit, 0 },
		{ 0, -1, 0, 0, 0 },
		{ 0, -1, 0, 0, 0 }
	}
};

menu_t	rtc_second_menu = { "SET RTC SECONDS", -1.0F, -1.0F, 0, 6, rtc_second_items[0] };

static struct dosdate_t	date;
static struct dostime_t	time;

/* 0x8010ea00: a second picked */
int second_select(int n)
{
	int	i;

	_getdate(&date);
	_gettime(&time);
	time.second = second_pg * 6 + n;
	if(!second_accept)
	{
		for(i = 0; i < 10; i++)
		{
			memcpy(&rtc_second_items[i][8], &rtc_second_items[i][7], sizeof(menu_item_t));
			rtc_second_items[i][7].name = "ACCEPT";
			rtc_second_items[i][7].kind = MENU_FUNC;
			rtc_second_items[i][7].target = second_set;
			rtc_second_items[i][7].help = 0;
			rtc_second_items[i][7].hook = second_accept_help;
		}
		second_accept = 1;
	}
	rtc_second_menu.sel = 7;
	return 0;
}

/* 0x8010eb5c: the ACCEPT item's hook */
void second_accept_help(int n)
{
	char	buf[128];

	fix_date_time();
	graphical_printf(buf, "ACCEPT \x1b%dc%d\x1b%dc AND SET REAL TIME CLOCK", 0xffffff00,
		time.second, 0xff00ffff);
	message_box(buf, -1.0F, 5.0F, 1.0F);
}

/* 0x8010ebb8: ACCEPT */
int second_set(void)
{
	int	i;

	_setdate(&date);
	_settime(&time);
	rtc_second_menu.sel = 6;
	rtc_second_menu.items = rtc_second_items[0];
	for(i = 0; i < 10; i++)
	{
		memcpy(&rtc_second_items[i][7], &rtc_second_items[i][8], sizeof(menu_item_t));
		memcpy(&rtc_second_items[i][8], &rtc_second_items[i][9], sizeof(menu_item_t));
	}
	second_pg = 0;
	second_accept = 0;
	return 1;
}

/* 0x8010ec80: the next page */
int second_page(void)
{
	if(++second_pg >= 10)
		second_pg = 0;
	rtc_second_menu.items = rtc_second_items[second_pg];
	return 0;
}
