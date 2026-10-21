/*
 * guntest.c - one object (text 0x801189d8-0x8011b4d4, built at -O: -O -mips4 -mcpu=r5000
 * -fsigned-char, not DIAG's usual -O2): the gun board reset, the game's audit average, float
 * adjustments and CMOS layout, and the player records (0x801189d8-0x801199ec), then GUN
 * CALIBRATION and GUN TEST (0x801199ec-0x8011b4d4). Rodata 0x80159ab0-0x80159ec4, data
 * 0x80167d98-0x801680dc, bss tmp_record 0x802ef498 (and the guns, guns_calibrated).
 *
 * The code up to 0x801199ec is the game's: gun_board_reset is src/game/render/camera.c's,
 * average_audit .. game_setup_cmos are src/game/core/adjust.c's, and the record functions are
 * the ones that follow them in the game's adjust.c (0x800f2b6c-0x800f3580, not decompiled there
 * yet), at an older revision: get_float_adjustment reads the first slot three times and
 * rewrites through set_float_adjustment. Here only gun_board_reset, get_float_adjustment and
 * set_float_adjustment are called; the rest came along.
 *
 * One object: no object can start at 0x801199ec (objects are 8-aligned and there is no junk
 * word before it), the data runs on unpadded at 0x80168084 ("OFF"/"ON" after the default
 * name records), the rodata likewise at 0x80159b70, and the empty function at 0x8011b4cc is
 * the startaudit game_setup_cmos calls. The object ends with the junk word at 0x8011b4d4.
 *
 * A player record (generic CMOS record table 0, 0x38 bytes) is a name and PIN, an age, the
 * games played and won and the stats the record tables rank by; the comparison functions
 * are for qsort and put players with fewer than four games last. Its stats from 0x11 on are
 * unaligned (each such field packed): the compiler reads them as bit fields, a 32-bit one
 * with ulw.
 *
 * The calibration and test read the guns from the I/O ASIC's gun bytes as the game's
 * read_guns() does (src/game/core/shoot.c), into the same records (gun_t,
 * src/game/render/camera.h). Each gun is calibrated from where it reads at the top left and
 * bottom right targets, tried out until its trigger accepts it, and kept in adjustments
 * 0x53-0x68 (three slots a float) with 37373737 in 0x52; GUN TEST shows the aim, triggers and
 * pumps and sets the flash brightness (adjustment 0x37) with the start buttons.
 */
#include <stdio.h>
#include <stdlib.h>
#include <goose/cmos.h>
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include "diag.h"

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

/* a light gun's record (0x802f03b0 player 1, 0x802f03d4 player 2), as the game's */
typedef struct gun
{
	int	raw_x, raw_y;		/* +0x00: the I/O ASIC's 12 and 10 bit readings */
	int	on_screen;		/* +0x08: the gun sees the screen (bit 0x40) */
	float	scale_x, scale_y;	/* +0x0c, +0x10: raw gun units per pixel */
	float	off_x, off_y;		/* +0x14, +0x18 */
	int	screen_x, screen_y;	/* +0x1c, +0x20: 1..399, 1..255 */
} gun_t;

gun_t	guns[2];				/* 0x802f03b0 */
int	guns_calibrated;			/* 0x802ef4e4: the CMOS holds a calibration (set at start-up) */
extern int	tick_counter;			/* 0x8016982c: counted by the interrupt handler */

extern int	diag_get_dip_coin_current(void);
extern void	specific_gamma_load(float, float);
extern void	sleep(int);

#define GUN_X1	(*(volatile unsigned char *)0xb6800000)
#define GUN_P1	(*(volatile unsigned char *)0xb6800004)
#define GUN_Y1	(*(volatile unsigned char *)0xb6800008)
#define GUN_Y1_HI	(*(volatile unsigned char *)0xb680000c)
#define GUN_X2	(*(volatile unsigned char *)0xb6800010)
#define GUN_P2	(*(volatile unsigned char *)0xb6800014)
#define GUN_Y2	(*(volatile unsigned char *)0xb6800018)
#define GUN_Y2_HI	(*(volatile unsigned char *)0xb680001c)
#define GUN_LED1	(*(unsigned char *)0xb6800004)	/* written through a plain pointer: the store goes in a delay slot */
#define GUN_RESET	(*(volatile unsigned int *)0xb6800000)

#define CALIBRATION_MAGIC	37373737	/* adjustment 0x52 */

/* both guns' raw positions and whether they see the screen; the gun LED blinks */
#define READ_GUNS()							\
	guns[0].raw_x = GUN_X1;						\
	guns[0].raw_x |= (GUN_P1 & 0xf) << 8;				\
	guns[0].raw_y = GUN_Y1;						\
	guns[0].raw_y |= (GUN_Y1_HI & 3) << 8;				\
	guns[0].raw_x &= 0xfff;						\
	guns[0].on_screen = GUN_P1 & 0x40;				\
	guns[1].raw_x = GUN_X2;						\
	guns[1].raw_x |= (GUN_P2 & 0xf) << 8;				\
	guns[1].raw_y = GUN_Y2;						\
	guns[1].raw_y |= (GUN_Y2_HI & 3) << 8;				\
	guns[1].raw_x &= 0xfff;						\
	guns[1].on_screen = GUN_P2 & 0x40;				\
	GUN_LED1 = (GUN_P1 & 0x7f) | ((tick_counter & 4) << 5)

/* a gun's screen position from its raw one (the clamps of y set x, as in the game) */
#define GUN_SCREEN(g)							\
	(g).screen_x = ((float)(g).raw_x - (g).off_x) / (g).scale_x;	\
	if((float)(g).screen_x < 1.0F)					\
		(g).screen_x = 1;					\
	if(399.0F < (float)(g).screen_x)				\
		(g).screen_x = 399;					\
	(g).screen_y = 256.0F - ((float)(g).raw_y - (g).off_y) / (g).scale_y; \
	if((float)(g).screen_y < 1.0F)					\
		(g).screen_x = 1;					\
	if(255.0F < (float)(g).screen_y)				\
		(g).screen_x = 255

char	on_off[][4] = { "OFF", "ON" };
char	open_closed[][7] = { "OPEN", "CLOSED" };
char	gun_names[][32] = { "LEFT GUN (GREEN)", "RIGHT GUN (PURPLE)" };

/* 1 when any of the bits is set */
static inline int is_set(int bits)
{
	return(bits != 0);
}

void	calibrate_gun(int);
int	try_calibration(int);
void	wait_gun_release(void);

/* 0x801199ec */
int calibrate_guns(void)
{
	int	done1 = 0, done2 = 0;
	int	val;
	float	gamma;

	if(get_adjustment(0x37, &val))
		gamma = 0.6F;
	else
		gamma = (float)val / 100.0F;
	specific_gamma_load(gamma, 1.0F);
	draw_title("GUN CALIBRATION");
	set_text_justification_mode(10);
	set_text_font(1);
	while(!done1)
	{
		calibrate_gun(0);
		done1 = try_calibration(0);
	}
	while(!done2)
	{
		calibrate_gun(1);
		done2 = try_calibration(1);
	}
	set_float_adjustment(0x53, guns[0].scale_x);
	set_float_adjustment(0x56, guns[0].scale_y);
	set_float_adjustment(0x59, guns[0].off_x);
	set_float_adjustment(0x5c, guns[0].off_y);
	set_float_adjustment(0x5f, guns[1].scale_x);
	set_float_adjustment(0x62, guns[1].scale_y);
	set_float_adjustment(0x65, guns[1].off_x);
	set_float_adjustment(0x68, guns[1].off_y);
	set_adjustment(0x52, CALIBRATION_MAGIC);
	guns_calibrated = 1;
	specific_gamma_load(0.0F, 1.0F);
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	return(0);
}

/* 0x80119b78 */
int gun_calibration(void)
{
	calibrate_guns();
	return(0);
}

/* 0x80119b98 */
int gun_test(void)
{
	int	last, sw;
	int	val;
	float	gamma;

	last = diag_get_dip_coin_current();
	draw_title("GUN TEST");
	set_text_justification_mode(10);
	set_text_font(1);
	if(!guns_calibrated)
	{
		set_text_color(0xffff0000);
		set_text_position((int)(hres * 0.5F), (int)(vres * 0.525F), 2.0F);
		oprintf("CALIBRATION NOT FOUND");
		set_text_position((int)(hres * 0.5F), (int)(vres * 0.475F), 2.0F);
		oprintf("PLEASE CALIBRATE GUNS");
		wait_test_exit();
		delete_multiple_strings(0, 0);
		del1c(0, 0);
		return(0);
	}
	message_box("PRESS CREDIT OR TEST TO EXIT", -1.0F, 5.0F, 1.0F);
	set_text_color(0xff404040);
	set_text_position((int)(hres * 0.5F), (int)(vres * 0.65F), 2.0F);
	oprintf("POINT GUNS TOWARD SCREEN TO TEST TARGETING");
	set_text_position((int)(hres * 0.5F), (int)(vres * 0.525F), 1.0F);
	oprintf("DECREASE/INCREASE WITH PLAYER 1/PLAYER 2 START");
	set_text_position((int)(hres * 0.5F), (int)(vres * 0.475F), 1.0F);
	oprintf("PRESS CREDIT OR TEST TO SAVE AND EXIT");
	if(get_adjustment(0x37, &val))
		val = 60;
	gamma = (float)val / 100.0F;
	if(gamma < 0.3 || gamma > 1.0)
		gamma = 0.5F;
	while(!sw_select() && !sw_back())
	{
		if(gamma != 0.0)
		{
			sw = diag_get_dip_coin_current();
			if((sw ^ last) & 0x24)
			{
				if((sw & 4) && gamma >= 0.31)
					gamma = gamma - 0.01;
				if((sw & 0x20) && gamma <= 0.99)
					gamma = gamma + 0.01;
			}
			last = sw;
			specific_gamma_load(gamma, 1.0F);
		}
		READ_GUNS();
		GUN_SCREEN(guns[0]);
		GUN_SCREEN(guns[1]);
		GUN_RESET = 0;

		delete_multiple_strings(1, 1);
		set_string_id(1);
		set_text_color(-1);
		if(gamma != 0.0)
		{
			set_text_position((int)(hres * 0.5F), (int)(vres * 0.575F), 1.0F);
			oprintf("FLASH BRIGHTNESS: %3.2f", gamma);
		}

		set_text_color(0xff008000);
		if(guns_calibrated && guns[0].on_screen)
		{
			set_text_position(guns[0].screen_x, guns[0].screen_y, 1.0F);
			oprintf("X");
		}
		set_text_position((int)(hres * 0.25F), (int)(vres * 0.4F), 2.0F);
		oprintf("PLAYER 1 GUN");
		set_text_position((int)(hres * 0.25F), (int)(vres * 0.3F), 2.0F);
		if(guns_calibrated)
			oprintf("%s SCREEN", on_off[guns[0].on_screen ? 1 : 0]);
		set_text_position((int)(hres * 0.25F), (int)(vres * 0.25F), 2.0F);
		oprintf("TRIGGER %s", open_closed[is_set(GUN_P1 & 0x10)]);
		set_text_position((int)(hres * 0.25F), (int)(vres * 0.2F), 2.0F);
		oprintf("PUMP %s", open_closed[is_set(GUN_P1 & 0x20)]);

		set_text_color(0xff800080);
		if(guns_calibrated && guns[1].on_screen)
		{
			set_text_position(guns[1].screen_x, guns[1].screen_y, 1.0F);
			oprintf("x");
		}
		set_text_position((int)(hres * 0.75F), (int)(vres * 0.4F), 2.0F);
		oprintf("PLAYER 2 GUN");
		set_text_position((int)(hres * 0.75F), (int)(vres * 0.3F), 2.0F);
		if(guns_calibrated)
			oprintf("%s SCREEN", on_off[guns[1].on_screen ? 1 : 0]);
		set_text_position((int)(hres * 0.75F), (int)(vres * 0.25F), 2.0F);
		oprintf("TRIGGER %s", open_closed[is_set(GUN_P2 & 0x10)]);
		set_text_position((int)(hres * 0.75F), (int)(vres * 0.2F), 2.0F);
		oprintf("PUMP %s", open_closed[is_set(GUN_P2 & 0x20)]);
		sleep(1);
	}
	while(sw_select() || sw_back())
		sleep(1);
	if(gamma != 0.0)
		set_adjustment(0x37, (int)(gamma * 100.0F));
	specific_gamma_load(0.0F, 1.0F);
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	return(0);
}

/* 0x8011a6e0: the calibrated gun's aim shown until its trigger accepts it (1) or a start
   button asks for another go (0) */
int try_calibration(int player)
{
	int	trigger;

	wait_gun_release();
	sleep(5);
	set_text_color(-1);
	set_text_position((int)(hres * 0.5F), (int)(vres * 0.6F), 2.0F);
	set_string_id(4);
	oprintf("POINT GUN TOWARD SCREEN TO TEST TARGETING");
	set_text_position((int)(hres * 0.5F), (int)(vres * 0.5F), 2.0F);
	set_string_id(4);
	oprintf("PULL TRIGGER TO ACCEPT CALIBRATION");
	set_text_position((int)(hres * 0.5F), (int)(vres * 0.4F), 2.0F);
	set_string_id(4);
	oprintf("PRESS EITHER START BUTTON TO RECALIBRATE");
	for(;;)
	{
		if(diag_get_dip_coin_current() & 0x24)
		{
			delete_multiple_strings(1, 1);
			delete_multiple_strings(4, 4);
			wait_gun_release();
			return(0);
		}
		READ_GUNS();
		if(player == 0)
		{
			GUN_SCREEN(guns[0]);
			trigger = GUN_P1 & 0x10;
		}
		else
		{
			GUN_SCREEN(guns[1]);
			trigger = GUN_P2 & 0x10;
		}
		GUN_RESET = 0;
		if(trigger)
		{
			delete_multiple_strings(1, 1);
			delete_multiple_strings(4, 4);
			wait_gun_release();
			return(1);
		}
		delete_multiple_strings(1, 1);
		set_string_id(1);
		if(player == 0)
			set_text_color(0xff00ff00);
		else
			set_text_color(0xffff00ff);
		if(guns[player].on_screen)
		{
			set_text_position(guns[player].screen_x, guns[player].screen_y, 1.0F);
			oprintf("x");
		}
		sleep(1);
	}
}

/* 0x8011abe8: a gun's calibration from where it reads at the top left and bottom right
   targets (the factory one when they make no sense); a start button cancels */
void calibrate_gun(int player)
{
	int	count = 0, cancel;
	int	x1, y1, x2, y2;
	int	trigger;
	float	w, h;

	wait_gun_release();
	x1 = x2 = y1 = y2 = -1;
	cancel = 0;
	set_text_color(0xff000000);
	set_text_position((int)(hres * 0.5F), (int)(vres * 0.45F), 2.0F);
	set_string_id(4);
	oprintf("POINT GUN AT X AND PULL TRIGGER");
	set_text_position((int)(hres * 0.5F), (int)(vres * 0.35F), 2.0F);
	set_string_id(4);
	oprintf("PRESS EITHER START BUTTON TO CANCEL");
	if(player == 0)
		set_text_color(0xff00ff00);
	else
		set_text_color(0xffff00ff);
	set_text_position((int)(hres * 0.5F), (int)(vres * 0.65F), 2.0F);
	set_string_id(4);
	oprintf(gun_names[player]);
	set_text_color(0xff000000);
	set_string_id(1);
	set_text_position((int)(hres * 0.5F), (int)(vres * 0.55F), 2.0F);
	oprintf("TOP LEFT");
	while(x1 == -1 && !cancel)
	{
		if(diag_get_dip_coin_current() & 0x24)
			cancel = 1;
		else
		{
			if(player == 0)
				trigger = GUN_P1 & 0x10;
			else
				trigger = GUN_P2 & 0x10;
			if(trigger)
			{
				READ_GUNS();
				if(guns[player].on_screen && guns[player].raw_x && guns[player].raw_y)
				{
					x1 = guns[player].raw_x;
					y1 = guns[player].raw_y;
				}
			}
			GUN_RESET = 0;
			count++;
			if(count & 2)
			{
				set_text_color(-1);
				set_string_id(2);
				set_text_position(20, (int)(vres - 20.0F), 2.0F);
				oprintf("X");
			}
			else
				delete_multiple_strings(2, 2);
			sleep(1);
		}
	}
	delete_multiple_strings(2, 2);
	if(!cancel)
	{
		sleep(1);
		wait_gun_release();
		sleep(5);
		delete_multiple_strings(1, 1);
		set_text_position((int)(hres * 0.5F), (int)(vres * 0.55F), 2.0F);
		set_string_id(1);
		oprintf("BOTTOM RIGHT");
		while(x2 == -1 && !cancel)
		{
			if(diag_get_dip_coin_current() & 0x24)
				cancel = 1;
			else
			{
				if(player == 0)
					trigger = GUN_P1 & 0x10;
				else
					trigger = GUN_P2 & 0x10;
				if(trigger)
				{
					READ_GUNS();
					if(guns[player].on_screen && guns[player].raw_x && guns[player].raw_y)
					{
						x2 = guns[player].raw_x;
						y2 = guns[player].raw_y;
					}
				}
				GUN_RESET = 0;
				count++;
				if(count & 2)
				{
					set_text_color(-1);
					set_string_id(2);
					set_text_position((int)(hres - 20.0F), 20, 2.0F);
					oprintf("X");
				}
				else
					delete_multiple_strings(2, 2);
				sleep(1);
			}
		}
		if(!cancel)
		{
			printf("%d %d %d %d\n", x1, y1, x2, y2);
			if(x1 >= x2 || y1 >= y2)
			{
				x1 = 0x14a;
				y1 = 0x24;
				x2 = 0x6b8;
				y2 = 0xdc;
				printf("%d %d %d %d\n", x1, y1, x2, y2);
			}
			w = x2 - x1;
			h = y2 - y1;
			if(w == 0.0F)
				w = 0.1F;
			if(h == 0.0F)
				h = 0.1F;
			guns[player].scale_x = w / 360.0F;
			guns[player].scale_y = h / 216.0F;
			guns[player].off_x = ((float)x1 / guns[player].scale_x - 20.0F) * guns[player].scale_x;
			guns[player].off_y = ((float)y1 / guns[player].scale_y - 20.0F) * guns[player].scale_y;
		}
	}
	delete_multiple_strings(1, 1);
	delete_multiple_strings(4, 4);
	delete_multiple_strings(2, 2);
	wait_gun_release();
}

/* 0x8011b458: until the start buttons and both triggers are let go */
void wait_gun_release(void)
{
	int	sw;

	sw = diag_get_dip_coin_current();
	while((sw & 0x24) || (GUN_P1 & 0x10) || (GUN_P2 & 0x10))
	{
		sw = diag_get_dip_coin_current();
		sleep(1);
	}
}

/* 0x8011b4cc: game_setup_cmos's audit names (the game writes them; nothing here) */
void startaudit(FILE *f)
{
}
