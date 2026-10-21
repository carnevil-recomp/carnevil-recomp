/*
 * sysinfo.c - SYSTEM INFORMATION (0x80104548-): the platform, and from the security PIC the
 * serial number, the game it was made for and its date of manufacture.
 */
#include <stdio.h>
#include <string.h>
#include <goose/goose.h>
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

extern int	open(const char *, int, ...);
extern int	close(int);
extern int	_ioctl(int, int, void *);

#define PIC_SERIAL	0x7000		/* _ioctl on "pic:" */
#define PIC_GAME	0x7001
#define PIC_DATE	0x7002		/* days: 372 to a year from 1980, 31 to a month */

/* the games this DIAG knows, by the PIC's two game numbers */
struct {
	int	id, id2;
	char	*name;
} pic_games[] = {
	{ 469, 469, "CARNEVIL" }
};

/* 0x80104548 */
int system_info(void)
{
	char	platform[64];
	char	*p, *q, *name;
	int	fd, serial, game, date, color, i;
	int	year, month, day;
	float	y;

	fd = open("pic:", 0);
	if(fd < 0)
	{
		fprintf(stderr, "Can not open PIC device\r\n");
		return 0;
	}
	draw_title("SYSTEM INFORMATION");
	set_text_font(0);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	y = vres - font0_height * 6.0F;
	set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
	oprintf("MIDWAY GAMES INC.");
	y -= font0_height + font0_height;
	set_text_font(1);
	set_text_color(0xff00ffff);
	set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
	strcpy(platform, get_platform_id());
	p = platform;
	while(*p != ' ')
		p++;
	while(*p == ' ')
		p++;
	q = p;
	while(*p != '1')
		p++;
	*p = 0;
	oprintf("%s SYSTEM", q);

	y = vres / 2.0F;
	set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
	color = 0xffff0000;
	set_text_color(0xff00ff00);
	_ioctl(fd, PIC_SERIAL, &serial);
	_ioctl(fd, PIC_GAME, &game);
	oprintf("Serial Number:  \x1b%dc%d", -1, serial);

	y -= font1_height + font1_height;
	name = "DEVELOPMENT PIC";
	for(i = 0; i < sizeof(pic_games) / sizeof(pic_games[0]); i++)
	{
		if(game == pic_games[i].id || game == pic_games[i].id2)
		{
			name = pic_games[i].name;
			color = -1;
			break;
		}
	}
	set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
	set_text_color(0xff00ff00);
	oprintf("Game:  \x1b%dc%s", color, name);

	_ioctl(fd, PIC_DATE, &date);
	day = date % 31 + 1;
	month = date % 372 / 31 + 1;
	year = date / 372 + 80;
	y -= font1_height + font1_height;
	set_text_color(0xff00ff00);
	set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
	oprintf("Date of manufacture:  \x1b%dc%02d/%02d/%02d", -1, month, day, year);

	wait_test_exit();
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	close(fd);
	return 0;
}
