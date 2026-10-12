/* audio_null.c - no real-time audio device (headless builds); --wav capture still works. */
#include "audio.h"
int  audio_start(void) { return 0; }
void audio_stop(void) { }
