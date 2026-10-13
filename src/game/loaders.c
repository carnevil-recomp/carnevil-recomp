/*
 * loaders.c - the level's data files (0x800ca4d0-0x800cb5d4): camera, animation, model and
 * path files, each read whole into memory and checked against a CRC, with retries and a
 * backup copy. Only the camera loader is in C so far.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int	open(const char *, int, ...);
extern int	read(int, void *, int);
extern int	close(int);
extern unsigned int crc(void *, int);			/* GOOSE crc.c */
extern int	increment_audit(int);
extern void	load_backup_file(char *);		/* 0x800caf48 */

int	camera_loads, camera_crc_errors;		/* 0x80179be0, 0x80179be4 */

/* 0x800ca4d0: count 28-byte entries of a camera file into *out: 0 when loaded, 1 when out
   of memory, 2 when the file never read correctly */
int load_camera_file(void **out, int count, char *name, int given_crc)
{
	char	path[32];
	int	tries, backup_tries, done, backup;
	int	size, fd, n;
	unsigned int sum;
	void	*buf;

	tries = 5;
	backup_tries = 5;
	done = 0;
	backup = 0;
	size = count * 28;
	camera_loads++;
	buf = malloc(size);
	if(!buf)
	{
		printf("Could not load camera file %s. Not enought memory\n", path);
		return(1);
	}
	strcpy(path, name);
	if(backup_tries > 0)
	{
		do
		{
			fd = open(path, 4);
			n = read(fd, buf, size);
			close(fd);
			if(n != size)
			{
				printf("ERROR - load_camera %s. File size doesn't match.\r\n", path);
				printf(" Read %d bytes. Wanted %d bytes.\n", n, size);
				increment_audit(0x31);
				if(backup == 0) tries--; else backup_tries--;
			}
			else if((sum = crc(buf, size)) != given_crc)
			{
				printf("Bad crc on file %s. Wanted 0x08%X, got 0x08%X\n", path, given_crc, sum);
				if(backup == 0) tries--; else backup_tries--;
				camera_crc_errors++;
				increment_audit(0x30);
			}
			else
			{
				done = 1;
			}
			if(done)
			{
				break;
			}
			if(tries == 0 && backup == 0)
			{
				tries = 1;
				backup = 1;
				load_backup_file(path);
				printf("trying backup %s\n", path);
			}
		} while(tries > 0);
	}
	if(!done)
	{
		free(buf);
		return(2);
	}
	*out = buf;
	return(0);
}
