/*
 * audio_wav.c - frame-clocked audio capture: with --wav FILE the port renders the board's
 * output in lock step with the vblank clock (31250 / 54 samples per vblank) and writes it as
 * a 16-bit mono WAV, so a headless run produces the exact soundtrack of the frames it ran.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"
#include "dcs_board.h"

static FILE	*wav;
static unsigned long total;
static double	carry;
extern int hal_refresh_hz(void);

static void put32(unsigned v) { fputc(v & 255, wav); fputc((v >> 8) & 255, wav); fputc((v >> 16) & 255, wav); fputc(v >> 24, wav); }
static void put16(unsigned v) { fputc(v & 255, wav); fputc((v >> 8) & 255, wav); }

void audio_wav_open(const char *path)
{
	wav = fopen(path, "wb");
	if(!wav) { fprintf(stderr, "[audio] cannot write %s\n", path); return; }
	fwrite("RIFF", 1, 4, wav); put32(0); fwrite("WAVEfmt ", 1, 8, wav); put32(16); put16(1); put16(1);
	put32(AUDIO_RATE); put32(AUDIO_RATE * 2); put16(2); put16(16); fwrite("data", 1, 4, wav); put32(0);
}

void audio_wav_frame(void)
{
	short	buf[2048];
	int	n;
	if(!wav) return;
	carry += (double)AUDIO_RATE / hal_refresh_hz();
	n = (int)carry; carry -= n;
	if(n > 2048) n = 2048;
	dcs_render(buf, n);
	fwrite(buf, 2, n, wav);
	total += n;
}

void audio_wav_close(void)
{
	if(!wav) return;
	fseek(wav, 4, SEEK_SET); put32(36 + total * 2);
	fseek(wav, 40, SEEK_SET); put32(total * 2);
	fclose(wav); wav = 0;
	fprintf(stderr, "[audio] wrote %lu samples (%.1f s)\n", total, total / (double)AUDIO_RATE);
}
