/*
 * adjust.c - the game's CMOS set-up, audit groups and adjustments (0x800f18e0-0x800f1fa8,
 * 0x800f2a28). Audits are cleared and checked in groups by id list; the game's own
 * adjustments are 0x30-0x3d (defaults in game_adjustments) and are read into globals by
 * load_game_adjustments.
 */
#include <stdio.h>
#include <goose/cmos.h>

extern int	get_audit(int, int *), set_audit(int, int);
extern int	get_adjustment(int, int *), set_adjustment(int, int);
#include <goose/adjust.h>
extern void	startaudit(FILE *);				/* 0x800ef870: the audit names for audits.fmt */
extern int	violence_level;					/* 0x801e5bfc */
extern int	stereo_output;					/* 0x801de954 */

int	difficulty, D_801de8a4, D_80237438;			/* difficulty-type adjustments 0x30-0x32 (0..4) */
int	D_801e5c10, D_801e70a0, D_801e6ed0, D_80237420, D_801e704c;
float	D_80212df4;						/* adjustment 0x37 / 100 */

/* the game's adjustments: number, min, max, default (0x8018ed48) */
static int	game_adjustments[14][4] =
{
	{ 0x30, 0, 4, 2 }, { 0x31, 0, 4, 2 }, { 0x32, 0, 4, 2 }, { 0x33, 0, 1, 1 }, { 0x34, 0, 3, 3 },
	{ 0x35, 0, 1, 1 }, { 0x36, 0, 1, 1 }, { 0x37, 30, 99, 50 }, { 0x38, 0, 1, 0 }, { 0x39, 0, 1, 0 },
	{ 0x3a, 0, 1, 1 }, { 0x3b, 0, 1, 1 }, { 0x3c, 0, 1, 1 }, { 0x3d, 0, 1, 1 },
};

/* 0x800f1fa8 */
void restore_game_adjustments(void)
{
	restore_factory_adjustments(14, (adjustment_info_t *)game_adjustments);
}

/* 0x800f1d88: the game's adjustments into their globals, with defaults when unreadable */
int load_game_adjustments(void)
{
	int	val;

	if(get_adjustment(0x30, &difficulty)) difficulty = 2;
	if((unsigned)difficulty >= 5) difficulty = 2;
	if(get_adjustment(0x31, &D_801de8a4)) D_801de8a4 = 2;
	if((unsigned)D_801de8a4 >= 5) D_801de8a4 = 2;
	if(get_adjustment(0x32, &D_80237438)) D_80237438 = 2;
	if((unsigned)D_80237438 >= 5) D_80237438 = 2;
	if(get_adjustment(0x35, &D_801e5c10)) D_801e5c10 = 1;
	if(get_adjustment(0x37, &val))
	{
		D_80212df4 = 0.6f;
	}
	else
	{
		D_80212df4 = (float)val / 100.0f;
	}
	if(get_adjustment(0x34, &violence_level)) violence_level = 4;
	if(get_adjustment(0x36, &D_801e70a0)) D_801e70a0 = 1;
	if(get_adjustment(0x3a, &D_801e6ed0)) D_801e6ed0 = 1;
	if(get_adjustment(0x3b, &D_80237420)) D_80237420 = 1;
	if(get_adjustment(0x3c, &stereo_output)) stereo_output = 1;
	if(get_adjustment(0x3d, &D_801e704c)) D_801e704c = 1;
	return(0);
}

/* 0x800f2a28: describe the CMOS layout in audits.fmt (for the diagnostics) and configure it */
int game_setup_cmos(void)
{
	cmos_config_info_t cfg;
	FILE	*f;
	int	i;

	cfg.cmos_size = 0x1f40;
	cfg.max_audits = 0xb6;
	cfg.max_adjustments = 0x6e;
	cfg.num_gcr_tables = 1;
	cfg.user_data_size = 0;
	cfg.gcr[0].entry_size = 0x38;
	cfg.gcr[0].num_entries = 1;
	cfg.gcr[1].entry_size = 0xc;
	cfg.gcr[1].num_entries = 1;
	f = fopen("audits.fmt", "wb");
	if(!f)
	{
		fprintf(stderr, "Can not open file AUDITS.FMT\r\n");
	}
	if(f)
	{
		fprintf(f, "cmos size %d audits %d adjustments %d gcrtables %d cmosdone\r\n", cfg.cmos_size, cfg.max_audits, cfg.max_adjustments, cfg.num_gcr_tables);
		for(i = 0; i < cfg.num_gcr_tables; i++)
		{
			fprintf(f, "cmosgcr entrysize %d numentries %d cmosgcrdone\r\n", cfg.gcr[i].entry_size, cfg.gcr[i].num_entries);
		}
		fprintf(f, "\r\n");
		startaudit(f);
		fflush(f);
		fclose(f);
	}
	return(config_cmos(&cfg));
}

/* 0x800f24c8: fold a value into a running average kept in two audits: the count and the mean */
void average_audit(int count_audit, int mean_audit, int value)
{
	int	count, mean;

	get_audit(count_audit, &count);
	get_audit(mean_audit, &mean);
	mean = (mean * count + value) / (count + 1);
	count = count + 1;
	set_audit(count_audit, count);
	set_audit(mean_audit, mean);
}
