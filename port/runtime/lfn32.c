/*
 * lfn32.c - Midway's LFN32 flat file system (Joe Linhoff's FILESYS.C in the Seattle BIOS), read
 * straight from the hard-disk image, so the port can unpack the game's files itself on first run
 * instead of needing tools/rom/lfn32.py. The same layout as that script, in 512-byte sectors from
 * the partition start:
 *   sector 2   info: magic 0x34011280, sectors per cluster (8), first FAT sector, root directory
 *              sector, FAT type (16)
 *   FAT        16-bit entries; entry = next cluster, 0 = end; FAT entry 0 belongs to cluster 3
 *   cluster c  at sector root + (c - 3) * 8 (4 KB); cluster 3 is the root directory
 *   directory  24-byte entries {u32 name[3] big-endian ASCII, u32 size in words, u32 date,
 *              u32 flags.8:0.8:cluster.16}; name[0] == 0 is deleted; the cluster's last word
 *              links the next directory cluster. There are no subdirectories.
 * Paths given here must be absolute: the HAL passes absolute paths through untouched.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#define SECTOR		512u
#define SPC		8u
#define CLUSTER		(SECTOR * SPC)
#define LFN32_MAGIC	0x34011280u

static uint32_t le32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

static int read_at(FILE *f, unsigned long long off, void *buf, size_t n)
{
	if(fseeko(f, (off_t)off, SEEK_SET)) return -1;
	return fread(buf, 1, n, f) == n ? 0 : -1;
}

/* unpack every file of the LFN32 partition 'index' of the disk image into outdir; returns the
   number of files written, or -1 */
int lfn32_extract(const char *image, int index, const char *outdir)
{
	unsigned char	sec[SECTOR], *fat = 0, *clu = 0, *data = 0;
	unsigned long long base;
	uint32_t	fatsector, rootsector, nfat, c, seen_guard;
	int		files = 0;
	FILE		*f = fopen(image, "rb");

	if(!f) return -1;
	if(read_at(f, 0, sec, SECTOR) || memcmp(sec, "TRAP", 4)) goto fail;	/* the BIOS partition table */
	if((uint32_t)index >= le32(sec + 4)) goto fail;
	base = (unsigned long long)le32(sec + 8 + 12 * index) * SECTOR;
	if(read_at(f, base + 2 * SECTOR, sec, SECTOR)) goto fail;
	if(le32(sec) != LFN32_MAGIC || le32(sec + 4) != SPC || le32(sec + 16) != 16)
	{
		fprintf(stderr, "[lfn32] %s partition %d is not an LFN32 file system\n", image, index);
		goto fail;
	}
	fatsector = le32(sec + 8); rootsector = le32(sec + 12);
	nfat = rootsector - fatsector;
	fat = malloc((size_t)nfat * SECTOR); clu = malloc(CLUSTER);
	if(!fat || !clu || read_at(f, base + (unsigned long long)fatsector * SECTOR, fat, (size_t)nfat * SECTOR)) goto fail;
	mkdir(outdir, 0755);
#define CLUSTER_OFF(cl)	(base + ((unsigned long long)rootsector + ((cl) - 3u) * SPC) * SECTOR)
#define NEXT(cl)	((uint32_t)(fat[2 * ((cl) - 3u)] | (fat[2 * ((cl) - 3u) + 1] << 8)))
	for(c = 3, seen_guard = 0; c && seen_guard < 4096; seen_guard++)		/* the directory's clusters */
	{
		unsigned char	dir[CLUSTER];
		unsigned	i;
		if(read_at(f, CLUSTER_OFF(c), dir, CLUSTER)) goto fail;
		for(i = 0; i < (CLUSTER - 4) / 24; i++)
		{
			const unsigned char *e = dir + i * 24;
			char		name[13], path[2048];
			uint32_t	size, fc, left, k, guard;
			FILE		*o;
			int		j;
			if(!le32(e)) continue;						/* deleted */
			for(j = 0; j < 12; j++) name[j] = (char)e[(j & ~3) + 3 - (j & 3)];	/* big-endian characters */
			name[12] = 0;
			size = le32(e + 12) * 4; fc = le32(e + 20) & 0xffff;
			snprintf(path, sizeof path, "%s/%s", outdir, name);
			if(!(o = fopen(path, "wb"))) goto fail;
			for(k = fc, left = size, guard = 0; k && left && guard < 65536; guard++, k = NEXT(k))
			{
				uint32_t n = left < CLUSTER ? left : CLUSTER;
				if(read_at(f, CLUSTER_OFF(k), clu, CLUSTER) || fwrite(clu, 1, n, o) != n) { fclose(o); goto fail; }
				left -= n;
			}
			fclose(o);
			files++;
		}
		c = le32(dir + CLUSTER - 4);
	}
	free(fat); free(clu); free(data); fclose(f);
	return files;
fail:
	free(fat); free(clu); free(data); fclose(f);
	return -1;
}
