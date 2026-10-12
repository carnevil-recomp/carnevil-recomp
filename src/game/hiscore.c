/*
 * hiscore.c - the high score table in CMOS adjustments 0x3e-0x47 (scores) and 0x48-0x51
 * (packed initials): 0x800e4eac reset_high_scores, 0x800e5014 load_high_scores,
 * 0x800e5114 write_scores, 0x800e6208 file_copy.
 */
#include <stdio.h>
#include <stdlib.h>

extern int	get_adjustment(int, int *), set_adjustment(int, int);
extern void	graphical_printf(char *, char *, ...);

typedef struct high_score
{
	char	name[4];
	int	score;
} high_score_t;

high_score_t	high_scores[10];			/* 0x8022b198 */

static high_score_t default_scores[10] =	/* 0x8017de08 */
{
	{ "JCK", 300000 }, { "SAM", 270000 }, { "DIO", 240000 }, { "SLC", 210000 }, { "MRF", 180000 },
	{ "MRT", 150000 }, { "ROW", 120000 }, { "JAS", 90000 }, { "KEV", 60000 }, { "MJL", 30000 },
};

/* 0x800e6208: copy a partition file (the default high-score photos over the current ones) */
void file_copy(char *readfile, char *writefile)
{
	FILE	*in, *out;
	char	buf[4096];
	size_t	n;

	in = fopen(readfile, "rb");
	if(!in)
	{
		printf("file_copy(): cannot open %s\n", readfile);
		return;
	}
	out = fopen(writefile, "wb");
	if(!out)
	{
		printf("file_copy(): cannot create %s\n", writefile);
		fclose(in);
		return;
	}
	while((n = fread(buf, 1, sizeof buf, in)) > 0)
	{
		fwrite(buf, 1, n, out);
	}
	fclose(out);
	fclose(in);
}

/* 0x800e5114 */
int write_scores(void)
{
	int	i;
	int	packed;

	for(i = 0; i < 10; i++)
	{
		packed = ((high_scores[i].name[3] & 0xff) << 24) | ((high_scores[i].name[0] & 0xff) << 16) | ((high_scores[i].name[1] & 0xff) << 8) | (high_scores[i].name[2] & 0xff);
		set_adjustment(0x48 + i, packed);
		set_adjustment(0x3e + i, high_scores[i].score);
	}
	return(0);
}

/* 0x800e5014 */
void load_high_scores(void)
{
	int	i;
	int	packed, score;

	for(i = 0; i < 10; i++)
	{
		get_adjustment(0x48 + i, &packed);
		get_adjustment(0x3e + i, &score);
		high_scores[i].name[0] = (packed & 0xff0000) >> 16;
		high_scores[i].name[1] = (packed & 0xff00) >> 8;
		high_scores[i].name[2] = packed & 0xff;
		high_scores[i].name[3] = 0;
		high_scores[i].score = score;
	}
}

/* 0x800e4eac */
int reset_high_scores(void)
{
	int	i;
	char	src[16], dst[16];

	printf("reseting high scores\n");
	for(i = 0; i < 10; i++)
	{
		high_scores[i].name[0] = default_scores[i].name[0];
		high_scores[i].name[1] = default_scores[i].name[1];
		high_scores[i].name[2] = default_scores[i].name[2];
		high_scores[i].name[3] = 0;
		high_scores[i].score = default_scores[i].score;
		graphical_printf(src, "HSTDDF%02d.WMS", i + 1);
		graphical_printf(dst, "HSTD%02d.WMS", i + 1);
		file_copy(src, dst);
	}
	return(write_scores());
}
