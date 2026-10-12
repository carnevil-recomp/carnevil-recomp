/*
 * disk.c - the BIOS raw-disk calls the movie player uses, served from the disk image
 * (--raw IMAGE / CARNEVIL_RAW). Reads are synchronous: the completion callback the game
 * installs with install_disk_callback() runs before _SecReads() returns.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hal.h"

#define SECTOR		512
#define MAX_PARTITIONS	40

static FILE	*raw;
static int	raw_tried;
static int	part_table[2 + 3 * MAX_PARTITIONS];	/* partition_table_t: magic, count, {start, num, type}... */
static void	(*disk_callback)(int);

static void raw_open(void)
{
	unsigned char	sec[SECTOR];
	int		n, i;
	if(raw_tried) return;
	raw_tried = 1;
	if(!hal_raw_image) hal_raw_image = "original/carn.raw";
	raw = fopen(hal_raw_image, "rb");
	if(!raw) { fprintf(stderr, "[disk] no disk image at %s (--raw IMAGE): movies will be blank\n", hal_raw_image); return; }
	if(fread(sec, 1, SECTOR, raw) != SECTOR || memcmp(sec, "TRAP", 4)) { fprintf(stderr, "[disk] %s: no TRAP partition table\n", hal_raw_image); fclose(raw); raw = 0; return; }
	n = sec[4] | (sec[5] << 8) | (sec[6] << 16) | (sec[7] << 24);
	if(n > MAX_PARTITIONS) n = MAX_PARTITIONS;
	part_table[0] = 0x54524150;
	part_table[1] = n;
	for(i = 0; i < n * 3; i++)
		part_table[2 + i] = sec[8 + 4 * i] | (sec[9 + 4 * i] << 8) | (sec[10 + 4 * i] << 16) | (sec[11 + 4 * i] << 24);
	fprintf(stderr, "[disk] %s: %d partitions\n", hal_raw_image, n);
}

void *ide_get_partition_table(void) { raw_open(); return part_table; }

short _SecReads(unsigned int sector, void *buf, unsigned int num)
{
	int	err = 0;
	raw_open();
	if(!raw || fseeko(raw, (off_t)sector * SECTOR, SEEK_SET) || fread(buf, SECTOR, num, raw) != num)
	{
		memset(buf, 0, (size_t)num * SECTOR);
		err = raw ? 1 : 0;	/* no image at all: stay quiet, show black frames */
	}
	if(disk_callback) disk_callback(err);
	return err ? -1 : 0;
}

void install_disk_callback(void (*f)(int)) { disk_callback = f; }
void install_disk_info(void *p, void *sus, void *res) { (void)p; (void)sus; (void)res; }
int  func_8013ff24(void) { return 0; }		/* disk queue kick */
void func_8013fea4(void) { }			/* disk queue cleanup */
