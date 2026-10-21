/*
 * rtc.c - the real time clock (0x80108d90-0x80109140): CURRENT DATE AND TIME, redrawn every
 * second, and the same on the serial port.
 */
#include <stdio.h>
#include <goose/sprites.h>
#include <goose/ostrings.h>
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
extern void	sleep(int);
extern int	snd_scall_bank(char *, int, int, int, int);
extern int	(*sw_exit)(void);

char	*month_names[] = { "JANUARY", "FEBUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY",
	"AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER" };
char	*day_names[] = { "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY" };

static inline char *day_name(struct dosdate_t *d)
{
	if(d->dayofweek >= 7)
		return "INVALID";
	return day_names[d->dayofweek];
}

static inline char *month_name(struct dosdate_t *d)
{
	if(!d->month || d->month >= 13)
		return "INVALID";
	return month_names[d->month - 1];
}

/* 0x80108d90: the date and time on the serial port */
void print_date_time(void)
{
	struct dostime_t	t;
	struct dosdate_t	d;
	char	*day, *month;

	_gettime(&t);
	_getdate(&d);
	day = day_name(&d);
	month = month_name(&d);
	if(d.dayofweek >= 7 || !d.month || d.month >= 13)
		fprintf(stderr, ">>> PLEASE SET CLOCK! <<<\n");
	fprintf(stderr, "%s - %s  %02d,%04d\n", day, month, d.day, d.year);
	fprintf(stderr, "%02d:%02d:%02d\n", t.hour, t.minute, t.second);
}

/* 0x80108eb8 */
int rtc_test(void)
{
	struct dostime_t	t;
	struct dosdate_t	d;
	int	done = 0, sec;

	_gettime(&t);
	_getdate(&d);
	sec = t.second;
	do
	{
		draw_title("CURRENT DATE AND TIME");
		message_box("PRESS ANY PLAYER BUTTON OR TEST TO EXIT", -1.0F, 5.0F, 1.0F);
		set_text_font(0);
		set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		set_text_color(0xffffff00);
		oprintf("%s - %s  %02d,%04d",
			day_name(&d), month_name(&d),
			d.day, d.year);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F - (font0_height + font0_height)), 1.0F);
		oprintf("%02d:%02d:%02d", t.hour, t.minute, t.second);
		do
		{
			if(sw_exit() || sw_select() || sw_back())
			{
				done = 1;
				break;
			}
			sleep(1);
			_gettime(&t);
		} while(sec == t.second);
		if(!done)
			snd_scall_bank("ttest", 13, 180, 127, 0x8000);
		_getdate(&d);
		sec = t.second;
		delete_multiple_strings(0, 0);
		del1c(0, 0);
	} while(!done);
	snd_scall_bank("ttest", 11, 180, 127, 0x8000);
	return 0;
}
