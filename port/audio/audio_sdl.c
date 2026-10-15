/* SDL2 audio output pulling stereo 16-bit frames at 31250 Hz from the virtual DCS board
   (the counterpart of audio_macos.m). */
#include <stdio.h>
#include <SDL.h>
#include "audio.h"
#include "dcs_board.h"

static SDL_AudioDeviceID	dev;

static void fill(void *user, Uint8 *stream, int len)
{
	(void)user;
	dcs_render((short *)stream, len / 4);		/* 4 bytes per stereo frame */
}

int audio_start(void)
{
	SDL_AudioSpec	want, have;
	if(SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { fprintf(stderr, "[audio] SDL: %s\n", SDL_GetError()); return 0; }
	SDL_zero(want);
	want.freq = AUDIO_RATE; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024; want.callback = fill;
	dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if(!dev) { fprintf(stderr, "[audio] no output device: %s\n", SDL_GetError()); return 0; }
	SDL_PauseAudioDevice(dev, 0);
	return 1;
}

void audio_stop(void)
{
	if(dev) { SDL_CloseAudioDevice(dev); dev = 0; }
}
