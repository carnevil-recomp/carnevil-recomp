/*
 * hwinfo.c - the security PIC's serial number (0x800c6480) and the hardware info screen the
 * main process shows while it boots (0x800c65d4).
 */
#include <stdio.h>

extern int	open(const char *, int, ...);
extern int	_ioctl(int, int, int);
extern int	close(int);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_centered(int, char *, float, float, unsigned int);	/* 0x800ddf74 (add_to_print_) */
extern char	text_buffer[];						/* 0x801e7100 */
extern int	cmos_status;						/* main.c */
extern int	coinage_source;						/* 0x801e096c: 1 CMOS, 2 dipswitch */
extern int	gun_status;						/* 0x80237404: 1 bad, 2 ok */
extern int	snd_opsys_ver;						/* 0x802373e4 */

/* 0x801e7030: serial number, game id, month, day, year, date of manufacture (read as one block) */
int	pic_serial[6];
#define serial_number		pic_serial[0]
#define game_id			pic_serial[1]
#define month			pic_serial[2]
#define day			pic_serial[3]
#define year			pic_serial[4]

/* 0x800c6480: read the PIC through the BIOS "pic:" device and split the manufacture date
   (372 days per year, 31 per month, 1980 based) */
void read_pic_serial(int *info)
{
	int	fd;
	int	d;

	fd = open("pic:", 0);
	if(fd < 0)
	{
		info[0] = -1;
		info[1] = -1;
		info[5] = -1;
		info[3] = 0;
		info[2] = 0;
		info[4] = 0;
	}
	else
	{
		_ioctl(fd, 0x7000, (int)(long)&info[0]);
		_ioctl(fd, 0x7001, (int)(long)&info[1]);
		_ioctl(fd, 0x7002, (int)(long)&info[5]);
		d = info[5] - 1;
		info[5] = d;
		info[3] = d % 31 + 1;
		info[4] = d / 372 + 80;
		info[2] = (d % 372) / 31 + 1;
	}
	close(fd);
}

/* 0x800c65d4: version, serial, date and the self-test results, as a draw object */
void hw_info_screen(void)
{
	graphical_printf(text_buffer, "CARNEVIL V%d.%d.%d", 1, 0, 3);
	draw_string_centered(1, text_buffer, 200.0f, 230.0f, 0xffffff);
	if(game_id != 0x210 && game_id != 0x1d5 && game_id != 0x1e6)
	{
		draw_string_centered(1, "FATAL ERROR: U96 BAD", 200.0f, 200.0f, 0xff0000);
		return;
	}
	graphical_printf(text_buffer, "SERIAL NUMBER: %d", serial_number);
	draw_string_centered(1, text_buffer, 200.0f, 200.0f, 0xffffff);
	graphical_printf(text_buffer, "MANUFACTURE DATE %02d/%02d/%02d", month, day, year);
	draw_string_centered(1, text_buffer, 200.0f, 170.0f, 0xffffff);
	draw_string_centered(1, "CMOS:", 100.0f, 140.0f, 0xff00);
	if(cmos_status == 2)
	{
		draw_string_centered(1, "BAD", 300.0f, 140.0f, 0xff0000);
	}
	else if(cmos_status == 1)
	{
		draw_string_centered(1, "FACTORY SETTINGS RESTORED", 300.0f, 140.0f, 0xffff00);
	}
	else if(cmos_status == 3)
	{
		draw_string_centered(1, "OK", 300.0f, 140.0f, 0xff00);
	}
	draw_string_centered(1, "COINAGE:", 100.0f, 110.0f, 0xff00);
	if(coinage_source == 1)
	{
		draw_string_centered(1, "FROM CMOS", 300.0f, 110.0f, 0xff00);
	}
	else if(coinage_source == 2)
	{
		draw_string_centered(1, "FROM DIPSWITCH", 300.0f, 110.0f, 0xffff00);
	}
	draw_string_centered(1, "GUN:", 100.0f, 80.0f, 0xff00);
	if(gun_status == 1)
	{
		draw_string_centered(1, "BAD", 300.0f, 80.0f, 0xff0000);
	}
	else if(gun_status == 2)
	{
		draw_string_centered(1, "OK", 300.0f, 80.0f, 0xff00);
	}
	draw_string_centered(1, "DCS2:", 100.0f, 50.0f, 0xff00);
	if(snd_opsys_ver)
	{
		graphical_printf(text_buffer, "%04X", snd_opsys_ver);
		draw_string_centered(1, text_buffer, 300.0f, 50.0f, 0xff00);
	}
}
