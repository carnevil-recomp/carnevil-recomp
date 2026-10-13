/*
 * credits.c - coins, credits, pricing and the start buttons (0x800c6a64-0x800c8198).
 *
 * The GOOSE switch module calls the handlers installed by main_proc: coin_handler for the coin
 * switches (0, 1, 7, 8, 15), service_credit_handler (6), dispatch_slam_proc (3),
 * volume_switch_handler (11, 12) and dispatch_start_proc for the start buttons (2, 5), which
 * spawns start_proc to take the credits and start a player. Pricing comes from the dip
 * switches or the continent/country/price adjustments (pricing_data.c).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "credits.h"
#include "soundio.h"

#ifdef PORT_HOST
#define INLINE
#else
#define INLINE inline
#endif

extern int	get_adjustment(int, int *), set_adjustment(int, int);
extern int	get_audit(int, int *), set_audit(int, int), increment_audit(int);
extern int	get_dip_coin_current(void);
extern void	sleep(int);
extern void	*qcreate(char *, int, void (*)(), int, int, int, int);
extern int	snd_master_volume(int);
#include "levelsnd.h"
#include "waves.h"
extern void	graphical_printf(char *, char *, ...);
extern int	_ioctl(int, int, int);
extern void	init_player_stats(int);				/* 0x800e2d48 */
extern void	init_main_game_proc(int);			/* 0x800c5748 */
extern void	guAlphaSource(int), guColorCombineFunction(int), grConstantColorValue(unsigned int);
extern void	grDepthBufferFunction(int), grDepthMask(int);
extern void	draw_string_centered(int, char *, float, float, unsigned int);	/* 0x800ddf74 (add_to_print_) */
extern void	draw_tri_flat(void *, void *, void *);				/* 0x800d2174 */
extern char	text_buffer[];							/* 0x801e7100 */

/* credit state: one block at 0x801de8d8 (the compiler addresses its members as a block) */
credit_count_t	credit_count;
#define credits		credit_count.credits
#define service_credits	credit_count.service_credits
#define coin_units	credit_count.coin_units		/* units towards the next credit */
#define bonus_count	credit_count.bonus_count	/* coins towards the bonus credit */
#define bonus_units	credit_count.bonus_units
coin_lock_t	coin_lock;				/* 0x801e0970 */
#define slam_active	coin_lock.slam
#define coin_lock_unused coin_lock.unused
#define unk_801e097c	coin_lock.unk97c
#define unk_801e0980	coin_lock.unk980
int		intro_cue_played;			/* 0x80179b58 */
int		pricing_from_dips;			/* 0x80237274 */
int		unk_801e09d0;

/* the selected pricing (credit_info_struct_2, 0x801e5cf8) */
live_pricing_t	live_pricing;
pricing_record_t *pricing;				/* 0x801a4350 (credit_info_struct_1) */
int		custom_pricing[22];			/* 0x801a4360: record built from adjustments 4..25 */
int		*custom_pricing_strings;		/* 0x801a43ac */

/* coin counters (0x80237458..0x80237498) */
coin_counter_t	coin_counter;

char		credit_line1[128];			/* 0x801e6ee8 */
char		credit_line2[128];			/* 0x801e6f68 */
player_stats_t	player_stats[2];			/* 0x801e0f60 */
int		game_state[4];				/* 0x80212e14: mode, ?, start count, debug flags (0x80212e20) */

/* the file's own helpers, in the original's order (the inline ones only inline into later callers) */
static void	add_coin(int closed, int swid);
static void	setup_pricing(void);
void		free_play_func(void);
static void	init_credit_counts(void);
static void	init_coin_counters(void);
static void	slam_proc(int *args);
static void	pulse_coin_counter(int bits);
int		have_credits_to_start(void);
int		have_credits_to_continue(void);
int		check_credits_to_start_mode(void);
int		check_credits_to_continue_mode(void);
void		take_credits(short n);
void		draw_hbar(float x0, float x1, float y, float h);

/* 0x800c6a64: split the pricing text into up to three attract lines, upper-cased */
extern char	attract_text1[], attract_text2[], attract_text3[];
extern int	attract_text_state;
void something_todo_with_credits(void)
{
	char	*p;
	char	*s;
	char	*d1, *d2, *d3;
	char	c;
	char	nl;

	d1 = attract_text1;
	d2 = attract_text2;
	d3 = attract_text3;
	s = live_pricing.text;
	nl = '\n';
	while(*s && *s != nl)
	{
		*d1++ = *s++;
	}
	*d1 = 0;
	p = attract_text1;
	while((c = *p) != 0)
	{
		if((unsigned char)(c - 'a') < 26)
		{
			c -= 0x20;
		}
		*p = c;
		p++;
	}
	if(*s == 0)
	{
		attract_text_state = 1;
		return;
	}
	s++;
	while(*s && *s != nl)
	{
		*d2++ = *s++;
	}
	*d2 = 0;
	p = attract_text2;
	while((c = *p) != 0)
	{
		if((unsigned char)(c - 'a') < 26)
		{
			c -= 0x20;
		}
		*p = c;
		p++;
	}
	if(*s == 0)
	{
		attract_text_state = 2;
		return;
	}
	s++;
	while(*s && *s != nl)
	{
		*d3++ = *s++;
	}
	*d3 = 0;
	p = attract_text3;
	while((c = *p) != 0)
	{
		if((unsigned char)(c - 'a') < 26)
		{
			c -= 0x20;
		}
		*p = c;
		p++;
	}
	attract_text_state = 3;
}

/* 0x800c6c34: can this player start (or continue) with what is in the machine? */
INLINE int player_can_start(int player)
{
	switch((unsigned int)player_stats[player].state)
	{
	case 0x20:
	case 0:
		return(have_credits_to_start());
	case 8:
		return(have_credits_to_continue());
	default:
		return(0);
	}
}

/* 0x800c6ca8 */
void inc_80212e1c(void)
{
	game_state[2]++;
}

/* 0x800c6cc0 / 0x800c6d04: a start button closing spawns the start process */
void dispatch_start_proc(int closed, int swid)
{
	if(closed)
	{
		qcreate("start", 0, start_proc, swid, 0, 0, 0);
	}
}

void start_proc(int *args)
{
	int	player;

	switch(args[0])
	{
	case 5:
		player = 1;
		break;
	default:
		player = 0;
		break;
	}
	if(player_can_start(player))
	{
		switch((unsigned int)game_mode)
		{
		case 1:
		case 4:
			take_credits(player_stats[player].state == 8 ? check_credits_to_continue_mode() : check_credits_to_start_mode());
			init_player_stats(player);
			init_main_game_proc(player);
			break;
		case 2:
		case 0x10:
		case 0x20:
		case 0x80:
			take_credits(player_stats[player].state == 8 ? check_credits_to_continue_mode() : check_credits_to_start_mode());
			init_player_stats(player);
			break;
		case 0x100:
			unk_801e09d0 = 2;
			break;
		}
	}
	else if(player_stats[player].state == 8)
	{
		player_stats[player].time_to_continue = 300;
	}
	else if(game_mode == 0x100)
	{
		unk_801e09d0 = 2;
	}
}

/* 0x800c6f28: a volume switch closing wakes coin_volume_proc for a while */
void volume_switch_handler(int closed, int swid)
{
	(void)swid;
	if(closed == 1)
	{
		sound_unk6ec4 = 60;
	}
}

/* 0x800c6f44: hold a volume button to change the master volume */
void coin_volume_proc(int *args)
{
	int	sw;
	int	changed;
	(void)args;
	for(;;)
	{
		if(sound_init_pending == 1)
		{
			sound_unk6ec4 = 0;
		}
		if(sound_unk6ec4 > 0)
		{
			changed = 0;
			sw = get_dip_coin_current();
			if(sw & 0x1000)
			{
				sound_unk6ec4 = 60;
				if(sound_state.volume < 255)
				{
					sound_state.volume++;
					changed = 1;
				}
			}
			if(sw & 0x800)
			{
				sound_unk6ec4 = 60;
				if(sound_state.volume > 0)
				{
					sound_state.volume--;
					changed = 1;
				}
			}
			if(changed)
			{
				set_adjustment(3, sound_state.volume);
				if(snd_master_volume(sound_state.volume))
				{
					printf("coin_volume_proc(): ERROR sending master sound_state.volume %d\n", sound_state.volume);
				}
			}
			sound_unk6ec4--;
		}
		sleep(1);
	}
}

/* 0x800c706c: the volume bar while a volume button was recently held */
void show_volume_level(void)
{
	float	level;
	float	x0;

	if(sound_unk6ec4 <= 0)
	{
		return;
	}
	guAlphaSource(0);
	guColorCombineFunction(1);
	level = (float)sound_state.volume / 255.0f * 200.0f;
	x0 = 100.0f;
	grConstantColorValue(0x8000b000);
	draw_hbar(x0, level + x0, 40.0f, 16.0f);
	grConstantColorValue(0x80004000);
	draw_hbar(level + x0, 300.0f, 40.0f, 16.0f);
	graphical_printf(text_buffer, "VOLUME %d", sound_state.volume);
	draw_string_centered(0, text_buffer, 200.0f, 34.0f, 0xffffffff);
}

/* 0x800c718c: coin switch handler: reset the continue timers, add the coin, play the coin sound */
void coin_handler(int closed, int swid)
{
	if(closed != 1)
	{
		return;
	}
	if(player_stats[0].state == 8)
	{
		player_stats[0].time_to_continue = 300;
	}
	if(player_stats[1].state == 8)
	{
		player_stats[1].time_to_continue = 300;
	}
	add_coin(1, swid);
	if(!have_credits_to_start())
	{
		intro_cue_played = 0;
	}
	if(sound_init_pending)
	{
		return;
	}
	if(have_credits_to_start() && !intro_cue_played)
	{
		play_rotating_cue_flags_noint(26, "generic1", 0);
		intro_cue_played = closed;
	}
	else
	{
		play_rotating_cue_flags_noint(26, "generic1", 0);
	}
}

/* 0x800c726c: the service credit button */
void service_credit_handler(int closed, int swid)
{
	int	total;
	(void)swid;
	if(closed != 1 || (get_dip_coin_current() & 0x4000))
	{
		return;
	}
	play_rotating_cue_flags_noint(26, "generic1", 0);
	service_credits++;
	total = credits + service_credits;
	if(live_pricing.max_credits < total)
	{
		service_credits = live_pricing.max_credits - credits;
		coin_units = 0;
		bonus_units = 0;
		set_audit(10, live_pricing.max_credits);
	}
	else
	{
		set_audit(10, total);
		increment_audit(11);
	}
}

/* 0x800c7310: a coin in slot swid */
static void add_coin(int closed, int swid)
{
	int	slot;
	int	sum;
	int	total = 0;

	if(closed != 1 || slam_active)
	{
		return;
	}
	switch(swid)
	{
	case 0: slot = 0; break;
	case 1: slot = 1; break;
	case 7: slot = 2; break;
	case 8: slot = 3; break;
	case 15: slot = 4; break;
	default: slot = 0; break;
	}
	if(coin_counter.mode)
	{
		coin_counter.pending[slot] += coin_counter.pulses[slot];
	}
	else
	{
		coin_counter.pending[slot]++;
	}
	coin_units += live_pricing.slot_units[slot];
	sum = coin_units + bonus_units;
	if(sum >= live_pricing.bonus_threshold && sum >= live_pricing.units_per_credit)
	{
		credits += sum / live_pricing.units_per_credit;
		coin_units = 0;
		bonus_units = sum % live_pricing.units_per_credit;
	}
	bonus_count += live_pricing.slot_units[slot];
	if(live_pricing.bonus_coins && bonus_count && bonus_count >= live_pricing.bonus_coins)
	{
		credits += bonus_count / live_pricing.bonus_coins;
		bonus_count = bonus_count % live_pricing.bonus_coins;
	}
	if(live_pricing.max_credits)
	{
		total = credits + service_credits;
		if(total > live_pricing.max_credits)
		{
			total = live_pricing.max_credits;
			credits = live_pricing.max_credits - service_credits;
			coin_units = 0;
			bonus_units = 0;
		}
	}
	if(set_audit(10, total))
	{
		set_audit(10, total);
	}
}

/* 0x800c7570 */
void init_credits(void)
{
	free_play_func();
	setup_pricing();
	init_credit_counts();
	init_coin_counters();
	something_todo_with_credits();
}

/* 0x800c75b0: pick the pricing record and copy it into the live settings */
static void setup_pricing(void)
{
	unsigned int sw;
	int	continent, country, price;
	unsigned int i;
	int	j;
	int	bad;
	int	*p;
	pricing_record_t *r;

	pricing = 0;
	sw = get_dip_coin_current();
	if((sw & 0x10000) && !get_adjustment(0x2b, &continent) && !get_adjustment(0x2c, &country) && !get_adjustment(0x2d, &price))
	{
		if(continent == 6)
		{
			p = custom_pricing;
			bad = 0;
			for(i = 0; i < 22; i++)
			{
				if(get_adjustment(4 + i, &p[i]))
				{
					bad = 1;
				}
			}
			custom_pricing_strings = malloc(0x50);
			if(!custom_pricing_strings)
			{
				bad = 1;
			}
			else
			{
				p = custom_pricing_strings;
				for(j = 0; j < 20; j++)
				{
					if(get_adjustment(0x17 + j, p))
					{
						free(custom_pricing_strings);
						bad = 1;
					}
					p++;
				}
			}
			if(bad)
			{
				pricing = 0;
			}
			else
			{
				pricing = (pricing_record_t *)custom_pricing;
			}
		}
		else
		{
			pricing = pricing_continents[continent][country];
			pricing += price;
		}
	}
	if(!pricing)
	{
		pricing_from_dips = 1;
		pricing = pricing_by_dip[(sw >> 17) & 0x1f];
	}
	r = pricing;
	live_pricing.slot_units[0] = r->slot_units[0];
	live_pricing.slot_units[1] = r->slot_units[1];
	live_pricing.slot_units[2] = r->slot_units[2];
	live_pricing.slot_units[3] = r->slot_units[3];
	live_pricing.slot_units[4] = r->slot_units[4];
	live_pricing.units_per_credit = r->units_per_credit;
	live_pricing.bonus_coins = r->bonus_coins;
	live_pricing.bonus_threshold = r->bonus_threshold;
	live_pricing.credits_to_start = r->credits_to_start;
	live_pricing.credits_to_continue = r->credits_to_continue;
	live_pricing.max_credits = r->max_credits;
	live_pricing.unk2c = r->unk2c;
	live_pricing.show_fraction = r->show_fraction;
	if(r->currency == 0)
	{
		strcpy(live_pricing.symbol, "$");
		live_pricing.name[0] = 0;
	}
	else if(r->currency == 1)
	{
		strcpy(live_pricing.symbol, "L");
		live_pricing.name[0] = 0;
	}
	else
	{
		live_pricing.symbol[0] = 0;
		strcpy(live_pricing.name, currency_names[r->currency]);
	}
	strcpy(live_pricing.text, pricing->text);
}

/* 0x800c78b0: free play from the dip switch or the adjustment */
void free_play_func(void)
{
	int	adj;
	int	sw;
	int	dip;

	is_freeplay = 0;
	sw = get_dip_coin_current();
	dip = sw & 0x10000;
	if(!dip || get_adjustment(0, &adj))
	{
		fprintf(stderr, "Setting free play from dip\r\n");
		if(!dip)
		{
			fprintf(stderr, "Because dip is set\r\n");
		}
		if((sw & 0x3e0000) == 0x3e0000)
		{
			fprintf(stderr, "Enabling free play\r\n");
			is_freeplay = 1;
		}
	}
	else if(adj)
	{
		is_freeplay = 1;
	}
	slam_active = 0;
	coin_lock_unused = 1;
	unk_801e097c = 0;
	unk_801e0980 = 0;
}

/* 0x800c79a8: credits from the audit, counters cleared */
static void init_credit_counts(void)
{
	if(get_audit(10, &credits))
	{
		credits = 0;
	}
	else if(credits < 0)
	{
		set_audit(10, 0);
		credits = 0;
	}
	coin_units = 0;
	bonus_count = 0;
	bonus_units = 0;
}

/* 0x800c7a14 */
static void init_coin_counters(void)
{
	int	i;

	for(i = 4; i >= 0; i--)
	{
		coin_counter.pending[i] = 0;
	}
	coin_counter.pulses[0] = pricing->counter_pulses[0];
	coin_counter.pulses[1] = pricing->counter_pulses[1];
	coin_counter.pulses[2] = pricing->counter_pulses[2];
	coin_counter.pulses[3] = pricing->counter_pulses[3];
	coin_counter.pulses[4] = pricing->counter_pulses[4];
	coin_counter.enabled[0] = 1;
	coin_counter.enabled[1] = 1;
	coin_counter.enabled[2] = 1;
	coin_counter.enabled[3] = 1;
	coin_counter.enabled[4] = 1;
	coin_counter.mode = 1;
	coin_counter.unk = 0;
}

void dispatch_slam_proc(int closed, int swid)
{
	(void)swid;
	if(closed == 1 && slam_active != 1)
	{
		slam_active = 1;
		qcreate("slam", 0x8000, slam_proc, 0, 0, 0, 0);
	}
}

/* 0x800c7ab8 / 0x800c7b14: slam switch: lock coins out for a second */
static void slam_proc(int *args)
{
	(void)args;
	sleep(60);
	slam_active = 0;
}

/* 0x800c7b3c: the "ccproc" process drains the pending coin-meter pulses */
void ccproc_proc(int *args)
{
	int	i;
	(void)args;
	for(;;)
	{
		sleep(1);
		for(i = 0; i < 5; i++)
		{
			if(coin_counter.pending[i])
			{
				increment_audit(i);
				increment_audit(5);
				pulse_coin_counter(coin_counter.unk == 1 ? 1 : coin_counter.enabled[i]);
				coin_counter.pending[i]--;
			}
		}
	}
}

/* 0x800c7c04: one pulse on the coin meter output */
static void pulse_coin_counter(int bits)
{
	_ioctl(4, 0x6905, 0x10);
	_ioctl(4, 0x6905, bits);
	sleep(2);
	_ioctl(4, 0x6905, 0x10);
	_ioctl(4, 0x6905, 0);
	sleep(3);
}

/* 0x800c7c78 */
void clear_bonus_count(void)
{
	bonus_count = 0;
}

/* 0x800c7c88, 0x800c7cec, 0x800c7d38, 0x800c7d74, 0x800c7d84, 0x800c817c, 0x800c818c */
INLINE int have_credits_to_start(void)
{
	if((debug_mode & DEBUG_ON) || check_for_freeplay())
	{
		return(1);
	}
	return(credits + service_credits >= live_pricing.credits_to_start);
}

INLINE int have_credits_to_continue(void)
{
	if(check_for_freeplay() == 0)
	{
		return(credits + service_credits >= live_pricing.credits_to_continue);
	}
	return(1);
}

int credits_available(void)
{
	if(check_for_freeplay())
	{
		return(30);
	}
	return(credits + service_credits);
}

int check_for_freeplay(void)
{
	return(is_freeplay);
}

int any_credits(void)
{
	if(coin_units > 0 || bonus_units > 0 || credits > 0 || service_credits > 0)
	{
		return(1);
	}
	return(0);
}

/* 0x800c7dd0: the two credit lines shown on the attract screens */
void credit_info(int mode)
{
	int	total, bonus, upc;
	int	needed;

	if(is_freeplay)
	{
		graphical_printf(credit_line1, "FREE PLAY");
		graphical_printf(credit_line2, "PRESS START");
		return;
	}
	total = credits + service_credits;
	bonus = coin_units + bonus_units;
	upc = live_pricing.units_per_credit;
	if(mode == 1)
	{
		if(have_credits_to_start())
		{
			graphical_printf(credit_line2, "PRESS START");
		}
		else
		{
			needed = check_credits_to_start_mode() - total;
			if(needed == 1)
			{
				graphical_printf(credit_line2, "INSERT COIN");
			}
			else
			{
				graphical_printf(credit_line2, "INSERT %d COINS", needed);
			}
		}
	}
	else
	{
		if(have_credits_to_continue())
		{
			graphical_printf(credit_line2, "PRESS START");
		}
		else
		{
			needed = check_credits_to_continue_mode() - total;
			if(needed == 1)
			{
				graphical_printf(credit_line2, "INSERT COIN");
			}
			else
			{
				graphical_printf(credit_line2, "INSERT %d COINS", needed);
			}
		}
	}
	if(live_pricing.show_fraction && bonus)
	{
		if(total == 0)
		{
			graphical_printf(credit_line1, "CREDITS: %d/%d", bonus, upc);
		}
		else
		{
			graphical_printf(credit_line1, "CREDITS: %d  %d/%d", total, bonus, upc);
		}
		return;
	}
	if(total >= live_pricing.max_credits)
	{
		graphical_printf(credit_line1, "CREDITS: %d (MAX)", total);
	}
	else if(mode == 1)
	{
		if(have_credits_to_start())
		{
			graphical_printf(credit_line1, "CREDITS: %d", total);
		}
		else
		{
			graphical_printf(credit_line1, "CREDITS: %d OF %d", total, check_credits_to_start_mode());
		}
	}
	else
	{
		if(have_credits_to_continue())
		{
			graphical_printf(credit_line1, "CREDITS: %d", total);
		}
		else
		{
			graphical_printf(credit_line1, "CREDITS: %d OF %d", total, check_credits_to_continue_mode());
		}
	}
}

/* 0x800c811c: pay for a start or a continue */
void take_credits(short n)
{
	if(is_freeplay)
	{
		credits = 0;
	}
	else
	{
		credits -= n;
	}
	set_audit(10, credits);
}

int check_credits_to_start_mode(void)
{
	return(live_pricing.credits_to_start);
}

int check_credits_to_continue_mode(void)
{
	return(live_pricing.credits_to_continue);
}

/* 0x800dacb0: a flat horizontal bar from x0 to x1, h high, centred on y (two triangles) */
typedef struct { float x, y, z, pad; } flat_vertex_t;
void draw_hbar(float x0, float x1, float y, float h)
{
	flat_vertex_t	a, b, c, d;
	float	half, top, bottom;

	half = h * 0.5f;
	top = y - half;
	bottom = y + half;
	a.x = x0; a.y = top; a.z = 0.9090909f;
	b.x = x1; b.y = top; b.z = 0.9090909f;
	c.x = x0; c.y = bottom; c.z = 0.9090909f;
	d.x = x1; d.y = bottom; d.z = 0.9090909f;
	draw_tri_flat(&a, &b, &d);
	draw_tri_flat(&a, &d, &c);
}

