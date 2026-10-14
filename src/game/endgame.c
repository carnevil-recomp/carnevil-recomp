/*
 * endgame.c - the end of a player and of a level (part of the file at 0x800e3490-0x800e4eac:
 * end_of_game, end_of_wave, the game-over and high-score screens are not decompiled yet).
 */
#include "credits.h"
#include "waves.h"

extern int	D_801facc8;				/* vblanks since the game started */
extern int	loaded_wave;
extern void	average_audit(int count_audit, int mean_audit, int value);	/* adjust.c */
extern int	snd_stop_track(unsigned int);

/* 0x800e4bd0: the time the player spent in the level goes into its average audit */
void audit_level_time(player_stats_t *p)
{
	int	now, value, wave;

	now = D_801facc8;
	value = now - p->level_start_time;
	p->unk3c = now;
	wave = loaded_wave;
	switch(wave)
	{
	case WAVE_HHOUSE:
		average_audit(0x57, 0x56, value);
		break;
	case WAVE_FREAKSHOW:
		average_audit(0x5c, 0x5b, value);
		break;
	case WAVE_RICKITY:
		average_audit(0x61, 0x60, value);
		break;
	case WAVE_BIGTOP:
		average_audit(0x66, 0x65, value);
		break;
	}
}

/* 0x800e3c74: the player's health ran out: audit the time played, wait to continue (300
   frames), and stop the player's own music track */
void kill_player(player_stats_t *p)
{
	int	now, track;

	now = D_801facc8;
	p->unk34 = now;
	average_audit(0x6b, 0x6a, now - p->start_time);
	audit_level_time(p);
	p->state = 8;
	p->clicks = 0;
	p->health = 0.0f;
	p->time_to_continue = 300;
	track = 2;
	if(p->index)
	{
		track = 4;
	}
	snd_stop_track(track);
}
