/*
 * ide.c - the disk menu's tests (0x80104990-0x80107aaf): the sequential and random read
 * rates, with and without the BIOS disk cache, the drive's identify information and a
 * surface scan.
 */
#include <stdlib.h>
#include <string.h>
#include <ioctl.h>
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

extern int	_ioctl(int, int, void *);
extern short	_SecReads(unsigned int, void *, unsigned int);
extern short	SecReads(unsigned int, void *, unsigned int);
extern void	*get_ide_ident_info(void);
extern void	sleep(int);

#define TIMER_FD	5		/* the fd the timer ioctls go to */

/* what the drive's IDENTIFY command returns (the BIOS's ide.h); get_ide_ident_info() points
   at the BIOS's copy */
typedef struct hd_driveid
{
	unsigned short	config;
	unsigned short	cyls;
	unsigned short	reserved2;
	unsigned short	heads;
	unsigned short	track_bytes;
	unsigned short	sector_bytes;
	unsigned short	sectors;
	unsigned short	vendor0;
	unsigned short	vendor1;
	unsigned short	vendor2;
	unsigned char	serial_no[20];
	unsigned short	buf_type;
	unsigned short	buf_size;
	unsigned short	vs_bytes;
	unsigned char	fw_rev[8];
	unsigned char	model[40];
	unsigned char	max_multsect;
	unsigned char	vendor3;
	unsigned short	reserved48;
	unsigned char	vendor4;
	unsigned char	capability;
	unsigned short	reserved50;
	unsigned char	vendor5;
	unsigned char	tPIO;
	unsigned char	vendor6;
	unsigned char	tDMA;
	unsigned short	field_valid;		/* bits 0: cur_ok, 1: eide_ok */
	unsigned short	cur_cyls;
	unsigned short	cur_heads;
	unsigned short	cur_sectors;
	unsigned short	cur_capacity0;
	unsigned short	cur_capacity1;
	unsigned char	multsect;
	unsigned char	multsect_valid;
	unsigned int	lba_capacity;
	unsigned short	dma_1word;
	unsigned short	dma_mword;
	unsigned short	eide_pio_modes;
	unsigned short	eide_dma_min;
	unsigned short	eide_dma_time;
	unsigned short	eide_pio;
	unsigned short	eide_pio_iordy;
} DriveID;

/* a line of the results grid */
static const image_info_t	grid_ii = { 0, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F };

/* 0x80104990: reads 100 runs of 1, 2, 4 .. 2048 sectors from the start of the disk and shows
   the lowest, average and highest rate of each */
int ide_sequential_read_test(void)
{
	image_info_t	ii = grid_ii;
	float	ypos[13];
	float	xpos[4];
	float	result[3];
	int	ticks;
	float	pos = 0.0F, rate, low, high, sum, count;
	int	i, n, secs, sec, row;
	void	*buf;

	result[0] = 0.0F;
	result[2] = 0.0F;
	result[1] = 0.0F;
	draw_title("IDE SEQUENTIAL READ TEST");

	/* the grid: 13 lines across, then 5 down (the 5th centre lands past xpos[], in result[0]) */
	ii.w = hres - 20.0F;
	ii.h = 1.0F;
	for(i = 0; pos <= font1_height * 24.0F; pos += font1_height * 2.0F, i++)
	{
		ypos[i] = (pos + (pos + font1_height * 2.0F)) / 2.0F;
		ypos[i] += vres / 384.0F * 30.0F;
		beginobj(&ii, 10.0F, pos + vres / 384.0F * 30.0F, 1.0F, 0)->id = 0;
	}
	ii.h = pos;
	ii.w = 1.0F;
	for(pos = 10.0F, i = 0; pos <= hres - 10.0F; pos += (hres - 20.0F) / 4.0F, i++)
	{
		xpos[i] = (pos + (pos + (hres - 20.0F) / 4.0F)) / 2.0F;
		beginobj(&ii, pos, vres / 384.0F * 30.0F, 1.0F, 0)->id = 0;
	}

	set_text_font(1);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	for(n = 2048, i = 0; n; n >>= 1, i++)
	{
		set_text_position((int)xpos[0], (int)ypos[i], 1.0F);
		oprintf("%d", n);
	}
	set_text_position((int)xpos[0], (int)ypos[12], 1.0F);
	set_text_color(0xff00ffff);
	oprintf("SEC/TRANS");
	set_text_position((int)xpos[1], (int)ypos[12], 1.0F);
	oprintf("LOW (MB/s)");
	set_text_position((int)xpos[2], (int)ypos[12], 1.0F);
	oprintf("AVG (MB/s)");
	set_text_position((int)xpos[3], (int)ypos[12], 1.0F);
	oprintf("HIGH (MB/s)");

	set_text_font(2);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_color(0xffffffff);
	set_text_position((int)(hres / 2.0F), (int)(font2_height * 2.0F), 1.0F);
	oprintf("CREDIT OR TEST TO ABORT AND EXIT");
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_font(1);

	for(secs = 1, row = 11; secs <= 2048; secs <<= 1, row--)
	{
		set_string_id(0x12345678);
		set_text_color(0xffff0000);
		set_text_position((int)xpos[2], (int)ypos[row], 1.0F);
		oprintf("TEST TEST TEST TEST TEST TEST TEST TEST");
		sleep(1);

		high = 0.0F;
		low = 10000.0F;
		count = 1.0F;			/* (one more than the runs: the average comes out low) */
		buf = malloc(0x100000);
		if(!buf)
		{
			result[0] = 0.0F;
			result[2] = 0.0F;
			result[1] = 0.0F;
		}
		else
		{
			sum = 0.0F;
			for(sec = 0; sec < secs * 100; sec += secs)
			{
				_ioctl(TIMER_FD, FIOCSTARTTIMER1, 0);
				_SecReads(sec, buf, secs);
				_ioctl(TIMER_FD, FIOCGETTIMER1, &ticks);
				_ioctl(TIMER_FD, FIOCSTOPTIMER1, 0);
				rate = ticks;			/* ns -> s, then MB/s */
				rate /= 1000000000.0F;
				rate = (secs * 512) / rate;
				rate /= 1000000.0F;
				sum += rate;
				if(high > 45.0F)		/* no drive is that fast: a bad reading */
					high = 0.0F;
				if(rate > high)
					high = rate;
				if(rate < low)
					low = rate;
				count += 1.0F;
				if(sw_select() || sw_back())
					break;
			}
			result[0] = low;
			result[2] = high;
			result[1] = sum / count;
			free(buf);
		}
		if(sw_select() || sw_back())
			break;

		delete_multiple_strings(0x12345678, -1);
		set_string_id(0);
		set_text_color(0xffffffff);
		set_text_position((int)xpos[1], (int)ypos[row], 1.0F);
		oprintf("%5.2f", result[0]);
		set_text_position((int)xpos[2], (int)ypos[row], 1.0F);
		oprintf("%5.2f", result[1]);
		set_text_position((int)xpos[3], (int)ypos[row], 1.0F);
		oprintf("%5.2f", result[2]);
		sleep(1);
	}
	if(!(sw_select() || sw_back()))
		wait_prompt(0);
	else
		while(sw_select() || sw_back())
			sleep(1);
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	return 0;
}

/* 0x80105114: the same through the BIOS disk cache (SecReads), and without the sanity check on
   the highest rate */
int ide_cache_read_test(void)
{
	image_info_t	ii = grid_ii;
	float	ypos[13];
	float	xpos[4];
	float	result[3];
	int	ticks;
	float	pos = 0.0F, rate, low, high, sum, count;
	int	i, n, secs, sec, row;
	void	*buf;

	result[0] = 0.0F;
	result[2] = 0.0F;
	result[1] = 0.0F;
	draw_title("IDE DISK CACHE READ TEST");

	ii.w = hres - 20.0F;
	ii.h = 1.0F;
	for(i = 0; pos <= font1_height * 24.0F; pos += font1_height * 2.0F, i++)
	{
		ypos[i] = (pos + (pos + font1_height * 2.0F)) / 2.0F;
		ypos[i] += vres / 384.0F * 30.0F;
		beginobj(&ii, 10.0F, pos + vres / 384.0F * 30.0F, 1.0F, 0)->id = 0;
	}
	ii.h = pos;
	ii.w = 1.0F;
	for(pos = 10.0F, i = 0; pos <= hres - 10.0F; pos += (hres - 20.0F) / 4.0F, i++)
	{
		xpos[i] = (pos + (pos + (hres - 20.0F) / 4.0F)) / 2.0F;
		beginobj(&ii, pos, vres / 384.0F * 30.0F, 1.0F, 0)->id = 0;
	}

	set_text_font(1);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	for(n = 2048, i = 0; n; n >>= 1, i++)
	{
		set_text_position((int)xpos[0], (int)ypos[i], 1.0F);
		oprintf("%d", n);
	}
	set_text_position((int)xpos[0], (int)ypos[12], 1.0F);
	set_text_color(0xff00ffff);
	oprintf("SEC/TRANS");
	set_text_position((int)xpos[1], (int)ypos[12], 1.0F);
	oprintf("LOW (MB/s)");
	set_text_position((int)xpos[2], (int)ypos[12], 1.0F);
	oprintf("AVG (MB/s)");
	set_text_position((int)xpos[3], (int)ypos[12], 1.0F);
	oprintf("HIGH (MB/s)");

	set_text_font(2);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_color(0xffffffff);
	set_text_position((int)(hres / 2.0F), (int)(font2_height * 2.0F), 1.0F);
	oprintf("CREDIT OR TEST TO ABORT AND EXIT");
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_font(1);

	for(secs = 1, row = 11; secs <= 2048; secs <<= 1, row--)
	{
		set_string_id(0x12345678);
		set_text_color(0xffff0000);
		set_text_position((int)xpos[2], (int)ypos[row], 1.0F);
		oprintf("TEST TEST TEST TEST TEST TEST TEST TEST");
		sleep(1);

		high = 0.0F;
		low = 10000.0F;
		count = 1.0F;
		buf = malloc(0x100000);
		if(!buf)
		{
			result[0] = 0.0F;
			result[2] = 0.0F;
			result[1] = 0.0F;
		}
		else
		{
			sum = 0.0F;
			for(sec = 0; sec < secs * 100; sec += secs)
			{
				_ioctl(TIMER_FD, FIOCSTARTTIMER1, 0);
				SecReads(sec, buf, secs);
				_ioctl(TIMER_FD, FIOCGETTIMER1, &ticks);
				_ioctl(TIMER_FD, FIOCSTOPTIMER1, 0);
				rate = ticks;
				rate /= 1000000000.0F;
				rate = (secs * 512) / rate;
				rate /= 1000000.0F;
				sum += rate;
				if(rate > high)
					high = rate;
				if(rate < low)
					low = rate;
				count += 1.0F;
				if(sw_select() || sw_back())
					break;
			}
			result[0] = low;
			result[2] = high;
			result[1] = sum / count;
			free(buf);
		}
		if(sw_select() || sw_back())
			break;

		delete_multiple_strings(0x12345678, -1);
		set_string_id(0);
		set_text_color(0xffffffff);
		set_text_position((int)xpos[1], (int)ypos[row], 1.0F);
		oprintf("%5.2f", result[0]);
		set_text_position((int)xpos[2], (int)ypos[row], 1.0F);
		oprintf("%5.2f", result[1]);
		set_text_position((int)xpos[3], (int)ypos[row], 1.0F);
		oprintf("%5.2f", result[2]);
		sleep(1);
	}
	if(!(sw_select() || sw_back()))
		wait_prompt(0);
	else
		while(sw_select() || sw_back())
			sleep(1);
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	return 0;
}

/* 0x80105884: as the sequential test, but each run reads from a random sector of the disk */
int ide_random_read_test(void)
{
	image_info_t	ii = grid_ii;
	float	ypos[13];
	float	xpos[4];
	float	result[3];
	int	ticks;
	float	pos = 0.0F, rate, low, high, sum, count;
	int	i, n, secs, sec, row, sectors, range;
	void	*buf;

	result[0] = 0.0F;
	result[2] = 0.0F;
	result[1] = 0.0F;
	draw_title("IDE RANDOM READ TEST");

	ii.w = hres - 20.0F;
	ii.h = 1.0F;
	for(i = 0; pos <= font1_height * 24.0F; pos += font1_height * 2.0F, i++)
	{
		ypos[i] = (pos + (pos + font1_height * 2.0F)) / 2.0F;
		ypos[i] += vres / 384.0F * 30.0F;
		beginobj(&ii, 10.0F, pos + vres / 384.0F * 30.0F, 1.0F, 0)->id = 0;
	}
	ii.h = pos;
	ii.w = 1.0F;
	for(pos = 10.0F, i = 0; pos <= hres - 10.0F; pos += (hres - 20.0F) / 4.0F, i++)
	{
		xpos[i] = (pos + (pos + (hres - 20.0F) / 4.0F)) / 2.0F;
		beginobj(&ii, pos, vres / 384.0F * 30.0F, 1.0F, 0)->id = 0;
	}

	set_text_font(1);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	for(n = 2048, i = 0; n; n >>= 1, i++)
	{
		set_text_position((int)xpos[0], (int)ypos[i], 1.0F);
		oprintf("%d", n);
	}
	set_text_position((int)xpos[0], (int)ypos[12], 1.0F);
	set_text_color(0xff00ffff);
	oprintf("SEC/TRANS");
	set_text_position((int)xpos[1], (int)ypos[12], 1.0F);
	oprintf("LOW (MB/s)");
	set_text_position((int)xpos[2], (int)ypos[12], 1.0F);
	oprintf("AVG (MB/s)");
	set_text_position((int)xpos[3], (int)ypos[12], 1.0F);
	oprintf("HIGH (MB/s)");

	set_text_font(2);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_color(0xffffffff);
	set_text_position((int)(hres / 2.0F), (int)(font2_height * 2.0F), 1.0F);
	oprintf("CREDIT OR TEST TO ABORT AND EXIT");
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_font(1);

	for(secs = 1, row = 11; secs <= 2048; secs <<= 1, row--)
	{
		set_string_id(0x12345678);
		set_text_color(0xffff0000);
		set_text_position((int)xpos[2], (int)ypos[row], 1.0F);
		oprintf("TEST TEST TEST TEST TEST TEST TEST TEST");
		sleep(1);

		high = 0.0F;
		low = 10000.0F;
		count = 1.0F;
		sectors = ((DriveID *)get_ide_ident_info())->lba_capacity;
		buf = malloc(0x100000);
		if(!buf)
		{
			result[0] = 0.0F;
			result[2] = 0.0F;
			result[1] = 0.0F;
		}
		else
		{
			sum = 0.0F;
			range = sectors - secs;
			for(i = 0; i < secs * 100; i += secs)
			{
				sec = random();
				sec %= range;
				_ioctl(TIMER_FD, FIOCSTARTTIMER1, 0);
				_SecReads(sec, buf, secs);
				_ioctl(TIMER_FD, FIOCGETTIMER1, &ticks);
				_ioctl(TIMER_FD, FIOCSTOPTIMER1, 0);
				rate = ticks;
				rate /= 1000000000.0F;
				rate = (secs * 512) / rate;
				rate /= 1000000.0F;
				sum += rate;
				if(high > 45.0F)
					high = 0.0F;
				if(rate > high)
					high = rate;
				if(rate < low)
					low = rate;
				count += 1.0F;
				if(sw_select() || sw_back())
					break;
			}
			result[0] = low;
			result[2] = high;
			result[1] = sum / count;
			free(buf);
		}
		if(sw_select() || sw_back())
			break;

		delete_multiple_strings(0x12345678, -1);
		set_string_id(0);
		set_text_color(0xffffffff);
		set_text_position((int)xpos[1], (int)ypos[row], 1.0F);
		oprintf("%5.2f", result[0]);
		set_text_position((int)xpos[2], (int)ypos[row], 1.0F);
		oprintf("%5.2f", result[1]);
		set_text_position((int)xpos[3], (int)ypos[row], 1.0F);
		oprintf("%5.2f", result[2]);
		sleep(1);
	}
	if(!(sw_select() || sw_back()))
		wait_prompt(0);
	else
		while(sw_select() || sw_back())
			sleep(1);
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	return 0;
}

/* 0x80106054: the random read test again under its cache title (it reads with _SecReads all
   the same) */
int ide_random_cache_read_test(void)
{
	image_info_t	ii = grid_ii;
	float	ypos[13];
	float	xpos[4];
	float	result[3];
	int	ticks;
	float	pos = 0.0F, rate, low, high, sum, count;
	int	i, n, secs, sec, row, sectors, range;
	void	*buf;

	result[0] = 0.0F;
	result[2] = 0.0F;
	result[1] = 0.0F;
	draw_title("IDE RANDOM CACHE READ TEST");

	ii.w = hres - 20.0F;
	ii.h = 1.0F;
	for(i = 0; pos <= font1_height * 24.0F; pos += font1_height * 2.0F, i++)
	{
		ypos[i] = (pos + (pos + font1_height * 2.0F)) / 2.0F;
		ypos[i] += vres / 384.0F * 30.0F;
		beginobj(&ii, 10.0F, pos + vres / 384.0F * 30.0F, 1.0F, 0)->id = 0;
	}
	ii.h = pos;
	ii.w = 1.0F;
	for(pos = 10.0F, i = 0; pos <= hres - 10.0F; pos += (hres - 20.0F) / 4.0F, i++)
	{
		xpos[i] = (pos + (pos + (hres - 20.0F) / 4.0F)) / 2.0F;
		beginobj(&ii, pos, vres / 384.0F * 30.0F, 1.0F, 0)->id = 0;
	}

	set_text_font(1);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	for(n = 2048, i = 0; n; n >>= 1, i++)
	{
		set_text_position((int)xpos[0], (int)ypos[i], 1.0F);
		oprintf("%d", n);
	}
	set_text_position((int)xpos[0], (int)ypos[12], 1.0F);
	set_text_color(0xff00ffff);
	oprintf("SEC/TRANS");
	set_text_position((int)xpos[1], (int)ypos[12], 1.0F);
	oprintf("LOW (MB/s)");
	set_text_position((int)xpos[2], (int)ypos[12], 1.0F);
	oprintf("AVG (MB/s)");
	set_text_position((int)xpos[3], (int)ypos[12], 1.0F);
	oprintf("HIGH (MB/s)");

	set_text_font(2);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_color(0xffffffff);
	set_text_position((int)(hres / 2.0F), (int)(font2_height * 2.0F), 1.0F);
	oprintf("CREDIT OR TEST TO ABORT AND EXIT");
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_font(1);

	for(secs = 1, row = 11; secs <= 2048; secs <<= 1, row--)
	{
		set_string_id(0x12345678);
		set_text_color(0xffff0000);
		set_text_position((int)xpos[2], (int)ypos[row], 1.0F);
		oprintf("TEST TEST TEST TEST TEST TEST TEST TEST");
		sleep(1);

		high = 0.0F;
		low = 10000.0F;
		count = 1.0F;
		sectors = ((DriveID *)get_ide_ident_info())->lba_capacity;
		buf = malloc(0x100000);
		if(!buf)
		{
			result[0] = 0.0F;
			result[2] = 0.0F;
			result[1] = 0.0F;
		}
		else
		{
			sum = 0.0F;
			range = sectors - secs;
			for(i = 0; i < secs * 100; i += secs)
			{
				sec = random();
				sec %= range;
				_ioctl(TIMER_FD, FIOCSTARTTIMER1, 0);
				_SecReads(sec, buf, secs);
				_ioctl(TIMER_FD, FIOCGETTIMER1, &ticks);
				_ioctl(TIMER_FD, FIOCSTOPTIMER1, 0);
				rate = ticks;
				rate /= 1000000000.0F;
				rate = (secs * 512) / rate;
				rate /= 1000000.0F;
				sum += rate;
				if(high > 45.0F)
					high = 0.0F;
				if(rate > high)
					high = rate;
				if(rate < low)
					low = rate;
				count += 1.0F;
				if(sw_select() || sw_back())
					break;
			}
			result[0] = low;
			result[2] = high;
			result[1] = sum / count;
			free(buf);
		}
		if(sw_select() || sw_back())
			break;

		delete_multiple_strings(0x12345678, -1);
		set_string_id(0);
		set_text_color(0xffffffff);
		set_text_position((int)xpos[1], (int)ypos[row], 1.0F);
		oprintf("%5.2f", result[0]);
		set_text_position((int)xpos[2], (int)ypos[row], 1.0F);
		oprintf("%5.2f", result[1]);
		set_text_position((int)xpos[3], (int)ypos[row], 1.0F);
		oprintf("%5.2f", result[2]);
		sleep(1);
	}
	if(!(sw_select() || sw_back()))
		wait_prompt(0);
	else
		while(sw_select() || sw_back())
			sleep(1);
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	return 0;
}

/* 0x80106824: the drive's identify information: model, serial number, firmware, the current
   geometry and capacity and the DMA and PIO timings it supports */
int ide_disk_info(void)
{
	char	buf[40];
	char	*p;
	DriveID	*id;
	float	y;
	unsigned int	capacity;

	id = get_ide_ident_info();
	y = vres - font0_height * 2.0F;
	draw_title("IDE DISK INFORMATION");
	y -= font0_height + font1_height * 2.0F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_font(1);
	set_text_position(20, (int)y, 1.0F);
	oprintf("\x1b%dcModel Number:", -1);
	set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
	set_text_justification_mode(HJUST_RIGHT);
	memcpy(buf, id->model, sizeof(id->model));
	for(p = &buf[sizeof(id->model) - 1]; *p == ' '; p--)
		;
	p[1] = 0;
	oprintf("\x1b%dc%s", 0xffffff00, buf);

	y -= font1_height * 1.5F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_position(20, (int)y, 1.0F);
	oprintf("\x1b%dcSerial Number:", -1);
	set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
	set_text_justification_mode(HJUST_RIGHT);
	memcpy(buf, id->serial_no, sizeof(id->serial_no));
	for(p = &buf[sizeof(id->serial_no) - 1]; *p == ' '; p--)
		;
	p[1] = 0;
	oprintf("\x1b%dc%s", 0xffffff00, buf);

	y -= font1_height * 1.5F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_position(20, (int)y, 1.0F);
	oprintf("\x1b%dcFirware Revision:", -1);
	set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
	set_text_justification_mode(HJUST_RIGHT);
	memcpy(buf, id->fw_rev, sizeof(id->fw_rev));
	for(p = &buf[sizeof(id->fw_rev) - 1]; *p == ' '; p--)
		;
	p[1] = 0;
	oprintf("\x1b%dc%s", 0xffffff00, buf);

	if(id->field_valid & 1)
	{
		y -= font1_height * 1.5F;
		set_text_justification_mode(HJUST_LEFT);
		set_text_position(20, (int)y, 1.0F);
		oprintf("\x1b%dcNumber of current logical sectors:", -1);
		set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
		set_text_justification_mode(HJUST_RIGHT);
		oprintf("\x1b%dc%hu", 0xffffff00, id->cur_cyls);

		y -= font1_height * 1.5F;
		set_text_justification_mode(HJUST_LEFT);
		set_text_position(20, (int)y, 1.0F);
		oprintf("\x1b%dcNumber of current logical heads:", -1);
		set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
		set_text_justification_mode(HJUST_RIGHT);
		oprintf("\x1b%dc%hu", 0xffffff00, id->cur_heads);

		y -= font1_height * 1.5F;
		set_text_justification_mode(HJUST_LEFT);
		set_text_position(20, (int)y, 1.0F);
		oprintf("\x1b%dcNumber of current logical sectors/track:", -1);
		set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
		set_text_justification_mode(HJUST_RIGHT);
		oprintf("\x1b%dc%hu", 0xffffff00, id->cur_sectors);

		capacity = id->cur_capacity0;
		capacity |= id->cur_capacity1 << 16;
		y -= font1_height * 1.5F;
		set_text_justification_mode(HJUST_LEFT);
		set_text_position(20, (int)y, 1.0F);
		oprintf("\x1b%dcCurrent capacity:", -1);
		set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
		set_text_justification_mode(HJUST_RIGHT);
		oprintf("\x1b%dc%u", 0xffffff00, capacity);
	}

	y -= font1_height * 1.5F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_position(20, (int)y, 1.0F);
	oprintf("\x1b%dcMultiword DMA modes supported:", -1);
	set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("\x1b%dc%hu", 0xffffff00, id->dma_mword & 0xff);

	y -= font1_height * 1.5F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_position(20, (int)y, 1.0F);
	oprintf("\x1b%dcMultiword DMA mode active:", -1);
	set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
	set_text_justification_mode(HJUST_RIGHT);
	oprintf("\x1b%dc%hu", 0xffffff00, id->dma_mword >> 8);

	if(id->field_valid & 2)
	{
		y -= font1_height * 1.5F;
		set_text_justification_mode(HJUST_LEFT);
		set_text_position(20, (int)y, 1.0F);
		oprintf("\x1b%dcAdvanced PIO modes supported:", -1);
		set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
		set_text_justification_mode(HJUST_RIGHT);
		oprintf("\x1b%dc%hu", 0xffffff00, id->eide_pio_modes & 0xff);

		y -= font1_height * 1.5F;
		set_text_justification_mode(HJUST_LEFT);
		set_text_position(20, (int)y, 1.0F);
		oprintf("\x1b%dcMin DMA cycle time:", -1);
		set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
		set_text_justification_mode(HJUST_RIGHT);
		oprintf("\x1b%dc%hu (ns)", 0xffffff00, id->eide_dma_min);

		y -= font1_height * 1.5F;
		set_text_justification_mode(HJUST_LEFT);
		set_text_position(20, (int)y, 1.0F);
		oprintf("\x1b%dcRecommended DMA cycle time:", -1);
		set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
		set_text_justification_mode(HJUST_RIGHT);
		oprintf("\x1b%dc%hu (ns)", 0xffffff00, id->eide_dma_time);

		y -= font1_height * 1.5F;
		set_text_justification_mode(HJUST_LEFT);
		set_text_position(20, (int)y, 1.0F);
		oprintf("\x1b%dcMin PIO cycle time w/o flow control:", -1);
		set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
		set_text_justification_mode(HJUST_RIGHT);
		oprintf("\x1b%dc%hu (ns)", 0xffffff00, id->eide_pio);

		y -= font1_height * 1.5F;
		set_text_justification_mode(HJUST_LEFT);
		set_text_position(20, (int)y, 1.0F);
		oprintf("\x1b%dcMin PIO cycle time with flow control:", -1);
		set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
		set_text_justification_mode(HJUST_RIGHT);
		oprintf("\x1b%dc%hu (ns)", 0xffffff00, id->eide_pio_iordy);
	}
	wait_test_exit();
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	return 0;
}

/* 0x801070a8: times a read of every 16 sectors of the disk, showing the fastest and slowest
   sector, the highest, lowest, current and average rate as it goes */
int ide_surface_scan(void)
{
	char	buf[40];
	unsigned int	ticks;
	char	*p;
	DriveID	*id;
	float	y, sector_y, fast_y, slow_y, high_y, low_y, avg_y, rate_y;
	float	sum, avg;
	unsigned int	total, sector, next, slowest, fastest, slow_sector, fast_sector;
	void	*mem;
	char	*sec_buf;

	id = get_ide_ident_info();
	y = vres - font0_height * 2.0F;
	draw_title("IDE DISK SURFACE SCAN");
	mem = malloc(0x2020);
	sec_buf = mem;
	while((int)sec_buf & 0x1f)
		sec_buf++;

	y -= font0_height + font1_height * 2.0F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_font(1);
	set_string_id(0x4f00);
	set_text_position(20, (int)y, 1.0F);
	oprintf("\x1b%dcModel Number:", -1);
	set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
	set_text_justification_mode(HJUST_RIGHT);
	memcpy(buf, id->model, sizeof(id->model));
	for(p = &buf[sizeof(id->model) - 1]; *p == ' '; p--)
		;
	p[1] = 0;
	oprintf("\x1b%dc%s", 0xffffff00, buf);

	y -= font1_height * 1.5F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_position(20, (int)y, 1.0F);
	oprintf("\x1b%dcSerial Number:", -1);
	set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
	set_text_justification_mode(HJUST_RIGHT);
	memcpy(buf, id->serial_no, sizeof(id->serial_no));
	for(p = &buf[sizeof(id->serial_no) - 1]; *p == ' '; p--)
		;
	p[1] = 0;
	oprintf("\x1b%dc%s", 0xffffff00, buf);

	y -= font1_height * 1.5F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_position(20, (int)y, 1.0F);
	oprintf("\x1b%dcFirware Revision:", -1);
	set_text_position((int)(hres - 20.0F), (int)y, 1.0F);
	set_text_justification_mode(HJUST_RIGHT);
	memcpy(buf, id->fw_rev, sizeof(id->fw_rev));
	for(p = &buf[sizeof(id->fw_rev) - 1]; *p == ' '; p--)
		;
	p[1] = 0;
	oprintf("\x1b%dc%s", 0xffffff00, buf);

	y -= font1_height * 1.5F;
	y -= font1_height * 1.5F;
	sector_y = y - font1_height * 1.5F;
	ticks = 0;
	total = (id->cur_capacity1 << 16) | id->cur_capacity0;
	set_text_justification_mode(HJUST_LEFT);
	set_text_position(20, (int)sector_y, 1.0F);
	oprintf("\x1b%dcCurrent Sector:", -1);
	fast_y = sector_y - font1_height * 1.5F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_position(20, (int)fast_y, 1.0F);
	oprintf("\x1b%dcFastest Sector:", -1);
	slow_y = fast_y - font1_height * 1.5F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_position(20, (int)slow_y, 1.0F);
	oprintf("\x1b%dcSlowest Sector:", -1);
	high_y = slow_y - font1_height * 1.5F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_position(20, (int)high_y, 1.0F);
	oprintf("\x1b%dcHighest Rate:", -1);
	low_y = high_y - font1_height * 1.5F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_position(20, (int)low_y, 1.0F);
	oprintf("\x1b%dcSlowest Rate:", -1);
	rate_y = low_y - font1_height * 1.5F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_position(20, (int)rate_y, 1.0F);
	oprintf("\x1b%dcCurrent Rate:", -1);
	avg_y = rate_y - font1_height * 1.5F;
	set_text_justification_mode(HJUST_LEFT);
	set_text_position(20, (int)avg_y, 1.0F);
	oprintf("\x1b%dcAverage Rate:", -1);

	set_text_font(2);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_color(0xffffffff);
	set_text_position((int)(hres / 2.0F), (int)(font2_height * 2.0F), 1.0F);
	oprintf("CREDIT OR TEST TO ABORT AND EXIT");
	set_string_id(0x4f01);
	set_text_justification_mode(HJUST_RIGHT);
	set_text_color(0xffffff00);
	set_text_font(1);

	slow_sector = 0;
	fast_sector = 0;
	slowest = 0;
	fastest = 0x80000000;
	sum = 0.0F;
	sector = 0;
	_SecReads(0, sec_buf, 16);
	do
	{
		_ioctl(TIMER_FD, FIOCSTARTTIMER3, 0);
		_SecReads(sector, sec_buf, 16);
		_ioctl(TIMER_FD, FIOCGETTIMER3, &ticks);
		_ioctl(TIMER_FD, FIOCSTOPTIMER3, 0);
		sum += ticks;
		if(ticks > slowest)
		{
			slowest = ticks;
			slow_sector = sector;
		}
		if(ticks < fastest)
		{
			fastest = ticks;
			fast_sector = sector;
		}
		next = sector + 16;
		avg = sum / next;
		delete_multiple_strings(0x4f01, -1);
		set_text_position((int)(hres - 20.0F), (int)sector_y, 1.0F);
		oprintf("%d OF %d", sector, total);
		set_text_position((int)(hres - 20.0F), (int)slow_y, 1.0F);
		oprintf("%u", slow_sector);
		set_text_position((int)(hres - 20.0F), (int)fast_y, 1.0F);
		oprintf("%u", fast_sector);
		set_text_position((int)(hres - 20.0F), (int)low_y, 1.0F);
		oprintf("%6.2f MB/S", 1.0F / (slowest / 16000.0F / 512.0F));
		set_text_position((int)(hres - 20.0F), (int)high_y, 1.0F);
		oprintf("%6.2f MB/S", 1.0F / (fastest / 16000.0F / 512.0F));
		set_text_position((int)(hres - 20.0F), (int)rate_y, 1.0F);
		oprintf("%6.2f MB/S", 1.0F / (ticks / 16000.0F / 512.0F));
		set_text_position((int)(hres - 20.0F), (int)avg_y, 1.0F);
		oprintf("%6.2f MB/S\r\n", 1.0F / (avg / 1000.0F / 512.0F));
		sleep(1);
		if(sw_select() || sw_back())
			break;
		sector = next;
	} while(sector <= total - 16);
	free(mem);

	if(!(sw_select() || sw_back()))
		wait_prompt(0);
	else
		while(sw_select() || sw_back())
			sleep(1);
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	return 0;
}
