/*
 * integrity.c - DISK DATA INTEGRITY (0x80115920-0x80116020), of the DISK TESTS menu: until
 * TEST or CREDIT is held, a half megabyte test file of counting bytes is written, read back
 * and its bytes and CRC checked, counting the passes, good and bad.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <goose/goose.h>
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

extern void	sleep(int);
extern int	(*sw_exit)(void);		/* 0x80167d80: nothing on CarnEvil */
extern int	remove(const char *);	/* unlink() or remove(): calls _remove */

#define TEST_SIZE	0x80000

/* 0x80115920 */
int disk_data_integrity(void)
{
	unsigned char	*buf;
	FILE		*fp;
	unsigned char	sum;
	unsigned char	seed, start;
	int		i, errors;
	int		passes, good, bad;

	bad = 0;
	good = 0;
	passes = 0;
	seed = 1;
	draw_title("DISK DATA INTEGRITY");
	message_box("HOLD CREDIT/TEST TO EXIT", -1.0F, 5.0F, 1.0F);
	sleep(1);
	buf = malloc(TEST_SIZE);
	if(!buf)
	{
		fprintf(stderr, "Can not allocate memory\r\n");
		set_text_color(0xffff0000);
		set_string_id(0x4f00);
		set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		set_text_font(0);
		oprintf("CANT ALLOCATE BUFFER");
		sleep(120);
		delete_multiple_strings(0, 0);
		del1c(0, 0);
		return 0;
	}
	set_text_color(0xffff0000);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_text_position((int)(hres / 2.0F), (int)(vres * 0.79F), 1.0F);
	set_text_font(0);
	oprintf("WARNING");
	set_text_position((int)(hres / 2.0F), (int)(vres * 0.72F), 1.0F);
	oprintf("DO NOT POWER DOWN DURING TEST");
	set_text_position((int)(hres / 2.0F), (int)(vres * 0.65F), 1.0F);
	oprintf("HOLD CREDIT OR TEST TO EXIT");
	do
	{
		delete_multiple_strings(0x4f00, -1);
		set_text_color(0xffffff00);
		set_string_id(0x4f00);
		set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		start = seed;
		set_text_font(0);
		oprintf("WRITING TEST FILE");
		sleep(1);
		for(i = 0; i < TEST_SIZE; i++)
		{
			buf[i] = seed;
			seed++;
			if(!seed)
				seed = 1;
		}
		sum = crc(buf, TEST_SIZE);
		fp = fopen("test.dat", "wb");
		if(fwrite(buf, 1, TEST_SIZE, fp) != TEST_SIZE)
			fclose(fp);
		fclose(fp);
		delete_multiple_strings(0x4f00, -1);
		memset(buf, 0, TEST_SIZE);
		fp = fopen("test.dat", "rb");
		delete_multiple_strings(0x4f00, -1);
		set_text_color(0xffffff00);
		set_string_id(0x4f00);
		set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		set_text_font(0);
		oprintf("READING BACK TEST FILE");
		sleep(1);
		if(fread(buf, 1, TEST_SIZE, fp) != TEST_SIZE)
			fclose(fp);
		fclose(fp);
		seed = start;
		errors = 0;
		for(i = 0; i < TEST_SIZE; i++)
		{
			if(buf[i] != seed)
				errors++;
			seed++;
			if(!seed)
				seed = 1;
		}
		delete_multiple_strings(0x4f00, -1);
		set_text_color(0xffffff00);
		set_string_id(0x4f00);
		set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		set_text_font(0);
		oprintf("CHECK TEST FILE DATA INTEGRITY");
		sleep(1);
		if(sum != (unsigned char)crc(buf, TEST_SIZE))
			errors |= 1;
		set_text_font(1);
		set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
		set_string_id(0x4f01);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F - 20.0F), 1.0F);
		passes++;
		if(errors)
			bad++;
		else
			good++;
		delete_multiple_strings(0x4f01, -1);
		oprintf("\x1b%dcPASSES:  %d - \x1b%dcGOOD:  %d - \x1b%dcBAD:  %d",
			0xffffffff, passes, 0xff00ff00, good, 0xffff0000, bad);
		sleep(1);
	} while(!sw_select() && !sw_back() && !sw_exit());
	set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F - 40.0F), 1.0F);
	oprintf("\x1b%dcDATA INTREGRITY TEST ABORT", 0xffffffff);
	while(sw_select() || sw_back() || sw_exit())
		sleep(1);
	free(buf);
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	remove("test.dat");
	return 0;
}
