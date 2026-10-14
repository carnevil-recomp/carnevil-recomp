/*
 * loaders.c - the level's data files (0x800ca4d0-0x800cb5d4): camera, animation, model and
 * path files, each read whole into memory and checked against a CRC, with retries and a
 * backup copy.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model.h"

extern int	open(const char *, int, ...);
extern int	read(int, void *, int);
extern int	close(int);
extern unsigned int crc(void *, int);			/* GOOSE crc.c */
extern int	increment_audit(int);
extern void	load_backup_file(char *);		/* 0x800caf48 */
extern int	get_free_mem_left(void);

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

/* 0x800ca6d4: an animation file: nframes frames of 24 bytes for each of nsub submeshes, in
   submesh order; each submesh gets its own copy (sub[i].anim). 0 when loaded, 1 when out of
   memory, 2 when the file never read correctly */
int load_anim(submesh_t *sub, int nsub, int nframes, char *name, int given_crc)
{
	char	path[32];
	int	tries, backup_tries, done, backup;
	int	frame_bytes, size, fd, n, i;
	unsigned int sum;
	char	*buf, *p;

	tries = 5;
	backup_tries = 5;
	done = 0;
	frame_bytes = nframes * 24;
	camera_loads++;
	size = frame_bytes * nsub;
	buf = malloc(size);
	backup = 0;
	if(!buf)
	{
		printf("Could not load animation file %s. Not enought memory\n", name);
		printf("Wanted %d bytes, %d available\n", size, get_free_mem_left());
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
				printf("ERROR - load_animation %s. File size doesn't match.\r\n", path);
				printf(" Read %d bytes. Wanted %d bytes. %d frames, %d groups\r\n", n, size, nframes, nsub);
				increment_audit(0x31);
				if(backup == 0) tries--; else backup_tries--;
			}
			else if((sum = crc(buf, size)) != given_crc)
			{
				printf("Bad crc on file %s. Wanted 0x08%X, got 0x08%X\n", path, given_crc, sum);
				if(backup == 0) tries--; else backup_tries--;
				camera_crc_errors++;
				increment_audit(0x2d);
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
	p = buf;
	for(i = 0; i < nsub; i++)
	{
		sub[i].anim = malloc(frame_bytes);
		if(!sub[i].anim)
		{
			printf("Ran out of memory in load_animation\n");
			free(buf);
			return(1);
		}
		memcpy(sub[i].anim, p, frame_bytes);
		p += frame_bytes;
	}
	free(buf);
	return(0);
}

/* 0x800ca97c: a model file: the vertices (12 bytes each), the normals (12) and the faces
   (40), each copied into its own block */
int load_model(void **verts, int nverts, void **faces, int nfaces, void **normals, int nnormals, char *name, int given_crc)
{
	char	path[32];
	int	tries, backup_tries, done, backup;
	int	vbytes, nbytes, fbytes, size, fd, n;
	unsigned int sum;
	char	*buf, *p;

	tries = 5;
	backup_tries = 5;
	done = 0;
	backup = 0;
	vbytes = nverts * 12;
	nbytes = nnormals * 12;
	fbytes = nfaces * 40;
	size = vbytes + nbytes + fbytes;
	camera_loads++;
	buf = malloc(size);
	p = buf;
	if(!buf)
	{
		printf("Could not load model file %s. Not enought memory\n", path);
		return(1);
	}
	strcpy(path, name);
	if(backup_tries > 0)
	{
		do
		{
			fd = open(path, 4);
			n = read(fd, p, size);
			close(fd);
			if(n != size)
			{
				printf("ERROR - load_model %s. File size doesn't match.\r\n", path);
				printf(" Read %d bytes. Wanted %d bytes\n", n, size);
				increment_audit(0x31);
				if(backup == 0) tries--; else backup_tries--;
			}
			else if((sum = crc(p, size)) != given_crc)
			{
				printf("Bad crc on file %s. Wanted 0x08%X, got 0x08%X\n", path, given_crc, sum);
				if(backup == 0) tries--; else backup_tries--;
				camera_crc_errors++;
				increment_audit(0x2e);
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
		free(p);
		printf("No more retries! \n");
		return(2);
	}
	*verts = malloc(vbytes);
	if(*verts)
	{
		memcpy(*verts, p, vbytes);
		p += vbytes;
		*normals = malloc(nbytes);
		if(*normals)
		{
			memcpy(*normals, p, nbytes);
			p += nbytes;
			*faces = malloc(fbytes);
			if(*faces)
			{
				memcpy(*faces, p, fbytes);
				free(buf);
				return(0);
			}
		}
	}
	printf("Could not load model file %s. Not enought memory\n", path);
	free(buf);
	return(1);
}

/* 0x800cac64: a path file: count entries of 24 bytes into *out */
int load_path(void **out, int count, char *name, int given_crc)
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
	size = count * 24;
	camera_loads++;
	buf = malloc(size);
	if(!buf)
	{
		printf("Could not load path file %s. Not enought memory\n", path);
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
				printf("ERROR - load_path %s. File size doesn't match.\r\n", path);
				printf(" Read %d bytes. Wanted %d bytes.\n", n, size);
				increment_audit(0x31);
				if(backup == 0) tries--; else backup_tries--;
			}
			else if((sum = crc(buf, size)) != given_crc)
			{
				printf("Bad crc on file %s. Wanted 0x%08X, got 0x%08X\n", path, given_crc, sum);
				if(backup == 0) tries--; else backup_tries--;
				camera_crc_errors++;
				increment_audit(0x2f);
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

/* 0x800cae68: free each submesh's animation frames (the count and name are the loader's,
   unused here) */
void remove_animation(submesh_t *sub, int nsub, int nframes, char *name)
{
	int	i;

	for(i = 0; i < nsub; i++)
	{
		if(sub[i].anim)
		{
			free(sub[i].anim);
		}
		sub[i].anim = 0;
	}
}

/* 0x800caed0: free a model's faces, normals and vertices (the pointers are left as they are) */
void remove_model(void **verts, int nverts, void **faces, int nfaces, void **normals, int nnormals, char *name)
{
	free(*faces);
	free(*normals);
	free(*verts);
}

/* 0x800caf24 */
void remove_path(void **path, int count, char *name)
{
	free(*path);
}
