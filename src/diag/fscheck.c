/*
 * fscheck.c - FILE SYSTEM CHECK (0x80114b90-0x80115920), of the DISK TESTS menu. filesys.chk
 * is a list of the disk's files, each as findfirst() found it with the CRC of its contents,
 * and the CRC of the list in its last word; every file in it is read and its CRC checked.
 */
#include <stdio.h>
#include <stdlib.h>
#include <dir.h>
#include <goose/goose.h>
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

extern void	sleep(int);
extern int	(*sw_exit)(void);		/* 0x80167d80: nothing on CarnEvil */

/* an entry of filesys.chk */
struct fs_entry {
	struct ffblk	ff;		/* ff_attrib: set while the file is unchecked or bad */
	unsigned long	crc __attribute__((packed));
};

static struct fs_entry	*fs_list;	/* 0x801734a8: filesys.chk */

/* 0x80114b90 */
int file_system_check(void)
{
	struct ffblk	ff;
	FILE		*fp;
	unsigned char	*buf;
	int		n, count, i, checked;	/* n: the loop's count, then the errors */
	int		not_found, not_open, alloc_err, read_err, data_err;
	float		x, y, top;

	draw_title("FILE SYSTEM CHECK");
	set_text_font(1);
	set_text_color(0xffffff00);
	set_text_justification_mode(HJUST_CENTER|VJUST_CENTER);
	set_string_id(0x4f00);
	if(findfirst("filesys.chk", &ff, 0))
	{
		set_text_color(0xffff0000);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		oprintf("FILE SYSTEM CHECK FILE NOT FOUND");
		wait_test_exit();
		delete_multiple_strings(0, 0);
		return 0;
	}
	fs_list = malloc(ff.ff_fsize);
	if(!fs_list)
	{
		set_text_color(0xffff0000);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		oprintf("MEMORY ALLOCATION FAILURE");
		wait_test_exit();
		delete_multiple_strings(0, 0);
		return 0;
	}
	fp = fopen("filesys.chk", "rb");
	if(!fp)
	{
		set_text_color(0xffff0000);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		oprintf("CAN NOT OPEN FILE SYSTEM CHECK FILE");
		wait_test_exit();
		delete_multiple_strings(0, 0);
		free(fs_list);
		return 0;
	}
	if(fread(fs_list, 1, ff.ff_fsize, fp) != ff.ff_fsize)
	{
		fclose(fp);
		set_text_color(0xffff0000);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		oprintf("CAN NOT READ FILE SYSTEM CHECK FILE");
		wait_test_exit();
		delete_multiple_strings(0, 0);
		free(fs_list);
		return 0;
	}
	fclose(fp);
	if(crc((unsigned char *)fs_list, ff.ff_fsize - 4) != ((unsigned long *)fs_list)[ff.ff_fsize / 4 - 1])
	{
		set_text_color(0xffff0000);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		oprintf("CRC CHECK FAILURE ON FILESYS.CHK");
		wait_test_exit();
		delete_multiple_strings(0, 0);
		free(fs_list);
		return 0;
	}

	count = ff.ff_fsize / sizeof(struct fs_entry);
	n = count;
	set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F + 20.0F), 1.0F);
	set_text_color(0xffffffff);
	set_string_id(0x4f01);
	oprintf("CHECKING %d FILE%s", count, count > 1 ? "S" : "");
	set_string_id(0x4f00);
	not_found = 0;
	not_open = 0;
	alloc_err = 0;
	read_err = 0;
	data_err = 0;
	message_box("HOLD CREDIT/TEST TO EXIT", -1.0F, 5.0F, 1.0F);
	for(i = 0; i < n; i++)
	{
		if(sw_select() || sw_back() || sw_exit())
			break;
		delete_multiple_strings(0x4f00, -1);
		set_string_id(0x4f00);
		set_text_font(1);
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		set_text_color(0xffffffff);
		oprintf("%d - %s", i, fs_list[i].ff.ff_name);
		sleep(1);
		fs_list[i].ff.ff_attrib = 1;
		if(!findfirst(fs_list[i].ff.ff_name, &ff, 0))
		{
			fp = fopen(fs_list[i].ff.ff_name, "rb");
			if(fp)
			{
				buf = malloc(ff.ff_fsize);
				if(buf)
				{
					if(fread(buf, 1, ff.ff_fsize, fp) == ff.ff_fsize)
					{
						if(crc(buf, fs_list[i].ff.ff_fsize) != fs_list[i].crc)
							data_err++;
						else
							fs_list[i].ff.ff_attrib = 0;
					}
					else
						read_err++;
					free(buf);
				}
				else
					alloc_err++;
				fclose(fp);
			}
			else
				not_open++;
		}
		else
			not_found++;
	}
	delete_multiple_strings(0x4f00, -2);
	set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F + 20.0F), 1.0F);
	checked = i;
	set_text_color(0xffffffff);
	if(checked == count)
		oprintf("%d FILE%s CHECKED", checked, checked > 1 ? "S" : "");
	else
		oprintf("ABORTED AFTER %d OF %d FILE%s CHECKED", checked, count, checked > 1 ? "S" : "");
	if(not_found | not_open | alloc_err | read_err | data_err)
	{
		n = not_found + not_open + alloc_err + read_err + data_err;
		y = vres / 2.0F;
		set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
		set_text_color(0xffff0000);
		oprintf("ERRORS DETECTED IN %d FILE%s", n, n > 1 ? "S" : "");
		set_text_color(0xffffff00);
		if(not_found)
		{
			y -= font1_height + font1_height;
			set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
			oprintf("%d FILE%s NOT FOUND", not_found, not_found > 1 ? "S" : "");
		}
		if(not_open)
		{
			y -= font1_height + font1_height;
			set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
			oprintf("%d FILE%s NOT OPENABLE", not_open, not_open > 1 ? "S" : "");
		}
		if(alloc_err)
		{
			y -= font1_height + font1_height;
			set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
			oprintf("%d FILE ALLOCATION ERROR%s", not_open, alloc_err > 1 ? "S" : "");
		}
		if(read_err)
		{
			y -= font1_height + font1_height;
			set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
			oprintf("%d READ FAILURE%s", read_err, read_err > 1 ? "S" : "");
		}
		if(data_err)
		{
			y -= font1_height + font1_height;
			set_text_position((int)(hres / 2.0F), (int)y, 1.0F);
			oprintf("%d FILE DATA FAILURE%s", data_err, data_err > 1 ? "S" : "");
		}
		x = hres * 0.05F;
		set_text_color(0xffffffff);
		set_text_justification_mode(HJUST_LEFT|VJUST_CENTER);
		set_text_font(2);
		y -= font1_height + font1_height;
		top = y;
		for(i = 0; i < checked; i++)
		{
			if(fs_list[i].ff.ff_attrib)
			{
				set_text_position((int)x, (int)y, 1.0F);
				oprintf("%s", fs_list[i].ff.ff_name);
				y -= font2_height + font2_height;
				if(y < 40.0F)
				{
					y = top;
					x += 128.0F;
				}
			}
		}
	}
	else
	{
		set_text_position((int)(hres / 2.0F), (int)(vres / 2.0F), 1.0F);
		set_text_color(0xff00ff00);
		oprintf("%d FILE%s OK", checked, checked > 1 ? "S" : "");
	}
	if(checked < count)
	{
		while(sw_select() || sw_back() || sw_exit())
			sleep(1);
	}
	else
	{
		clear_message_box();
		wait_test_exit();
	}
	free(fs_list);
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	return 0;
}
