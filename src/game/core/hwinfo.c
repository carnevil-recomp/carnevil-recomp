/*
 * hwinfo.c - the security PIC's serial number (0x800c6480), the hardware info screen the
 * main process shows while it boots (0x800c65d4), and the test switch's way into the
 * diagnostics (0x800c6950-0x800c6a04): DIAG.EXE replaces the game through the BIOS exec.
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
extern void	*qcreate(char *, int, void (*)(), int, int, int, int);
extern void	sleep(int);
extern void	clean_up_level_sounds(void);
extern int	movie_pause(void);
extern void	install_disk_info(void *, void *, void *);
extern int	exec(char *, unsigned int, int *);
extern void	set_percent_audit(int, int, int);			/* 0x800f1cd4 */

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
		info[5] = info[5] - 1;
		info[3] = info[5] % 31 + 1;
		info[4] = info[5] / 372 + 80;
		info[2] = (info[5] % 372) / 31 + 1;
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
	switch(cmos_status)
	{
	case 3:
		draw_string_centered(1, "OK", 300.0f, 140.0f, 0xff00);
		break;
	case 1:
		draw_string_centered(1, "FACTORY SETTINGS RESTORED", 300.0f, 140.0f, 0xffff00);
		break;
	case 2:
		draw_string_centered(1, "BAD", 300.0f, 140.0f, 0xff0000);
		break;
	}
	draw_string_centered(1, "COINAGE:", 100.0f, 110.0f, 0xff00);
	switch(coinage_source)
	{
	case 1:
		draw_string_centered(1, "FROM CMOS", 300.0f, 110.0f, 0xff00);
		break;
	case 2:
		draw_string_centered(1, "FROM DIPSWITCH", 300.0f, 110.0f, 0xffff00);
		break;
	}
	draw_string_centered(1, "GUN:", 100.0f, 80.0f, 0xff00);
	switch(gun_status)
	{
	case 2:
		draw_string_centered(1, "OK", 300.0f, 80.0f, 0xff00);
		break;
	case 1:
		draw_string_centered(1, "BAD", 300.0f, 80.0f, 0xff0000);
		break;
	}
	draw_string_centered(1, "DCS2:", 100.0f, 50.0f, 0xff00);
	if(snd_opsys_ver)
	{
		graphical_printf(text_buffer, "%04X", snd_opsys_ver);
		draw_string_centered(1, text_buffer, 300.0f, 50.0f, 0xff00);
	}
}

/* 0x800c6950: the sounds stop and the movie holds, the disk loses its suspend hooks, and the BIOS
   loads DIAG.EXE over the game (it never comes back: DIAG execs GAME.EXE to leave) */
void diags_proc(int *args)
{
	clean_up_level_sounds();
	movie_pause();
	sleep(5);
	install_disk_info(0, 0, 0);
	exec("diag.exe", 0x8100000, args);
}

/* 0x800c69ac: the test switch (set_dcsw_handler(4) in main_proc) */
void dispatch_diags_proc(int closed, int swid)
{
	if(closed == 1)
	{
		set_percent_audit(0x23, 0xc, 0x26);
		qcreate("diags", 0, diags_proc, 0, 0, 0, 0);
	}
}
