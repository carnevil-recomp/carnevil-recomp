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
extern int	game_state[4];						/* credits.h: [0] is the phase */
extern void	start_shake(int);
extern float	func_800fe8dc(float range);				/* a random float in +-range */					/* shake.c */
extern void	kill_player(player_stats_t *);				/* endgame.c */
extern int	printf(const char *, ...);
#include "levelsnd.h"
#include "sprites.h"
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

int	hurt_cue_count;			/* 0x8017ddf0: the hurt cues alternate 0xa2/0xa3 */

/* 0x800e2a34: an enemy's attack lands: the damage comes off the health (a dead player goes
   to the continue countdown), the screen shakes, a hurt cue plays and the attack's hit mark
   (a screen sprite, kinds 6-15) appears over the player's side. kind 6 scatters its mark by
   param and plays no cue; 1 and 7 draw it mirrored when param is set; 5 only shakes and is
   the one attack that lands during the grace after a continue. */
void hurt_player(player_stats_t *p, int damage, int kind, float param)
{
	if(game_state[0] != 2)
	{
		return;
	}
	if(p->state != 4)
	{
		return;
	}
	if(p->unkd4 > 0)
	{
		if(kind != 5)
		{
			return;
		}
	}
	p->health = p->health - (float)damage;
	if(p->health <= 0.0f)
	{
		kill_player(p);
	}
	switch(kind)
	{
	case 6:
		add_screen_sprite(p->unk6c + param, func_800fe8dc(10.0f) + 128.0f, 11, 0);
		start_shake(1);
		break;
	case 5:
		start_shake(1);
		break;
	case 8:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		break;
	case 4:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite(p->unk6c, 128.0f, 9, 0);
		break;
	case 1:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite_mirror(p->unk6c, 128.0f, 7, 0, param);
		break;
	case 7:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite_mirror(p->unk6c, 128.0f, 8, 0, param);
		break;
	case 0:
	case 3:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite(p->unk6c, 128.0f, 6, 0);
		break;
	case 9:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite(p->unk6c, 128.0f, 12, 0);
		break;
	case 10:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite(p->unk6c, 128.0f, 13, 0);
		break;
	case 11:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite(p->unk6c, 128.0f, 14, 0);
		break;
	case 12:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite(p->unk6c, 128.0f, 15, 0);
		break;
	default:
		printf("Unknown hurt in decrement_health\n");
		break;
	}
}
