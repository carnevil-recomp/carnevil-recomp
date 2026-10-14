/*
 * player.c - the players' records (0x800e2d48-0x800e5xxx: the start, the continue, the HUD, the
 * high-score entry). Only the start and the boot-time set-up are in C so far.
 */
#include "credits.h"			/* player_stats */
#include "waves.h"

extern int	increment_audit(int);
extern void	set_weapon(player_stats_t *, int);
extern void	set_player_gun(player_stats_t *);			/* 0x800d6718 */
extern int	loaded_wave, D_801facc8;
extern int	D_801de8a4;						/* adjustment 0x31 (adjust.c) */

/* 0x800e4bc0 */
void set_level_start_time(player_stats_t *p)
{
	p->level_start_time = D_801facc8;
}

/* 0x800e2d48: a player starts or continues: the audits (starts, two-player games, continues
   per level and in total), the counters, then state 4 with the difficulty's health */
void init_player_stats(int player)
{
	player_stats_t	*p, *other;

	other = player_stats;
	p = other + player;
	if(player == 0)
	{
		increment_audit(0x1f);
		other++;
	}
	else
	{
		increment_audit(0x20);
	}
	p->clicks = 0;
	p->autofire = 0;
	p->shot_pending = 0;
	p->offscreen_timer = 0;
	p->trigger_was = 0;
	p->pistol_ammo = 6;
	set_weapon(p, 1);
	p->unk10c = 0;
	increment_audit(0x21);
	if(other->state == 4)
	{
		increment_audit(0x1d);
	}
	else
	{
		increment_audit(0x1e);
	}
	if(p->state != 8)
	{
		p->score = 0;
		p->hits = 0;
		p->shots = 0;
		p->accuracy = 0;
		p->continues = 0;
		increment_audit(0xc);
		p->unkd4 = 0;
	}
	else
	{
		switch(loaded_wave)
		{
		case WAVE_HHOUSE:
			increment_audit(0x58);
			break;
		case WAVE_FREAKSHOW:
			increment_audit(0x5d);
			break;
		case WAVE_RICKITY:
			increment_audit(0x62);
			break;
		case WAVE_BIGTOP:
			increment_audit(0x67);
			break;
		}
		set_level_start_time(p);
		p->continues++;
		increment_audit(0xd);
		if(p->continues < 15)
		{
			increment_audit(p->continues + 0xd);
		}
		else
		{
			increment_audit(0x1c);
		}
		p->unkd4 = 100;
	}
	p->state = 4;
	p->health = p->max_health = (float)(1300 - D_801de8a4 * 200);
	p->start_time = D_801facc8;
}

/* 0x800e2f08: a player's constants at boot: index, gun, colour, sound priorities, screen position */
void init_player(int player)
{
	player_stats_t	*p;

	p = &player_stats[player];
	p->index = player;
	set_player_gun(p);
	if(p->index == 0)
	{
		p->volume = 10;
		p->colour = 0xff00;
		p->priority = 0x80c1;
		p->unk6c = 100.0f;
		p->priority2 = 0x80c2;
	}
	else
	{
		p->volume = 0xf5;
		p->colour = 0xff00ff;
		p->priority = 0x8141;
		p->unk6c = 300.0f;
		p->priority2 = 0x8142;
	}
	p->unk7c = 0xff;
	p->unk80 = 0xff;
	p->shot_pending = 0;
	p->offscreen_timer = 0;
	p->unk10c = 0;
	p->unk110 = 0;
	p->unk114 = 0;
	p->unk118 = 0;
	p->unk11c = 0;
	p->unk70 = 128.0f;
}

/* 0x800e2958: points for a player, scaled by the accuracy so far, in tens */
void award_points(int player, int points)
{
	player_stats_t *p;
	float	f;

	p = &player_stats[player];
	f = (float)points;
	if(p->shots > 0)
	{
		f = f * p->accuracy;
	}
	p->score = p->score + (int)f * 10;
}

/* 0x800e29b0, 0x800e29bc: the damage of the shot being resolved, and the same scaled down
   (1.350000023841858) unless both players are in the game */
float shot_damage_of(player_stats_t *p)
{
	return(p->shot_damage);
}

float shot_damage_shared(player_stats_t *p)
{
	if((player_stats[0].state & 4) && (player_stats[1].state & 4))
	{
		return(p->shot_damage);
	}
	return(p->shot_damage * 1.350000023841858f);
}

/* 0x800e2a08: health back, up to the maximum */
void heal_player(player_stats_t *p, float amount)
{
	float	h;

	h = p->health + amount;
	if(p->max_health < h)
	{
		p->health = h;
		p->health = p->max_health;
	}
	else
	{
		p->health = h;
	}
}
