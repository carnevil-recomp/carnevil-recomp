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

extern int	get_adjustment(int, int *), set_adjustment(int, int);
extern int	get_audit(int, int *), set_audit(int, int), increment_audit(int);
extern int	get_dip_coin_current(void);
extern void	sleep(int);
extern void	*qcreate(char *, int, void (*)(), int, int, int, int);
extern int	snd_master_volume(int);
extern void	play_level_cue_pri(int, char *, int);		/* 0x800ebfa8 */
extern void	graphical_printf(char *, char *, ...);
extern int	_ioctl(int, int, int);
extern void	init_player_stats(int);				/* 0x800e2d48 */
extern void	init_main_game_proc(int);			/* 0x800c5748 */
extern int	debug_mode;					/* 0x80212e20 */
extern int	sound_init_pending, volume, sound_unk6ec4;	/* 0x801e6ec0, 0x801e6ec8, 0x801e6ec4 */
extern FILE	*__dj_stderr;
extern void	guAlphaSource(int), guColorCombineFunction(int), grConstantColorValue(unsigned int);
extern void	grDepthBufferFunction(int), grDepthMask(int);
extern void	draw_string_centered(int, char *, float, float, unsigned int);	/* 0x800ddf74 (add_to_print_) */
extern void	draw_tri_flat(void *, void *, void *);				/* 0x800d2174 */
extern char	text_buffer[];							/* 0x801e7100 */

/* credit state */
int		credits;				/* 0x801de8d8 */
int		service_credits;			/* 0x801de8dc */
int		coin_units;				/* 0x801de8e0 units towards the next credit */
int		bonus_count;				/* 0x801de8e4 coins towards the bonus credit */
int		bonus_units;				/* 0x801de8e8 */
int		slam_active;				/* 0x801e0970 */
int		coin_lock_unused;			/* 0x801e0974 */
int		is_freeplay;				/* 0x801e0978 */
int		unk_801e097c, unk_801e0980;
int		intro_cue_played;			/* 0x80179b58 */
int		pricing_from_dips;			/* 0x80237274 */
int		unk_80212e1c;
int		unk_801e09d0;

/* the selected pricing (credit_info_struct_2, 0x801e5cf8) */
int		slot_units[5];				/* 0x801e5cf8 */
int		units_per_credit;			/* 0x801e5d0c */
int		bonus_coins;				/* 0x801e5d10 */
int		bonus_threshold;			/* 0x801e5d14 */
int		coins_needed_to_play;			/* 0x801e5d18 */
int		num_credits_to_continue;		/* 0x801e5d1c */
int		max_credits;				/* 0x801e5d20 */
int		pricing_unk2c;				/* 0x801e5d24 */
int		show_credit_fraction;			/* 0x801e5d28 */
char		currency_symbol[8];			/* 0x801e5d2c */
char		currency_name[8];			/* 0x801e5d34 */
char		pricing_text[256];			/* 0x801e5d3c */
pricing_record_t *pricing;				/* 0x801a4350 (credit_info_struct_1) */
int		custom_pricing[22];			/* 0x801a4360: record built from adjustments 4..25 */
int		*custom_pricing_strings;		/* 0x801a43ac */

/* coin counters (0x80237458..0x80237498) */
int		counter_pulses[5];			/* 0x80237458 */
int		counter_enabled[5];			/* 0x8023746c */
int		counter_pending[5];			/* 0x80237480 */
int		counter_mode;				/* 0x80237494 */
int		counter_unk;				/* 0x80237498 */

char		credit_line1[128];			/* 0x801e6ee8 */
char		credit_line2[128];			/* 0x801e6f68 */
player_stats_t	player_stats[2];			/* 0x801e0f64 */
int		game_mode;				/* 0x80212e14 */

/* 0x800c6a64: split the pricing text into the three attract lines, upper-cased */
extern char	attract_text1[], attract_text2[], attract_text3[];
extern int	attract_text_state;
static void copy_line(char **src, char *dst)
{
	char	*s = *src;
	while(*s && *s != '\n')
	{
		*dst++ = *s++;
	}
	*dst = 0;
	*src = s;
}
static void upcase(char *s)
{
	for(; *s; s++)
	{
		if((unsigned char)(*s - 'a') < 26)
		{
			*s -= 0x20;
		}
	}
}
void something_todo_with_credits(void)
{
	char	*s = pricing_text;

	copy_line(&s, attract_text1);
	upcase(attract_text1);
	if(!*s++)
	{
		attract_text_state = 1;
		return;
	}
	copy_line(&s, attract_text2);
	upcase(attract_text2);
	if(!*s++)
	{
		attract_text_state = 2;
		return;
	}
	copy_line(&s, attract_text3);
	upcase(attract_text3);
	attract_text_state = 3;
}

/* 0x800c7c88, 0x800c7cec, 0x800c7d38, 0x800c7d74, 0x800c7d84, 0x800c817c, 0x800c818c */
int have_credits_to_start(void)
{
	if(debug_mode & 1)
	{
		return(1);
	}
	if(check_for_freeplay())
	{
		return(1);
	}
	return(credits + service_credits >= coins_needed_to_play);
}
int have_credits_to_continue(void)
{
	if(check_for_freeplay())
	{
		return(1);
	}
	return(credits + service_credits >= num_credits_to_continue);
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
	return(coin_units > 0 || bonus_units > 0 || credits > 0 || service_credits > 0);
}
int check_credits_to_start_mode(void)
{
	return(coins_needed_to_play);
}
int check_credits_to_continue_mode(void)
{
	return(num_credits_to_continue);
}

/* 0x800c6c34: can this player start (or continue) with what is in the machine? */
int player_can_start(int player)
{
	int	state = player_stats[player].state;

	if(state == 0x20 || state == 0)
	{
		return(have_credits_to_start());
	}
	if(state == 8)
	{
		return(have_credits_to_continue());
	}
	return(0);
}

/* 0x800c6ca8 */
void inc_80212e1c(void)
{
	unk_80212e1c++;
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
	int	player = (args[0] == 5);
	int	mode;

	if(player_can_start(player))
	{
		mode = game_mode;
		if(mode == 1 || mode == 4)
		{
			take_credits(player_stats[player].state == 8 ? check_credits_to_continue_mode() : check_credits_to_start_mode());
			init_player_stats(player);
			init_main_game_proc(player);
		}
		else if(mode == 2 || mode == 0x10 || mode == 0x20 || mode == 0x80)
		{
			take_credits(player_stats[player].state == 8 ? check_credits_to_continue_mode() : check_credits_to_start_mode());
			init_player_stats(player);
		}
		else if(mode == 0x100)
		{
			unk_801e09d0 = 2;
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
			sw = get_dip_coin_current();
			changed = 0;
			if(sw & 0x1000)
			{
				sound_unk6ec4 = 60;
				if(volume < 255)
				{
					volume++;
					changed = 1;
				}
			}
			if(sw & 0x800)
			{
				sound_unk6ec4 = 60;
				if(volume > 0)
				{
					volume--;
					changed = 1;
				}
			}
			if(changed)
			{
				set_adjustment(3, volume);
				if(snd_master_volume(volume))
				{
					printf("coin_volume_proc(): ERROR sending master volume %d\n", volume);
				}
			}
			sound_unk6ec4--;
		}
		sleep(1);
	}
}

/* 0x800c7310: a coin in slot swid */
static void add_coin(int closed, int swid)
{
	int	slot;
	int	total;

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
	if(counter_mode)
	{
		counter_pending[slot] += counter_pulses[slot];
	}
	else
	{
		counter_pending[slot]++;
	}
	coin_units += slot_units[slot];
	total = coin_units + bonus_units;
	if(total >= bonus_threshold && total >= units_per_credit)
	{
		credits += total / units_per_credit;
		coin_units = 0;
		bonus_units = total % units_per_credit;
	}
	bonus_count += slot_units[slot];
	if(bonus_coins && bonus_count && bonus_count >= bonus_coins)
	{
		credits += bonus_count / bonus_coins;
		bonus_count = bonus_count % bonus_coins;
	}
	total = credits;
	if(max_credits)
	{
		total = credits + service_credits;
		if(total > max_credits)
		{
			total = max_credits;
			credits = max_credits - service_credits;
			coin_units = 0;
			bonus_units = 0;
		}
	}
	if(set_audit(10, total))
	{
		set_audit(10, total);
	}
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
		play_level_cue_pri(26, "generic1", 0);
		intro_cue_played = closed;
	}
	else
	{
		play_level_cue_pri(26, "generic1", 0);
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
	play_level_cue_pri(26, "generic1", 0);
	service_credits++;
	total = credits + service_credits;
	if(max_credits < total)
	{
		service_credits = max_credits - credits;
		coin_units = 0;
		bonus_units = 0;
		set_audit(10, max_credits);
	}
	else
	{
		set_audit(10, total);
		increment_audit(11);
	}
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
		fprintf(__dj_stderr, "Setting free play from dip\r\n");
		if(!dip)
		{
			fprintf(__dj_stderr, "Because dip is set\r\n");
		}
		if((sw & 0x3e0000) == 0x3e0000)
		{
			fprintf(__dj_stderr, "Enabling free play\r\n");
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

/* 0x800c75b0: pick the pricing record and copy it into the live settings */
static void setup_pricing(void)
{
	int	sw;
	int	continent, country, price;
	int	i, bad;
	pricing_record_t *r;

	pricing = 0;
	sw = get_dip_coin_current();
	if((sw & 0x10000) && !get_adjustment(0x2b, &continent) && !get_adjustment(0x2c, &country) && !get_adjustment(0x2d, &price))
	{
		if(continent == 6)
		{
			bad = 0;
			for(i = 0; i < 22; i++)
			{
				if(get_adjustment(4 + i, &custom_pricing[i]))
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
				for(i = 0; i < 20; i++)
				{
					if(get_adjustment(0x17 + i, &custom_pricing_strings[i]))
					{
						free(custom_pricing_strings);
						bad = 1;
					}
				}
			}
			pricing = bad ? 0 : (pricing_record_t *)custom_pricing;
		}
		else
		{
			pricing = pricing_continents[continent][country] + price;
		}
	}
	if(!pricing)
	{
		pricing = pricing_by_dip[(sw >> 17) & 0x1f];
		pricing_from_dips = 1;
	}
	r = pricing;
	for(i = 0; i < 5; i++)
	{
		slot_units[i] = r->slot_units[i];
	}
	units_per_credit = r->units_per_credit;
	bonus_coins = r->bonus_coins;
	bonus_threshold = r->bonus_threshold;
	coins_needed_to_play = r->credits_to_start;
	num_credits_to_continue = r->credits_to_continue;
	max_credits = r->max_credits;
	pricing_unk2c = r->unk2c;
	show_credit_fraction = r->show_fraction;
	if(r->currency == 0)
	{
		strcpy(currency_symbol, "$");
		currency_name[0] = 0;
	}
	else if(r->currency == 1)
	{
		strcpy(currency_symbol, "L");
		currency_name[0] = 0;
	}
	else
	{
		currency_symbol[0] = 0;
		strcpy(currency_name, currency_names[r->currency]);
	}
	strcpy(pricing_text, pricing->text);
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

	for(i = 0; i < 5; i++)
	{
		counter_pending[i] = 0;
	}
	for(i = 0; i < 5; i++)
	{
		counter_pulses[i] = pricing->counter_pulses[i];
		counter_enabled[i] = 1;
	}
	counter_mode = 1;
	counter_unk = 0;
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

/* 0x800c7ab8 / 0x800c7b14: slam switch: lock coins out for a second */
static void slam_proc(int *args)
{
	(void)args;
	sleep(60);
	slam_active = 0;
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
			if(counter_pending[i])
			{
				increment_audit(i);
				increment_audit(5);
				pulse_coin_counter(counter_enabled[4] == 1 ? 1 : counter_enabled[i]);
				counter_pending[i]--;
			}
		}
	}
}

/* 0x800c7c78 */
void clear_bonus_count(void)
{
	bonus_count = 0;
}

/* 0x800c7dd0: the two credit lines shown on the attract screens */
void credit_info(int mode)
{
	int	total, bonus;

	if(is_freeplay)
	{
		graphical_printf(credit_line1, "FREE PLAY");
		graphical_printf(credit_line2, "PRESS START");
		return;
	}
	total = credits + service_credits;
	bonus = coin_units + bonus_units;
	if(mode == 1)
	{
		if(have_credits_to_start())
		{
			graphical_printf(credit_line2, "PRESS START");
		}
		else if(check_credits_to_start_mode() - total == 1)
		{
			graphical_printf(credit_line2, "INSERT COIN");
		}
		else
		{
			graphical_printf(credit_line2, "INSERT %d COINS", check_credits_to_start_mode() - total);
		}
	}
	else
	{
		if(have_credits_to_continue())
		{
			graphical_printf(credit_line2, "PRESS START");
		}
		else if(check_credits_to_continue_mode() - total == 1)
		{
			graphical_printf(credit_line2, "INSERT COIN");
		}
		else
		{
			graphical_printf(credit_line2, "INSERT %d COINS", check_credits_to_continue_mode() - total);
		}
	}
	if(show_credit_fraction && bonus)
	{
		if(total)
		{
			graphical_printf(credit_line1, "CREDITS: %d  %d/%d", total, bonus, units_per_credit);
		}
		else
		{
			graphical_printf(credit_line1, "CREDITS: %d/%d", bonus, units_per_credit);
		}
		return;
	}
	if(total >= max_credits)
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

/* 0x800dacb0: a flat horizontal bar from x0 to x1, h high, centred on y (two triangles) */
typedef struct { float x, y, z, pad; } flat_vertex_t;
void draw_hbar(float x0, float x1, float y, float h)
{
	flat_vertex_t	a, b, c, d;

	a.x = x0; a.y = y - h * 0.5f; a.z = 0.9090909f;
	b.x = x1; b.y = y - h * 0.5f; b.z = 0.9090909f;
	c.x = x0; c.y = y + h * 0.5f; c.z = 0.9090909f;
	d.x = x1; d.y = y + h * 0.5f; d.z = 0.9090909f;
	draw_tri_flat(&a, &b, &d);
	draw_tri_flat(&a, &d, &c);
}

/* 0x800c706c: the volume bar while a volume button was recently held */
void show_volume_level(void)
{
	float	level;

	if(sound_unk6ec4 <= 0)
	{
		return;
	}
	guAlphaSource(0);
	guColorCombineFunction(1);
	level = (float)volume / 255.0f * 200.0f + 100.0f;
	grConstantColorValue(0x8000b000);
	draw_hbar(100.0f, level, 40.0f, 16.0f);
	grConstantColorValue(0x80004000);
	draw_hbar(level, 300.0f, 40.0f, 16.0f);
	graphical_printf(text_buffer, "VOLUME %d", volume);
	draw_string_centered(0, text_buffer, 200.0f, 34.0f, 0xffffffff);
}

/* 0x800cbab4: the attract-mode credit lines, alternating every 50 frames */
int	credit_line_timer;				/* 0x80179d3c */
void attract_credit_lines(void)
{
	grDepthBufferFunction(7);
	grDepthMask(0);
	credit_info(1);
	credit_line_timer += 4;
	if(credit_line_timer < 50)
	{
		draw_string_centered(1, credit_line1, 200.0f, 5.0f, 0x00ffffff);
	}
	else
	{
		draw_string_centered(1, credit_line2, 200.0f, 5.0f, 0x00ffffff);
	}
	if(credit_line_timer >= 101)
	{
		credit_line_timer = 0;
	}
	grDepthMask(1);
	grDepthBufferFunction(3);
}
