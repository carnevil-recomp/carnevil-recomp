/* Audio output for the port: pulls mono 16-bit samples at 31250 Hz from the virtual DCS board. */
#ifndef PORT_AUDIO_H
#define PORT_AUDIO_H
#define AUDIO_RATE	31250
int  audio_start(void);				/* real-time output; 0 if none is available */
void audio_stop(void);
void audio_wav_open(const char *path);		/* --wav FILE: render in step with the frame clock instead */
void audio_wav_frame(void);			/* called once per vblank by the HAL */
#endif
