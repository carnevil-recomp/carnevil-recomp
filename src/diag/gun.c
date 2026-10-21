/*
 * gun.c - the gun board reset, the game's audit average, float adjustments and CMOS layout,
 * and the player records (text 0x801189d8-0x801199ec, rodata 0x80159ab0-0x80159b70, data
 * 0x80167d98-0x80168084, bss tmp_record 0x802ef498). Built at -O (-O -mips4 -mcpu=r5000
 * -fsigned-char), not DIAG's usual -O2.
 *
 * The code is the game's: gun_board_reset is src/game/render/camera.c's, average_audit ..
 * game_setup_cmos are src/game/core/adjust.c's, and the record functions are the ones that
 * follow them in the game's adjust.c (0x800f2b6c-0x800f3580, not decompiled there yet), at
 * an older revision: get_float_adjustment reads the first slot three times and rewrites
 * through set_float_adjustment. Here only gun_board_reset, get_float_adjustment and
 * set_float_adjustment are called; the rest came along.
 *
 * The object goes on with the gun calibration and test (guntest.c, 0x801199ec-0x8011b4d4,
 * whose last function, at 0x8011b4cc, is an empty startaudit): no object can start at
 * 0x801199ec (objects are 8-aligned) and the data runs on unpadded at 0x80168084.
 *
 * A player record (generic CMOS record table 0, 0x38 bytes) is a name and PIN, an age, the
 * games played and won and the stats the record tables rank by; the comparison functions
 * are for qsort and put players with fewer than four games last. Its stats from 0x11 on are
 * unaligned (each such field packed): the compiler reads them as bit fields, a 32-bit one
 * with ulw.
 */
#include <stdio.h>
#include <stdlib.h>
#include <goose/cmos.h>

extern int	get_audit(int, int *), set_audit(int, int);
extern int	get_adjustment(int, int *), set_adjustment(int, int);
extern void	startaudit(FILE *);			/* 0x8011b4cc: empty here (the game writes the audit names) */

/* a message to stderr, as a statement (the do-while shows in the code: the compiler does not
   carry what it knows about f past it) */
#define ERROR(s)	do { fprintf(stderr, s); } while(0)

#define NUM_RECORDS	1
#define NUM_NAME_RECORDS	1
#define MIN_GAMES	4

typedef struct plyr_record
{
	char	name[6];		/* 0x00 */
	char	pin[4];			/* 0x06 */
	short	last_used;		/* 0x0a: games since it was used */
	short	games_played;		/* 0x0c */
	short	games_won;		/* 0x0e */
	char	x10;			/* 0x10 */
	short	x11 __attribute__((packed));	/* 0x11 */
	short	x13 __attribute__((packed));	/* 0x13 */
	int	x15 __attribute__((packed));	/* 0x15 */
	short	x19 __attribute__((packed));	/* 0x19 */
	short	x1b __attribute__((packed));	/* 0x1b */
	int	x1d __attribute__((packed));	/* 0x1d */
	int	x21 __attribute__((packed));	/* 0x21 */
	char	x25;			/* 0x25 */
	short	x26;			/* 0x26 */
	int	x28, x2c, x30, x34;	/* 0x28.. */
} plyr_record_t;

/* record table 1 (0xc bytes) */
typedef struct name_record
{
	char	name[6];
	char	pin[4];
	short	count;
} name_record_t;

int		set_float_adjustment(int, float);
void		restore_default_records(void);
void		restore_name_records(void);
void		clear_record(char *, int);
int		compare_record(char *, char *, int);

plyr_record_t	*ram_records = 0;			/* 0x80167d98 */

/* 0x80167d9c */
plyr_record_t	default_records[] =
{
	{ "TURMEL", "0322", 0, 5, 4, 3, 121, 194, 100, 50, 11, 300, 325, 0, 0, 55, 30, 9, 5 },
	{ "DIVITA", "1111", 0, 5, 3, 0, 121, 187, 1, 52, 12, 300, 350, 0, 0, 44, 32, 8, 5 },
	{ "JAPPLE", "2222", 0, 5, 4, 4, 126, 201, 138, 14, 170, 400, 0, 30, 0, 12, 10, 6, 0 },
	{ "JENIFR", "3333", 0, 5, 3, 1, 124, 217, 1, 42, 15, 275, 450, 0, 0, 52, 29, 11, 5 },
	{ "LUIS  ", "3333", 0, 5, 3, 1, 117, 209, 1, 40, 20, 295, 320, 0, 0, 45, 34, 9, 6 },
	{ "DANIEL", "3333", 0, 5, 3, 1, 114, 217, 1, 38, 12, 325, 270, 0, 0, 45, 32, 9, 6 },
	{ "JASON ", "3141", 0, 5, 4, 1, 110, 209, 1, 38, 14, 365, 335, 0, 0, 40, 24, 10, 5 },
	{ "MIKE  ", "3333", 0, 5, 3, 1, 118, 201, 1, 36, 12, 120, 351, 0, 0, 43, 23, 12, 6 },
	{ "J ROOT", "3333", 0, 5, 3, 1, 121, 204, 1, 34, 12, 360, 170, 0, 0, 46, 15, 13, 7 },
	{ "CARLTN", "1111", 0, 5, 3, 1, 116, 220, 1, 32, 12, 345, 115, 0, 0, 50, 19, 12, 8 },
	{ "GRINCH", "0222", 0, 5, 3, 1, 116, 220, 1, 30, 12, 345, 145, 0, 0, 35, 10, 5, 6 },
	{ "GUIDO ", "6765", 0, 5, 3, 0, 107, 219, 1, 34, 10, 300, 210, 0, 0, 40, 15, 10, 6 },
};

/* 0x8016803c */
name_record_t	default_name_records[] =
{
	{ "TURMEL", "0322", 2 },
	{ "DIVITA", "1111", 5 },
	{ "JAPPLE", "2222", 10 },
	{ "DANIEL", "3333", 2 },
	{ "JENIFR", "3333", 21 },
	{ "JASON ", "3141", 10 },
};

plyr_record_t	tmp_record;				/* 0x802ef498 */

/* 0x801189d8: reset the gun board (bit 4 of its control byte) */
int gun_board_reset(void)
{
	signed char	*control = (signed char *)0xb7f00000;
	char		v;

	v = *control;
	printf("NSS reset = %x\n", v);
	*control = v | 0x10;
	return(0);
}

/* 0x80118a24: fold a value into a running average kept in two audits: the count and the mean */
void average_audit(int count_audit, int mean_audit, int value)
{
	int	count, mean, total;

	get_audit(count_audit, &count);
	get_audit(mean_audit, &mean);
	total = mean * count + value;
	mean = total / ++count;
	set_audit(count_audit, count);
	set_audit(mean_audit, mean);
}

/* 0x80118ad4: a float adjustment is kept in three consecutive slots */
int set_float_adjustment(int adj, float value)
{
	int	v;

	v = *(int *)&value;
	set_adjustment(adj, v);
	set_adjustment(adj + 1, v);
	set_adjustment(adj + 2, v);
	return(0);
}

/* 0x80118b2c: read the three copies of a float adjustment (this revision reads the first slot
   three times): the value the readable copies agree on, else the default, rewritten unless
   they all agreed; the return code says which copies were bad or disagreed */
int get_float_adjustment(int adj, float *out, float def)
{
	float	a, b, c;
	int	ea, eb, ec;

	ea = get_adjustment(adj, (int *)&a);
	eb = get_adjustment(adj, (int *)&b);
	ec = get_adjustment(adj, (int *)&c);
	if(!ea && !eb && !ec)
	{
		if(a == b && b == c)
		{
			*out = a;
			return(0);
		}
		if(a == b)
		{
			*out = a;
			set_float_adjustment(adj, a);
			return(4);
		}
		if(a == c)
		{
			*out = a;
			set_float_adjustment(adj, a);
			return(2);
		}
		if(b == c)
		{
			*out = b;
			set_float_adjustment(adj, b);
			return(1);
		}
		set_float_adjustment(adj, def);
		*out = def;
		return(7);
	}
	if(!ea && !eb)
	{
		if(a == b)
		{
			*out = a;
			set_float_adjustment(adj, a);
			return(0x40);
		}
		set_float_adjustment(adj, def);
		*out = def;
		return(0x47);
	}
	if(!ea && !ec)
	{
		if(a == c)
		{
			*out = a;
			set_float_adjustment(adj, a);
			return(0x20);
		}
		set_float_adjustment(adj, def);
		*out = def;
		return(0x27);
	}
	if(!ec && !eb)
	{
		if(c == b)
		{
			*out = c;
			set_float_adjustment(adj, c);
			return(0x10);
		}
		set_float_adjustment(adj, def);
		*out = def;
		return(0x47);
	}
	if(!ea)
	{
		*out = a;
		set_float_adjustment(adj, a);
		return(0x60);
	}
	if(!eb)
	{
		*out = b;
		set_float_adjustment(adj, b);
		return(0x50);
	}
	if(!ec)
	{
		*out = c;
		set_float_adjustment(adj, c);
		return(0x30);
	}
	set_float_adjustment(adj, def);
	*out = def;
	return(0x70);
}

/* 0x80118ddc: describe the CMOS layout in audits.fmt and configure it */
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
		ERROR("Can not open file AUDITS.FMT\r\n");
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

/* 0x80118f24: clear the records that fail their check; when any did, the defaults go back */
int validate_records(void)
{
	int	bad;
	int	i;

	bad = 0;
	for(i = 0; i < NUM_RECORDS; i++)
	{
		if(get_generic_record(0, i, &tmp_record))
		{
			bad = 1;
			clear_record((char *)&tmp_record, sizeof(plyr_record_t));
			set_generic_record(0, i, &tmp_record);
		}
	}
	if(bad)
	{
		restore_default_records();
		restore_name_records();
	}
	return(bad);
}

/* 0x80118fc0 */
void restore_default_records(void)
{
	int	i;

	for(i = 0; i < sizeof(default_records) / sizeof(plyr_record_t); i++)
	{
		set_generic_record(0, i, &default_records[i]);
	}
	clear_record((char *)&tmp_record, sizeof(plyr_record_t));
	for(; i < NUM_RECORDS; i++)
	{
		set_generic_record(0, i, &tmp_record);
	}
}

/* 0x80119054 */
void restore_name_records(void)
{
	int	i;

	for(i = 0; i < NUM_NAME_RECORDS; i++)
	{
		set_generic_record(1, i, &default_name_records[i]);
	}
}

/* 0x801190b0 */
void clear_record(char *p, int n)
{
	while(n)
	{
		*p++ = 0;
		n--;
	}
}

/* 0x801190d0: age the records in use (as written, those that fail their check) */
void age_records(void)
{
	int	i;

	for(i = 0; i < NUM_RECORDS; i++)
	{
		if(get_generic_record(0, i, &tmp_record))
		{
			if(tmp_record.games_played > 0)
			{
				tmp_record.last_used++;
				set_generic_record(0, i, &tmp_record);
			}
		}
	}
}

/* 0x8011914c: the CMOS record with this name and PIN, or -1 */
int find_record(char *name)
{
	int	i;

	for(i = 0; i < NUM_RECORDS; i++)
	{
		if(!get_generic_record(0, i, &tmp_record))
		{
			if(!compare_record((char *)&tmp_record, name, 10))
			{
				return(i);
			}
		}
		else
		{
			clear_record((char *)&tmp_record, sizeof(plyr_record_t));
		}
	}
	return(-1);
}

/* 0x801191e0: the same in the records read into memory, numbered from 1 */
int find_ram_record(char *name)
{
	plyr_record_t	*rec;
	int		i;

	if(ram_records)
	{
		rec = ram_records;
		for(i = 0; i < NUM_RECORDS; i++, rec++)
		{
			if(!compare_record((char *)rec, name, 10))
			{
				return(i + 1);
			}
		}
	}
	return(-1);
}

/* 0x80119258 */
int compare_record(char *a, char *b, int n)
{
	while(n)
	{
		if(*a++ != *b++)
		{
			return(1);
		}
		n--;
	}
	return(0);
}

/* 0x80119290: an unused record, else the oldest */
int find_free_record(void)
{
	int	i;
	int	oldest, age;

	oldest = 0;
	age = -1;
	for(i = 0; i < NUM_RECORDS; i++)
	{
		if(get_generic_record(0, i, &tmp_record))
		{
			clear_record((char *)&tmp_record, sizeof(plyr_record_t));
		}
		if(tmp_record.games_played <= 0)
		{
			return(i);
		}
		if(tmp_record.last_used >= age)
		{
			age = tmp_record.last_used;
			oldest = i;
		}
	}
	return(oldest);
}

/* 0x80119338: read the records into memory */
void get_all_records(void)
{
	plyr_record_t	*rec;
	int		i;

	if(!ram_records)
	{
		ram_records = malloc(sizeof(plyr_record_t) * NUM_RECORDS);
	}
	if(ram_records)
	{
		rec = ram_records;
		for(i = 0; i < NUM_RECORDS; i++, rec++)
		{
			get_generic_record(0, i, rec);
		}
	}
}

/* 0x801193b0.. the qsort orders: a player with fewer than MIN_GAMES games goes last */
int sort_games_won(plyr_record_t *a, plyr_record_t *b)
{
	int	d;

	if(a->games_played < MIN_GAMES && b->games_played < MIN_GAMES)
	{
		return(0);
	}
	if(a->games_played < MIN_GAMES)
	{
		return(1);
	}
	if(b->games_played < MIN_GAMES)
	{
		return(-1);
	}
	d = b->games_won - a->games_won;
	if(d == 0)
	{
		return(b->games_played - a->games_played);
	}
	return(d);
}

/* 0x80119418 */
int sort_x10(plyr_record_t *a, plyr_record_t *b)
{
	int	d;

	if(a->games_played < MIN_GAMES && b->games_played < MIN_GAMES)
	{
		return(0);
	}
	if(a->games_played < MIN_GAMES)
	{
		return(1);
	}
	if(b->games_played < MIN_GAMES)
	{
		return(-1);
	}
	d = b->x10 - a->x10;
	if(d == 0)
	{
		return(b->games_played - a->games_played);
	}
	return(d);
}

/* 0x80119480 */
int sort_win_percent(plyr_record_t *a, plyr_record_t *b)
{
	int	d;
	int	pa, pb;

	if(a->games_played < MIN_GAMES && b->games_played < MIN_GAMES)
	{
		return(0);
	}
	if(a->games_played < MIN_GAMES)
	{
		return(1);
	}
	if(b->games_played < MIN_GAMES)
	{
		return(-1);
	}
	pa = (a->games_won * 1000) / a->games_played;
	pb = (b->games_won * 1000) / b->games_played;
	d = pb - pa;
	if(d == 0)
	{
		return(b->games_played - a->games_played);
	}
	return(d);
}

/* 0x80119574 */
int sort_games_played(plyr_record_t *a, plyr_record_t *b)
{
	int	d;

	if(a->games_played < MIN_GAMES && b->games_played < MIN_GAMES)
	{
		return(0);
	}
	if(a->games_played < MIN_GAMES)
	{
		return(1);
	}
	if(b->games_played < MIN_GAMES)
	{
		return(-1);
	}
	if((d = b->games_played - a->games_played))
	{
		return(d);
	}
	if((d = b->games_won - a->games_won))
	{
		return(d);
	}
	return(b->x10 - a->x10);
}

/* 0x801195f0 */
int sort_x11(plyr_record_t *a, plyr_record_t *b)
{
	int	d;
	int	ga, gb;

	if(a->games_played < MIN_GAMES && b->games_played < MIN_GAMES)
	{
		return(0);
	}
	if(a->games_played < MIN_GAMES)
	{
		return(1);
	}
	if(b->games_played < MIN_GAMES)
	{
		return(-1);
	}
	ga = a->games_played;
	if(!ga)
	{
		ga = 500;
	}
	gb = b->games_played;
	if(!gb)
	{
		gb = 500;
	}
	if((d = (b->x11 * 100) / gb - (a->x11 * 100) / ga))
	{
		return(d);
	}
	if((d = b->games_won - a->games_won))
	{
		return(d);
	}
	return(b->x10 - a->x10);
}

/* 0x80119714: lower is better */
int sort_x13(plyr_record_t *a, plyr_record_t *b)
{
	int	d;
	int	ga, gb, va, vb;

	if(a->games_played < MIN_GAMES && b->games_played < MIN_GAMES)
	{
		return(0);
	}
	if(a->games_played < MIN_GAMES)
	{
		return(1);
	}
	if(b->games_played < MIN_GAMES)
	{
		return(-1);
	}
	ga = a->games_played;
	va = a->x13;
	if(!ga)
	{
		ga = 1;
		va = 500;
	}
	gb = b->games_played;
	vb = b->x13;
	if(!gb)
	{
		gb = 1;
		vb = 500;
	}
	if((d = (va * 100) / ga - (vb * 100) / gb))
	{
		return(d);
	}
	if((d = a->games_won - b->games_won))
	{
		return(d);
	}
	return(a->x10 - b->x10);
}

/* 0x8011985c */
int sort_x1d(plyr_record_t *a, plyr_record_t *b)
{
	int	d;
	float	fa, fb;

	if(a->games_played < MIN_GAMES && b->games_played < MIN_GAMES)
	{
		return(0);
	}
	if(a->games_played < MIN_GAMES)
	{
		return(1);
	}
	if(b->games_played < MIN_GAMES)
	{
		return(-1);
	}
	fa = (float)a->x1d / (float)a->games_played;
	fb = (float)b->x1d / (float)b->games_played;
	if((d = (int)((fb - fa) * 1000.0f)))
	{
		return(d);
	}
	if((d = b->games_won - a->games_won))
	{
		return(d);
	}
	return(b->x10 - a->x10);
}

/* 0x80119924 */
int sort_x21(plyr_record_t *a, plyr_record_t *b)
{
	int	d;
	float	fa, fb;

	if(a->games_played < MIN_GAMES && b->games_played < MIN_GAMES)
	{
		return(0);
	}
	if(a->games_played < MIN_GAMES)
	{
		return(1);
	}
	if(b->games_played < MIN_GAMES)
	{
		return(-1);
	}
	fa = (float)a->x21 / (float)a->games_played;
	fb = (float)b->x21 / (float)b->games_played;
	if((d = (int)((fb - fa) * 1000.0f)))
	{
		return(d);
	}
	if((d = b->games_won - a->games_won))
	{
		return(d);
	}
	return(b->x10 - a->x10);
}
