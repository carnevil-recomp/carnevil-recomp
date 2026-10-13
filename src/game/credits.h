/*
 * credits.h - coins, credits, pricing and the start buttons (src/game/credits.c, data in the
 * generated pricing_data.c).
 */
#ifndef CREDITS_H
#define CREDITS_H

/* an 88-byte pricing record (the game's "credit_info" table entries) */
typedef struct pricing_record
{
	int	slot_units[5];		/* +0x00 units per coin: left, right, center, extra, bill validator */
	int	units_per_credit;	/* +0x14 */
	int	bonus_coins;		/* +0x18 coins per bonus credit (0 = none) */
	int	bonus_threshold;	/* +0x1c units before the bonus applies */
	int	credits_to_start;	/* +0x20 */
	int	credits_to_continue;	/* +0x24 */
	int	max_credits;		/* +0x28 */
	int	unk2c;			/* +0x2c */
	int	show_fraction;		/* +0x30 */
	int	counter_pulses[5];	/* +0x34 coin counter pulses per coin, per slot */
	int	currency;		/* +0x48 index into currency_names */
	char	*text;			/* +0x4c attract-screen pricing text (newline separated) */
	char	*name;			/* +0x50 */
	char	*extra;			/* +0x54 */
} pricing_record_t;

extern pricing_record_t	pricing_records[];
extern pricing_record_t	**pricing_continents[7];	/* 0x80179b40 */
extern pricing_record_t	*pricing_by_dip[32];		/* 0x80179b60 */
extern char		*currency_names[12];		/* 0x80177be8 */

/* per-player state block (0x801e0f64 + player * 0x120): only the fields credits.c uses */
typedef struct player_stats
{
	int	unk0;			/* +0x00 (the record starts 4 bytes before Binary Ninja's player_stats) */
	int	state;			/* +0x04 (0x801e0f64): 0 idle, 4 playing, 8 waiting to continue, 0x20 joining */
	int	unk8;			/* +0x08: cleared when the main game process starts */
	unsigned int colour;		/* +0x0c: 0xff00 player 1, 0xff00ff player 2 */
	int	unk10, unk14, unk18, unk1c, unk20;
	float	unk24, unk28;		/* +0x24, +0x28: both start at 1300 - 200 * adjustment 0x31 */
	int	unk2c;
	int	start_time;		/* +0x30: D_801facc8 when the player joined */
	int	unk34;
	int	level_start_time;	/* +0x38: D_801facc8 when the level started */
	int	unk3c;
	int	continues;		/* +0x40 */
	void	*gun;			/* +0x44: the player's gun record (camera.c guns[]) */
	int	unk48, unk4c, unk50, unk54, unk58, unk5c, unk60, unk64, unk68;
	float	unk6c;			/* +0x6c: 100 player 1, 300 player 2 (a screen x) */
	float	unk70;			/* +0x70: 128 (a screen y) */
	int	priority;		/* +0x74: sound priority word (levelsnd.c) */
	int	priority2;		/* +0x78 */
	int	unk7c, unk80, unk84;
	int	volume;			/* +0x88: sound volume (levelsnd.c) */
	int	unk8c[11];
	int	shooting;		/* +0xb8: 1 while the player's shot is being resolved */
	int	unkbc[5];
	int	index;			/* +0xd0: 0 or 1 */
	int	unkd4;			/* +0xd4: 100 after a continue */
	int	unkd8[7];
	int	time_to_continue;	/* +0xf4 */
	int	unkf8, unkfc, unk100;
	int	unk104;			/* +0x104: 6 at a start */
	int	unk108, unk10c, unk110, unk114, unk118, unk11c;
} player_stats_t;

/* the credit counters (0x801de8d8) */
typedef struct credit_count
{
	int	credits, service_credits, coin_units, bonus_count, bonus_units;
} credit_count_t;

/* the pricing in force (0x801e5cf8): the record's first 13 words, then the strings */
typedef struct live_pricing
{
	int	slot_units[5];
	int	units_per_credit, bonus_coins, bonus_threshold;
	int	credits_to_start, credits_to_continue, max_credits, unk2c, show_fraction;
	char	symbol[8];		/* 0x801e5d2c "$" / "L" */
	char	name[8];		/* 0x801e5d34 */
	char	text[256];		/* 0x801e5d3c the attract pricing text */
} live_pricing_t;

/* the coin meters (0x80237458) */
typedef struct coin_counter
{
	int	pulses[5], enabled[5], pending[5], mode, unk;
} coin_counter_t;

extern player_stats_t	player_stats[2];		/* 0x801e0f60 (player1_stats), 0x801e1080 */
extern int		game_state[4];			/* 0x80212e14: mode (1 attract, 2/4/0x10/0x20/0x80 in play, 0x100), ?, start count, debug flags */
#define game_phase	game_state[0]		/* 0x800 booted/attract, 1 attract loop, 0x10 game start, 0x20 level select, 0x80 loading, 2 playing, 8 game over */
#define debug_mode	game_state[3]		/* 0x80212e20 (one array: the draw pass keeps its address in a register) */
extern credit_count_t	credit_count;
#define credits		credit_count.credits
extern live_pricing_t	live_pricing;
extern coin_counter_t	coin_counter;
/* the coin lockout block (0x801e0970) */
typedef struct coin_lock
{
	int	slam, unused, freeplay, unk97c, unk980;
} coin_lock_t;
extern coin_lock_t	coin_lock;
#define is_freeplay	coin_lock.freeplay
extern char		credit_line1[], credit_line2[];	/* 0x801e6ee8, 0x801e6f68 */

void	init_credits(void);
void	credit_info(int mode);
int	check_for_freeplay(void);
int	check_credits_to_start_mode(void);
int	check_credits_to_continue_mode(void);
int	have_credits_to_start(void);
int	have_credits_to_continue(void);
int	any_credits(void);
void	coin_handler(int closed, int swid);
void	service_credit_handler(int closed, int swid);
void	volume_switch_handler(int closed, int swid);
void	dispatch_start_proc(int closed, int swid);
void	dispatch_slam_proc(int closed, int swid);
void	start_proc(int *args);
void	coin_volume_proc(int *args);
void	ccproc_proc(int *args);
void	something_todo_with_credits(void);
void	show_volume_level(void);
void	attract_credit_lines(void);

#endif
