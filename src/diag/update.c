/*
 * update.c - the software update (0x80116020-0x80118194): an update ROM on the board carries
 * one part of an update; each part is saved to the disk as UPLrrNnn.ROM until all of them are
 * there, then they are joined into UPD.DAT, a gzip-compressed script of file operations, which
 * is run, and GAMEINF.REV gets the new revision.
 */
#include <stdio.h>
#include <unistd.h>
#include <goose/goose.h>
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

typedef void	*gzFile;
extern void	sleep(int);
extern int	exec(char *, unsigned int, int *);
extern int	graphical_printf(char *, char *, ...);

/* updfile.c */
extern int	file_open(char *, int *);
extern int	file_create(char *, int *);
extern int	file_close(int);
extern int	file_exists(char *);
extern int	file_delete(char *);
extern int	file_move(char *, char *);
extern int	file_copy(char *, char *);
extern int	file_read(int, void *, int);
extern int	file_write(int, void *, int);
extern int	file_seek(int, int, int);
extern int	file_tell(int, int *);
extern int	zfile_open(char *, gzFile *);
extern int	zfile_close(gzFile);
extern int	zfile_read(gzFile, void *, int);
extern unsigned short	checksum(unsigned char *, int);
extern char	*rom_file_name();			/* (unsigned char rev, unsigned char rom): called
							   without a prototype, the halfwords go unmasked */

/* a game and its software revision, as GAMEINF.REV starts */
typedef struct rev {
	unsigned short	game;
	unsigned short	rev;
} rev_t;

/* the start of each update ROM (and of its UPLrrNnn.ROM file) */
typedef struct upd_header {
	int		magic;			/* UPD_MAGIC */
	int		size;			/* of the image: header, data and trailer */
	unsigned short	sum;			/* checksum() of the image */
	unsigned short	nsum;			/* ~sum */
	char		desc[28];		/* GAMEINF.REV's text for the new revision */
	rev_t		from;			/* what it updates */
	rev_t		to;			/* and to what */
	unsigned short	roms;			/* the ROMs of the update */
	unsigned short	rom;			/* which one this is, from 1 */
	char		pad[12];
} upd_header_t;

#define UPD_MAGIC	'UTRP'
#define UPD_TRAILER	0x47434d00		/* the image's last 4 bytes as a word, the first
						   byte aside: "?MCG" */
#define ROM_SIZE	0x100000
#define ROM_BASE	0xbfd00000

float		update_text_y;			/* 0x802fdd00 */
static unsigned char	buf[0x20000];		/* 0x801734b0 */
static char	msg[256];			/* 0x801934b0 */
static unsigned char	rom_image[ROM_SIZE];	/* 0x801935b0 */
static int	rom_loaded = 0;			/* 0x80167d60 */

int	rom_verify(void);
int	rom_read_header(upd_header_t *);
int	rev_info_read(rev_t *, char *);
int	rev_info_write(rev_t *, char *);
int	rom_file_saved(int, int);
void	roms_missing(int, int, int *, int *);
int	rom_save(int, int, int);
int	update_assemble(void);
int	update_run_script(void);

/* the update ROM, read whole on first use */
static void rom_load(void)
{
	int	*s, *d;
	int	i;

	if(!rom_loaded)
	{
		d = (int *)rom_image;
		s = (int *)ROM_BASE;
		i = ROM_SIZE / 4;
		while(i--)
			*d++ = *s++;
		rom_loaded = 1;
	}
}

static int rom_read(int offset, void *to, int len)
{
	unsigned char	*s, *d;
	int		r;

	rom_load();
	if(offset + len <= ROM_SIZE)
	{
		s = rom_image + offset;
		d = to;
		while(len--)
			*d++ = *s++;
		r = 0;
	}
	else
		r = -12;
	return r;
}

/* 0x80116020: 0, or -11 without an update ROM, -13 when its checksum is wrong */
int update_rom_check(void)
{
	int	r;

	r = rom_verify();
	if(r == -13)
		printf("The update ROM appears to be corrupted\n");
	return r;
}

/* the update's next line of text */
static void update_message(char *s)
{
	set_text_position((int)(hres / 2.0F), (int)update_text_y, 1.0F);
	set_text_color(0xffffffff);
	set_text_font(2);
	oprintf("%s", s);
	sleep(1);
	update_text_y -= font2_height;
	update_text_y -= font2_height;
}

/* 0x80116068: save this ROM's part of the update; with every part there, apply it */
int update_rom_process(void)
{
	upd_header_t	hdr;
	rev_t		rev;
	char		desc[28];
	int		count, next;
	int		r;

	r = rom_read_header(&hdr);
	if(!r)
	{
		r = rev_info_read(&rev, desc);
		if(!r)
		{
			if(rev.game == hdr.from.game)
			{
				if(rev.rev == hdr.to.rev)
				{
					graphical_printf(msg, "SOFTWARE ALREADY UPDATED");
					update_message(msg);
					r = -16;
				}
				else if(rev.rev == hdr.from.rev)
				{
					if(rom_file_saved(hdr.to.rev, hdr.rom))
					{
						graphical_printf(msg, "ROM number %d already processed", hdr.rom);
						update_message(msg);
						roms_missing(hdr.to.rev, hdr.roms, &count, &next);
						if(count > 0)
						{
							graphical_printf(msg, "%d ROM%s needed to complete this update", count, count == 1 ? "" : "s");
							update_message(msg);
							graphical_printf(msg, "Please turn off the game and load ROM number %d next", next);
							update_message(msg);
						}
						r = -17;
					}
					else
					{
						graphical_printf(msg, "Processing ROM number %d", hdr.rom);
						update_message(msg);
						r = rom_save(hdr.to.rev, hdr.rom, hdr.size);
						if(!r)
						{
							graphical_printf(msg, "ROM number %d loaded", hdr.rom);
							update_message(msg);
							roms_missing(hdr.to.rev, hdr.roms, &count, &next);
							if(count > 0)
							{
								graphical_printf(msg, "%d ROM%s needed to complete this update", count, count == 1 ? "" : "s");
								update_message(msg);
								graphical_printf(msg, "Please turn off the game and load ROM number %d next", next);
								update_message(msg);
								r = -19;
							}
							else
							{
								r = update_assemble();
								if(!r)
								{
									r = update_run_script();
									if(r)
									{
										graphical_printf(msg, "ERROR PROCESSING SCRIPT FILE");
										update_message(msg);
									}
									else
									{
										r = rev_info_write(&hdr.to, hdr.desc);
										if(r)
										{
											graphical_printf(msg, "ERROR UPDATING REVISION INFO");
											update_message(msg);
										}
										file_delete("UPD.DAT");
									}
								}
							}
						}
						else
						{
							graphical_printf(msg, "An error occured while trying to process ROM number %d", hdr.rom);
							update_message(msg);
						}
					}
				}
				else
				{
					graphical_printf(msg, "ROM is wrong revision level");
					update_message(msg);
					r = -15;
				}
			}
			else
			{
				graphical_printf(msg, "ROM IS NOT FOR THIS GAME");
				update_message(msg);
				r = -14;
			}
		}
		else
		{
			graphical_printf(msg, "REV. INFO BAD");
			update_message(msg);
		}
	}
	return r;
}

/* 0x8011693c */
int rom_read_header(upd_header_t *hdr)
{
	int	r;

	r = rom_read(0, hdr, sizeof(*hdr));
	if(r)
		return r;
	return hdr->magic == UPD_MAGIC ? 0 : -11;
}

static int rom_read_trailer(int size, unsigned char *trailer)
{
	int	r;

	r = rom_read(size - 4, trailer, 4);
	if(r)
		return r;
	return (*(int *)trailer & 0xffffff00) == UPD_TRAILER ? 0 : -11;
}

static int rom_sum(int len)
{
	unsigned char	*p;
	int		sum;

	rom_load();
	p = rom_image;
	sum = 0;
	while(len--)
		sum += *p++;
	return (unsigned short)sum;
}

/* 0x801169f4: the update ROM's header, trailer and checksum */
int rom_verify(void)
{
	upd_header_t	hdr;
	unsigned char	trailer[4];
	unsigned short	sum;
	int		r;

	r = rom_read_header(&hdr);
	if(r)
		return r;
	r = rom_read_trailer(hdr.size, trailer);
	if(r)
		return r;
	sum = rom_sum(hdr.size);
	if(sum != hdr.sum || (unsigned short)~sum != hdr.nsum)
		r = -13;
	return r;
}

static int file_read_header(int fd, upd_header_t *hdr)
{
	int	pos, r, r2;

	r = file_tell(fd, &pos);
	if(r)
		return r;
	r = file_seek(fd, 0, SEEK_SET);
	if(r)
		return r;
	r = file_read(fd, hdr, sizeof(*hdr));
	if(!r)
		r = hdr->magic == UPD_MAGIC ? 0 : -11;
	r2 = file_seek(fd, pos, SEEK_SET);
	if(r)
		return r;
	return r2;
}

static int file_read_trailer(int fd, int size, unsigned char *trailer)
{
	int	pos, r, r2;

	r = file_tell(fd, &pos);
	if(r)
		return r;
	r = file_seek(fd, size - 4, SEEK_SET);
	if(r)
		return r;
	r = file_read(fd, trailer, 4);
	if(!r)
		r = (*(int *)trailer & 0xffffff00) == UPD_TRAILER ? 0 : -11;
	r2 = file_seek(fd, pos, SEEK_SET);
	if(r)
		return r;
	return r2;
}

/* 0x80116c48: an update ROM image in the open file fd, or in the named file */
int rom_file_verify(int fd, char *name)
{
	upd_header_t	hdr;
	unsigned char	trailer[4];
	unsigned short	sum;
	int		opened, pos, size, len;
	int		r, r2;

	if(name)
	{
		opened = 1;
		r = file_open(name, &fd);
	}
	else
	{
		opened = 0;
		r = 0;
	}
	if(r)
		return r;
	r = file_read_header(fd, &hdr);
	if(!r)
		r = file_read_trailer(fd, hdr.size, trailer);
	if(!r)
	{
		r = file_tell(fd, &pos);
		if(!r)
		{
			r = file_seek(fd, 0, SEEK_SET);
			if(!r)
			{
				sum = 0;
				size = hdr.size;
				while(size > 0)
				{
					len = size > sizeof(buf) ? sizeof(buf) : size;
					size -= len;
					r = file_read(fd, buf, len);
					if(r)
						break;
					sum += checksum(buf, len);
				}
				if(!r && (sum != hdr.sum || (unsigned short)~sum != hdr.nsum))
					r = -13;
				r2 = file_seek(fd, pos, SEEK_SET);
				if(!r)
					r = r2;
			}
		}
	}
	if(opened)
		file_close(fd);
	return r;
}

/* 0x80116ec8: GAMEINF.REV: the game and revision, then its text */
int rev_info_read(rev_t *rev, char *desc)
{
	int	fd, r, r2;

	r = file_open("GAMEINF.REV", &fd);
	if(!r)
	{
		r = file_read(fd, rev, sizeof(*rev));
		if(!r)
			r = file_read(fd, desc, 28);
		r2 = file_close(fd);
		if(!r)
			r = r2;
	}
	else
	{
		graphical_printf(msg, "CAN NOT OPEN REV.INF");
		update_message(msg);
	}
	return r;
}

/* 0x80116ff8: a new GAMEINF.REV, the old one kept as GAMEINF.OLD until it is written */
int rev_info_write(rev_t *rev, char *desc)
{
	int	fd, r, r2;

	if(file_exists("GAMEINF.OLD"))
		file_delete("GAMEINF.OLD");
	r = file_move("GAMEINF.REV", "GAMEINF.OLD");
	if(!r)
	{
		r = file_create("GAMEINF.REV", &fd);
		if(!r)
		{
			r = file_write(fd, rev, sizeof(*rev));
			if(!r)
				r = file_write(fd, desc, 28);
			r2 = file_close(fd);
			if(!r)
				r = r2;
		}
		if(!r)
			file_delete("GAMEINF.OLD");
		else
		{
			file_delete("GAMEINF.REV");
			file_move("GAMEINF.OLD", "GAMEINF.REV");
		}
	}
	return r;
}

/* 0x8011710c */
int rom_file_saved(int rev, int rom)
{
	return file_exists(rom_file_name(rev, rom));
}

/* 0x80117134: how many of the ROMs are not on the disk yet, and the first of them */
void roms_missing(int rev, int roms, int *count, int *next)
{
	int	i;

	*count = 0;
	*next = 0;
	for(i = roms; i > 0; i--)
	{
		if(!rom_file_saved(rev, i))
		{
			(*count)++;
			*next = i;
		}
	}
}

/* 0x801171c0: the update ROM to its file */
int rom_save(int rev, int rom, int size)
{
	char	*name;
	int	fd, offset, len;
	int	r, r2;

	name = rom_file_name(rev, rom);
	r = file_create(name, &fd);
	if(!r)
	{
		offset = 0;
		while(size > 0)
		{
			len = size > sizeof(buf) ? sizeof(buf) : size;
			size -= len;
			r = rom_read(offset, buf, len);
			offset += len;
			if(r)
				break;
			r = file_write(fd, buf, len);
			if(r)
				break;
		}
		r2 = file_close(fd);
		if(!r)
			r = r2;
		if(r)
			file_delete(name);
	}
	return r;
}

/* 0x80117348: check the saved ROMs, join their data into UPD.DAT and delete them */
int update_assemble(void)
{
	upd_header_t	hdr, fhdr;
	int		out, in, size, len;
	int		i, r, r2;

	r = rom_read_header(&hdr);
	if(!r)
	{
		for(i = 1; i <= hdr.roms; i++)
		{
			r = rom_file_verify(-1, rom_file_name(hdr.to.rev, i));
			if(r)
				break;
		}
		if(!r)
		{
			r = file_create("UPD.DAT", &out);
			if(!r)
			{
				for(i = 1; i <= hdr.roms; i++)
				{
					r = file_open(rom_file_name(hdr.to.rev, i), &in);
					if(!r)
					{
						r = file_read_header(in, &fhdr);
						if(!r)
						{
							size = fhdr.size - sizeof(fhdr) - 4;
							r = file_seek(in, sizeof(fhdr), SEEK_SET);
							if(!r)
							{
								while(size > 0)
								{
									len = size > sizeof(buf) ? sizeof(buf) : size;
									size -= len;
									r = file_read(in, buf, len);
									if(r)
										break;
									r = file_write(out, buf, len);
									if(r)
										break;
								}
							}
						}
						r2 = file_close(in);
						if(!r)
							r = r2;
					}
				}
				r2 = file_close(out);
				if(!r)
					r = r2;
				if(!r)
				{
					for(i = 1; i <= hdr.roms; i++)
						r = file_delete(rom_file_name(hdr.to.rev, i));
				}
			}
		}
		else if(r == -13 && i <= hdr.roms)
		{
			file_delete(rom_file_name(hdr.to.rev, i));
			graphical_printf(msg, "Please reload ROM number %d", i);
			update_message(msg);
		}
	}
	return r;
}

/* a string of the script: its length, then it padded to a word */
static int script_read_string(gzFile fp, char *s)
{
	int	len, r;

	r = zfile_read(fp, &len, 4);
	if(!r)
		r = len < 256 ? 0 : -27;
	if(!r)
		r = zfile_read(fp, s, (len + 3) & ~3);
	if(!r)
		s[len] = 0;
	else
		s[0] = 0;
	return r;
}

/* 0x8011773c: a file from the script: its name, size and data */
int script_extract(gzFile fp)
{
	char	name[256];
	int	size, fd, len;
	int	r, r2;

	r = script_read_string(fp, name);
	if(!r)
	{
		r = zfile_read(fp, &size, 4);
		size = (size + 3) & ~3;
		if(!r)
		{
			r = file_create(name, &fd);
			if(!r)
			{
				while(size > 0)
				{
					len = size > sizeof(buf) ? sizeof(buf) : size;
					size -= len;
					r = zfile_read(fp, buf, len);
					if(r)
						break;
					r = file_write(fd, buf, len);
					if(r)
						break;
				}
				r2 = file_close(fd);
				if(!r)
					r = r2;
			}
		}
	}
	return r;
}

/* 0x801178bc */
int script_move(gzFile fp)
{
	char	from[256], to[256];
	int	r;

	r = script_read_string(fp, from);
	if(r)
		return r;
	r = script_read_string(fp, to);
	if(r)
		return r;
	return file_move(from, to);
}

/* 0x801179e0 */
int script_copy(gzFile fp)
{
	char	from[256], to[256];
	int	r;

	r = script_read_string(fp, from);
	if(r)
		return r;
	r = script_read_string(fp, to);
	if(r)
		return r;
	return file_copy(from, to);
}

/* a piece of the patched file from its old copy: where, and how long */
static int patch_copy(gzFile fp, int from, int to)
{
	int	offset, count, len, r;

	r = zfile_read(fp, &offset, 4);
	if(!r)
	{
		r = zfile_read(fp, &count, 4);
		if(!r)
		{
			r = file_seek(from, offset, SEEK_SET);
			if(!r)
			{
				while(count > 0)
				{
					len = count > sizeof(buf) ? sizeof(buf) : count;
					count -= len;
					r = file_read(from, buf, len);
					if(r)
						break;
					r = file_write(to, buf, len);
					if(r)
						break;
				}
			}
		}
	}
	return r;
}

/* a piece of the patched file from the script: how long, then the data */
static int patch_insert(gzFile fp, int to)
{
	int	count, len, r;

	r = zfile_read(fp, &count, 4);
	if(!r)
	{
		while(count > 0)
		{
			len = count > sizeof(buf) ? sizeof(buf) : count;
			count -= len;
			r = zfile_read(fp, buf, len);
			if(r)
				break;
			r = file_write(to, buf, len);
			if(r)
				break;
		}
	}
	return r;
}

/* 0x80117b04: rebuild a file from its old copy and the script's pieces */
int script_patch(gzFile fp)
{
	char	name[256], backup[256];
	int	old, new, cmd, done;
	int	r, r2;

	r = script_read_string(fp, name);
	if(!r)
	{
		r = script_read_string(fp, backup);
		if(!r)
		{
			r = file_move(name, backup);
			if(!r)
			{
				r = file_open(backup, &old);
				if(!r)
				{
					r = file_create(name, &new);
					if(!r)
					{
						done = 0;
						while(!done)
						{
							r = zfile_read(fp, &cmd, 4);
							if(!r)
							{
								switch(cmd)
								{
								case 0:
									r = patch_copy(fp, old, new);
									break;
								case 1:
									r = patch_insert(fp, new);
									break;
								case 2:
									done = 1;
									break;
								default:
									r = -29;
									break;
								}
							}
							if(r)
								done = 1;
						}
						r2 = file_close(old);
						if(!r)
							r = r2;
						r2 = file_close(new);
						if(!r)
							r = r2;
						if(!r)
							file_delete(backup);
						else
						{
							file_delete(name);
							file_move(backup, name);
						}
					}
				}
			}
		}
	}
	return r;
}

/* run a program, with the four arguments that follow its name */
static int script_exec(gzFile fp)
{
	char	path[256];
	int	args[4];
	int	i, r;

	r = script_read_string(fp, path);
	if(r)
		return r;
	for(i = 0; i < 4; i++)
	{
		r = zfile_read(fp, &args[i], 4);
		if(r)
			return r;
	}
	exec(path, 0x800c0000, args);			/* where the game loads */
	return 0;
}

static int script_delete(gzFile fp)
{
	char	name[256];

	if(!script_read_string(fp, name))
		file_delete(name);
	return 0;
}

/* 0x80117e60: run UPD.DAT's commands (0 run, 1 extract, 2 delete, 3 move, 4 patch, 5 end,
   6 copy), blinking UPDATING meanwhile */
int update_run_script(void)
{
	gzFile	fp;
	int	cmd, color, done;
	int	r, r2;

	color = 0xffffff00;
	set_text_font(1);
	r = zfile_open("UPD.DAT", &fp);
	if(!r)
	{
		done = 0;
		while(!done)
		{
			if(color == 0xffffff00)
				color = 0xffff0000;
			else
				color = 0xffffff00;
			set_text_color(color);
			delete_string_id(1);
			set_string_id(1);
			set_text_position((int)(hres / 2.0F), (int)(vres * 3.0F / 8.0F), 1.0F);
			oprintf("UPDATING, DO NOT POWER DOWN...");
			sleep(90);
			r = zfile_read(fp, &cmd, 4);
			if(!r)
			{
				switch(cmd)
				{
				case 0:
					r = script_exec(fp);
					break;
				case 1:
					r = script_extract(fp);
					break;
				case 2:
					r = script_delete(fp);
					break;
				case 3:
					r = script_move(fp);
					break;
				case 4:
					r = script_patch(fp);
					break;
				case 6:
					r = script_copy(fp);
					break;
				case 5:
					done = 1;
					break;
				default:
					r = -28;
					break;
				}
			}
			if(r)
				done = 1;
		}
		r2 = zfile_close(fp);
		if(!r)
			r = r2;
		delete_string_id(1);
		sleep(1);
	}
	return r;
}
