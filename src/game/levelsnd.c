/*
 * levelsnd.c - the level sound helpers: one original file, 0x800ebb18-0x800ec53c (text) with
 * its fade state in .data at 0x8018e038. See levelsnd.h.
 */
#include <stdio.h>
#include "levelsnd.h"
#include "credits.h"			/* game_phase (0x80212e14) */

extern int	snd_scall_bank(char *, unsigned int, unsigned int, unsigned int, unsigned int);
extern int	snd_stop_track(unsigned int);
extern int	snd_stop_all(void);
extern int	snd_track_volume(int, int);
extern int	snd_stream_volume(int);
extern int	get_adjustment(int, int *);
extern void	stream_stop(void);			/* 0x8013fb84 */

int	track_volume = 0xff;		/* 0x8018e038: the music track's volume */
int	track_fade = 0;			/* 0x8018e03c: 1 fading in, 2 fading out */
int	stream_volume = 0xaa;		/* 0x8018e040: the PCM stream's volume */
int	stream_fade = 0;		/* 0x8018e044 */
int	level_sound_unk048 = 0;		/* 0x8018e048: set by the enemy code (0x800eb150, game_proc2) */
int	cue_priority_rotor = 0;		/* 0x8018e04c */

/* 0x800ebb18 */
void reset_stream_volume(void)
{
	stream_volume = 0xaa;
	snd_stream_volume(0xaa);
}

/* 0x800ebb44, 0x800ebb58 */
void fade_out_level_stream(void)
{
	stream_fade = 2;
}
void fade_in_level_stream(void)
{
	stream_fade = 1;
}

/* 0x800ebb6c: once per frame, step the music and stream fades */
void level_sound_fades(void)
{
	if(track_fade == 1)
	{
		track_volume += 5;
		if(track_volume >= 0x100)
		{
			track_volume = 0xff;
			track_fade = 0;
		}
	}
	else if(track_fade == 2)
	{
		track_volume -= 5;
		if(track_volume < 0)
		{
			track_volume = 0;
			track_fade = 0;
			snd_stop_track(1);
		}
	}
	else
	{
		goto stream;
	}
	snd_track_volume(1, track_volume);
stream:
	if(stream_fade == 1)
	{
		stream_volume += 5;
		if(stream_volume >= 0xab)
		{
			stream_volume = 0xaa;
			stream_fade = 0;
		}
	}
	else if(stream_fade == 2)
	{
		stream_volume -= 5;
		if(stream_volume < 0)
		{
			stream_volume = 0;
			stream_fade = 0;
		}
	}
	else
	{
		return;
	}
	snd_stream_volume(stream_volume);
}

/* 0x800ebca4 */
void clean_up_level_sounds(void)
{
	track_volume = 0xff;
	track_fade = 0;
	stream_volume = 0xaa;
	stream_fade = 0;
	stop_level_cue_and_stream();
	snd_stop_all();
}

/* 0x800ebcf4, 0x800ebd30: an enemy's sound n at its own priority (noint: no interrupt) */
void play_enemy_sound(snd_object_t *obj, int scall, char *bank, int n)
{
	snd_scall_bank(bank, scall, 0xe6, 0x80, (obj->prio_base - n) | obj->prio_flags);
}
void play_enemy_sound_noint(snd_object_t *obj, int scall, char *bank, int n)
{
	snd_scall_bank(bank, scall, 0xe6, 0x80, ((obj->prio_base - n) | obj->prio_flags) & 0xdfff);
}

/* 0x800ebd70: start the music silent and fade it in */
void start_level_music(int scall, char *bank)
{
	snd_scall_bank(bank, scall, 0, 0x80, 0x8040);
	track_volume = 0;
	track_fade = 1;
}

/* 0x800ebdbc: the music at full volume */
void play_level_music(int scall, char *bank)
{
	if(track_volume != 0xff)
	{
		track_volume = 0xff;
		track_fade = 0;
		snd_track_volume(1, 0xff);
	}
	snd_scall_bank(bank, scall, 0xe6, 0x80, 0x8040);
}

/* 0x800ebe34 */
void play_level_speech(int scall, char *bank)
{
	snd_scall_bank(bank, scall, 0xe6, 0x80, 0x82a0);
}

/* 0x800ebe6c, 0x800ebe8c */
void stop_level_music(void)
{
	snd_stop_track(1);
}
void fade_out_level_music(void)
{
	track_fade = 2;
}

/* 0x800ebea0, 0x800ebef4: speech through the three rotating priority slots */
void play_rotating_cue(int scall, char *bank)
{
	snd_scall_bank(bank, scall, 0xe6, 0x80, (unsigned short)(next_cue_priority() | 0x40));
}
void play_rotating_cue_noint(int scall, char *bank)
{
	snd_scall_bank(bank, scall, 0xe6, 0x80, (next_cue_priority() | 0x40) & 0xdfff);
}

/* 0x800ebf48, 0x800ebfa8 */
void play_rotating_cue_flags(int scall, char *bank, int flags)
{
	snd_scall_bank(bank, scall, 0xe6, 0x80, (unsigned short)(flags | next_cue_priority()));
}
void play_rotating_cue_flags_noint(int scall, char *bank, int flags)
{
	snd_scall_bank(bank, scall, 0xe6, 0x80, (flags | next_cue_priority()) & 0xdfff);
}

/* 0x800ec008, 0x800ec044 */
void play_cue_flags(int scall, char *bank, int flags)
{
	snd_scall_bank(bank, scall, 0xe6, 0x80, (unsigned short)(flags | 0x40));
}
void play_cue_flags_noint(int scall, char *bank, int flags)
{
	snd_scall_bank(bank, scall, 0xe6, 0x80, (flags | 0x40) & 0xdfff);
}

/* 0x800ec080: an enemy's generic sound; in the attract only if attract sounds are on */
void play_generic_sound(snd_object_t *obj, int scall)
{
	int	on;

	if(game_phase == 1)
	{
		if(get_adjustment(2, &on))
		{
			return;
		}
		if(!on)
		{
			return;
		}
	}
	snd_scall_bank("generic1", scall, 0xff, 0x80, obj->priority);
}

/* 0x800ec100 */
void play_bank_sound(snd_object_t *obj, int scall, char *bank)
{
	int	on;

	if(game_phase == 1)
	{
		if(get_adjustment(2, &on))
		{
			return;
		}
		if(!on)
		{
			return;
		}
	}
	snd_scall_bank(bank, scall, 0xff, 0x80, obj->priority);
}

/* 0x800ec188 */
void play_generic_rotating_cue(snd_object_t *obj, int scall)
{
	snd_scall_bank("generic1", scall, 0xe6, obj->volume, (unsigned short)(next_cue_priority() | 0x10));
}

/* 0x800ec1e0 */
void play_level_cue(int scall, char *bank)
{
	snd_scall_bank(bank, scall, 0xe6, 0x80, 0x8010);
}

/* 0x800ec218, 0x800ec238 */
void stop_level_cue(void)
{
	snd_stop_track(1);
}
void stop_level_cue_and_stream(void)
{
	snd_stop_track(1);
	stream_stop();
}

/* 0x800ec260: attract-mode cue, only if the "attract sounds" adjustment is on */
void play_attract_cue(int scall, char *bank)
{
	int	on;

	if(get_adjustment(2, &on) == 0 && on)
	{
		snd_scall_bank(bank, scall, 0xff, 0x7f, 0x8000);
	}
}

/* 0x800ec2c8 */
void stop_attract_cue(void)
{
	snd_stop_track(1);
}

/* 0x800ec2e8: rotate through three priority slots for level speech */
unsigned int next_cue_priority(void)
{
	if(cue_priority_rotor == 0)
	{
		cue_priority_rotor = 1;
		return(0xe180);
	}
	if(cue_priority_rotor == 1)
	{
		cue_priority_rotor = 2;
		return(0xe200);
	}
	cue_priority_rotor = 0;
	return(0xe280);
}

/* 0x800ec338: the priority slot of a level cue (the explicit case 5 is what makes the
   compiler lay this out as a compare tree) */
unsigned int cue_priority(int slot)
{
	switch(slot)
	{
	case 3:
		return(0xe180);
	case 4:
		return(0xe200);
	case 5:
	default:
		return(0xe280);
	}
}

/* 0x800ec370 */
void play_bigtop_cue(int n, int slot)
{
	snd_scall_bank("bigtop", n + 0x27, 0xe6, 0x80, 0x8180 | (15 - slot));
}

/* 0x800ec3b0-0x800ec4c0: a bank's sound n with priority 0x20 - n */
void play_tokken2_sound(int scall, int n)
{
	snd_scall_bank("tokken2", scall, 0xe6, 0x80, (0x20 - n) | 0x8180);
}
void play_bigtop2_sound(int scall, int n)
{
	snd_scall_bank("bigtop2", scall, 0xe6, 0x80, (0x20 - n) | 0x8200);
}
void play_skeleton_sound(int scall, int n)
{
	snd_scall_bank("skeleton", scall, 0xe6, 0x80, (0x20 - n) | 0x8280);
}
void play_bigtop2_sound2(int scall, int n)
{
	snd_scall_bank("bigtop2", scall, 0xe6, 0x80, (0x20 - n) | 0x8280);
}
void play_jesbig2_sound(int scall, int n)
{
	snd_scall_bank("jesbig2", scall, 0xe6, 0x80, (0x20 - n) | 0x8280);
}

/* 0x800ec504 */
void play_bigtop2_cue(int scall)
{
	snd_scall_bank("bigtop2", scall, 0xe6, 0x80, 0x8208);
}
