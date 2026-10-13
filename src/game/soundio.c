/*
 * soundio.c - the game's thin layer over the SDK sound library: start-up (main_init_sound,
 * 0x800c5e50) and the cue helpers the attract mode and levels use (0x800ec1e0-0x800ec3a0).
 */
#include <stdio.h>
#include "soundio.h"

extern int	snd_init_multi(int);
extern unsigned int snd_get_opsys_ver(void);
extern void	snd_bank_init(void);
extern int	snd_bank_load(char *);
extern int	snd_master_volume(int);
extern int	snd_output_stereo(void), snd_output_mono(void);
extern int	snd_scall_bank(char *, unsigned int, unsigned int, unsigned int, unsigned int);
extern int	snd_stop_track(unsigned int);
extern void	snd_delay(volatile int);
extern int	get_adjustment(int, int *);
extern void	stream_stop(void);			/* 0x8013fb84 */

int		snd_opsys_ver;			/* 0x802373e4 */
sound_state_t	sound_state;			/* 0x801e6ec0 */
int		stereo_output;			/* 0x801de954 */
int		sound_retry_audit;		/* 0x80179be8 */
int		cue_priority_rotor;		/* 0x8018e04c */

/* 0x800c5e50 */
void main_init_sound(void)
{
	int	tries = 0;
	int	err;

	err = snd_init_multi(1);
again:
	if(err)
	{
		printf("\nmain_init_sound(): ERROR initializing sound system.\n");
		tries++;
		snd_delay(20);
		printf("retrying the motherfucker\n");
		err = snd_init_multi(1);
		if(tries < 3)
		{
			goto again;
		}
	}
	if(err)
	{
		printf("fuck this shit... Aborting\n");
		snd_opsys_ver = 0xbad;
		sound_state.volume = 0;
		sound_init_pending = 1;
		return;
	}
	printf("main_init_sound: (information) requesting version number\n");
	snd_opsys_ver = snd_get_opsys_ver();
	sound_init_pending = 1;
	snd_bank_init();
	printf("main_init_sound(): Loading banks...\n");
	snd_bank_load("generic1");
	snd_bank_load("wavinmus");
	snd_bank_load("girl");
	if(get_adjustment(3, &sound_state.volume))
	{
		sound_state.volume = 200;
	}
	snd_master_volume(sound_state.volume);
	if(stereo_output)
	{
		snd_output_stereo();
	}
	else
	{
		snd_output_mono();
	}
	sound_init_pending = 0;
	sound_unk6ec4 = 0;
	printf("main_init_sound(): done\n");
}

/* 0x800c5fe8: re-initialise the board if its operating system is not the one we loaded */
void check_sound_os(void)
{
	if(snd_get_opsys_ver() != 0x223)
	{
		sound_retry_audit++;
	}
	main_init_sound();
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

/* 0x800ec338 */
unsigned int cue_priority(int slot)
{
	if(slot == 4)
	{
		return(0xe200);
	}
	if(slot < 5)
	{
		if(slot == 3)
		{
			return(0xe180);
		}
		return(0xe280);
	}
	return(0xe280);
}

/* 0x800ec370 */
void play_bigtop_cue(int n, int slot)
{
	snd_scall_bank("bigtop", n + 0x27, 0xe6, 0x80, 0x8180 | (15 - slot));
}
