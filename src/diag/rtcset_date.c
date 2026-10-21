/*
 * rtcset_date.c - SET RTC DATE (0x8010de50-0x8010e424): the day of the month, picked from a
 * calendar grid of 31 numbers.
 */
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
extern void	_setdate(struct dosdate_t *);
extern int	graphical_printf(char *, char *, ...);
extern int	dialog(char *, float, float, float, int);
extern void	sleep(int);
extern int	snd_scall_bank(char *, int, int, int, int);
extern void	draw_hline(float, float, float, float, int, int);
extern void	draw_vline(float, float, float, float, int, int);

static struct dostime_t	time;
static struct dosdate_t	date;
static float	cell[36][2];		/* the centres of the grid's cells */

/* 0x8010de50 */
int set_rtc_date(void)
{
	char	buf[128];
	float	ch, cw, w, h, x0, y0, x, y;
	int	i, delay = 30;
	int	done = 0;		/* never set: the buttons leave by break and goto */

	_getdate(&date);
	_gettime(&time);
	set_text_font(0);
	ch = font0_height + font0_height;
	cw = get_string_width("32") * 1.5F;
	w = cw * 6.0F;
	x0 = (hres - w) / 2.0F;
	h = ch * 6.0F;
	y0 = (vres - h) / 2.0F;
	for(x = 0.0F; x <= w; x += cw)
		draw_vline(x0 + x, y0, y0 + h, 1.0F, -1, 0);
	for(y = 0.0F; y <= h; y += ch)
		draw_hline(x0, x0 + w, y + y0, 1.0F, -1, 0);
	i = 0;
	for(y = y0 + h; y > y0; y -= ch)
		for(x = x0; x < x0 + w; x += cw)
		{
			cell[i][0] = (x + (x + cw)) / 2.0F;
			cell[i][1] = (y + (y - ch)) / 2.0F;
			i++;
		}
	draw_title("SET RTC DATE");
	message_box("VOL UP/DOWN TO HILIGHT - BUTTON TO FINISH", -1.0F, 5.0F, 1.0F);
redraw:
	set_text_font(0);
	delete_multiple_strings(0x5200, -1);
	del1c(0x5200, -1);
	for(i = 0; i < 31; i++)
	{
		set_text_position((int)cell[i][0], (int)cell[i][1], 1.0F);
		set_string_id(0x5200);
		if(i + 1 != date.day)
			set_text_color(0xffffffff);
		else
		{
			draw_rect(cell[i][0] - cw * 0.5F, cell[i][1] - ch * 0.5F,
				cell[i][0] + cw * 0.5F, cell[i][1] + ch * 0.5F, 2.0F, 0xff000080, 0x5200);
			set_text_color(0xffffff00);
		}
		set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
		oprintf("%d", i + 1);
	}
	sleep(1);
	while(delay && (sw_up() || sw_down() || sw_left() || sw_right()))
	{
		sleep(1);
		delay--;
	}
	if(sw_8() || sw_select() || sw_back())
		delay = 5;
	else
		delay = 30;
	do
	{
		if(sw_select() || sw_back())
		{
			snd_scall_bank("ttest", 10, 180, 127, 0x8000);
			break;
		}
		if(sw_down())
		{
			if(++date.day >= 32)
				date.day = 31;
			else
				snd_scall_bank("ttest", 12, 180, 127, 0x8000);
			goto redraw;
		}
		if(sw_up())
		{
			if(!--date.day)
				date.day = 1;
			else
				snd_scall_bank("ttest", 12, 180, 127, 0x8000);
			goto redraw;
		}
		sleep(1);
	} while(!done);
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	graphical_printf(buf, "Set the date to %d", date.day);
	if(dialog(buf, -1.0F, -1.0F, 1.0F, 3) == 1)
		_setdate(&date);
	return 0;
}
