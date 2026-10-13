/*
 * player.c - the players' records (0x800e2d48-0x800e5xxx: the start, the continue, the HUD, the
 * high-score entry). Only the start and the boot-time set-up are in C so far.
 */
#include "credits.h"			/* player_stats */
#include "waves.h"

extern int	increment_audit(int);
extern void	func_800d5f34(player_stats_t *, int);
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
	p->unk108 = 0;
	p->unk68 = 0;
	p->unk58 = 0;
	p->unk5c = 0;
	p->unk60 = 0;
	p->unk104 = 6;
	func_800d5f34(p, 1);
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
		p->unk8 = 0;
		p->unk1c = 0;
		p->unk18 = 0;
		p->unk20 = 0;
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
	p->unk24 = p->unk28 = (float)(1300 - D_801de8a4 * 200);
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
	p->unk58 = 0;
	p->unk5c = 0;
	p->unk10c = 0;
	p->unk110 = 0;
	p->unk114 = 0;
	p->unk118 = 0;
	p->unk11c = 0;
	p->unk70 = 128.0f;
}
