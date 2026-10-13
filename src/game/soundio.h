/*
 * soundio.h - the game's sound state block (0x801e6ec0): the compiler addresses its three
 * words as one object, so they are a struct.
 */
#ifndef SOUNDIO_H
#define SOUNDIO_H

typedef struct sound_state
{
	int	init_pending;		/* 0x801e6ec0: sound system (re)initialising */
	int	volume_timer;		/* 0x801e6ec4: frames left for the volume bar */
	int	volume;			/* 0x801e6ec8: master volume 0..255 */
} sound_state_t;

extern sound_state_t	sound_state;
#define sound_init_pending	sound_state.init_pending
#define sound_unk6ec4		sound_state.volume_timer

#endif
