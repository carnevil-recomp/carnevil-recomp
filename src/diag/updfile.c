/*
 * updfile.c - the software update's file library (0x80118198-0x801188c4): disk files through
 * open/read/write/lseek and gzip-compressed files through zlib 1.0.4's gzio, each call
 * returning 0 or a negative error code.
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

typedef void	*gzFile;
extern gzFile	gzopen(const char *, const char *);	
extern int	gzread(gzFile, void *, unsigned);	/* gzread */
extern int	gzwrite(gzFile, const void *, unsigned);	/* gzwrite */
extern int	gzclose(gzFile);				/* gzclose */
extern long	lseek(int, long, int);			
extern int	remove(const char *);			/* calls _remove */
extern int	graphical_printf(char *, char *, ...);

static char		rom_name[16];			/* 0x802935b0 */
static unsigned char	copy_buf[0x20000];		/* 0x802935c0 */

int file_exists(char *);
int file_delete(char *);
int file_copy(char *, char *);
int file_read(int, void *, int);
int file_write(int, void *, int);
int file_size(int, char *, int *);

/* 0x80118198 */
int file_open(char *name, int *fd)
{
	*fd = open(name, O_RDONLY | O_BINARY, S_IRUSR | S_IWUSR);
	return *fd >= 0 ? 0 : -1;
}

/* 0x801181dc: a new, empty file */
int file_create(char *name, int *fd)
{
	int	r;

	if(file_exists(name) && (r = file_delete(name)) != 0)
		return r;
	*fd = open(name, O_RDWR | O_BINARY | O_CREAT, S_IRUSR | S_IWUSR);
	return *fd >= 0 ? 0 : -2;
}

/* 0x8011824c */
int file_close(int fd)
{
	if(close(fd))
		return -3;
	return 0;
}

/* 0x80118274 */
int file_exists(char *name)
{
	int	fd, found;

	if(!file_open(name, &fd))
	{
		found = 1;
		file_close(fd);
	}
	else
		found = 0;
	return found;
}

/* 0x801182d0 */
int file_delete(char *name)
{
	if(remove(name))
		return -4;
	return 0;
}

/* 0x801182f8 */
int file_move(char *from, char *to)
{
	int	r;

	r = file_copy(from, to);
	if(!r)
		file_delete(from);
	return r;
}

/* 0x80118340 */
int file_copy(char *from, char *to)
{
	int	in, out, size, len;
	int	r;

	r = file_open(from, &in);
	if(r)
		return r;
	r = file_create(to, &out);
	if(!r)
	{
		r = file_size(in, 0, &size);
		if(!r)
		{
			while(size > 0)
			{
				len = size > sizeof(copy_buf) ? sizeof(copy_buf) : size;
				size -= len;
				r = file_read(in, copy_buf, len);
				if(r)
					break;
				r = file_write(out, copy_buf, len);
				if(r)
					break;
			}
		}
		file_close(out);
	}
	file_close(in);
	return r;
}

/* 0x80118494 */
int file_read(int fd, void *buf, int len)
{
	int	got, r;

	if(!len)
		return 0;
	got = read(fd, buf, len);
	if(got == len)
		r = 0;
	else
		r = !got ? -6 : -7;
	return r;
}

/* 0x801184ec */
int file_write(int fd, void *buf, int len)
{
	int	got, r;

	if(!len)
		return 0;
	got = write(fd, buf, len);
	if(got == len)
		r = 0;
	else
		r = !got ? -8 : -9;
	return r;
}

/* 0x80118544 */
int file_seek(int fd, int offset, int whence)
{
	return lseek(fd, offset, whence) >= 0 ? 0 : -10;
}

/* 0x80118574 */
int file_tell(int fd, int *pos)
{
	*pos = lseek(fd, 0, SEEK_CUR);
	return *pos >= 0 ? 0 : -10;
}

/* 0x801185bc: the size of the open file fd, or of the named file */
int file_size(int fd, char *name, int *size)
{
	int	opened, pos, r, r2;

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
	r = file_tell(fd, &pos);
	if(!r)
	{
		r = file_seek(fd, 0, SEEK_END);
		if(!r)
		{
			r = file_tell(fd, size);
			r2 = file_seek(fd, pos, SEEK_SET);
			if(!r)
				r = r2;
		}
	}
	if(opened)
		file_close(fd);
	return r;
}

/* 0x801186e8 */
int zfile_open(char *name, gzFile *fp)
{
	*fp = gzopen(name, "rb");
	if(!*fp)
		return -21;
	return 0;
}

/* 0x8011872c */
int zfile_create(char *name, gzFile *fp)
{
	*fp = gzopen(name, "wb9");
	if(!*fp)
		return -22;
	return 0;
}

/* 0x80118770 */
int zfile_close(gzFile fp)
{
	if(gzclose(fp))
		return -23;
	return 0;
}

/* 0x80118798: -26 at the end of the file */
int zfile_read(gzFile fp, void *buf, int len)
{
	int	got, r;

	if(!len)
		return 0;
	got = gzread(fp, buf, len);
	if(got <= 0)
		r = got < 0 ? -24 : -26;
	else
		r = got == len ? 0 : -24;
	return r;
}

/* 0x80118800 */
int zfile_write(gzFile fp, void *buf, int len)
{
	if(!len)
		return 0;
	if(gzwrite(fp, buf, len) != len)
		return -25;
	return 0;
}

/* 0x80118848: the 16-bit sum of the bytes */
unsigned short checksum(unsigned char *p, int len)
{
	int	sum;

	sum = 0;
	while(len--)
		sum += *p++;
	return sum;
}

/* 0x80118880: the file a ROM of an update is kept in */
char *rom_file_name(unsigned char rev, unsigned char rom)
{
	graphical_printf(rom_name, "UPL%02XN%02X.ROM", rev, rom);
	return rom_name;
}
