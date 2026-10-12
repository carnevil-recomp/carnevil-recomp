/* audio_macos.m - CoreAudio output queue pulling samples from the virtual DCS board. */
#include <AudioToolbox/AudioToolbox.h>
#include <stdio.h>
#include "audio.h"
#include "dcs_board.h"

#define NBUF		4
#define BUFSAMPLES	1024

static AudioQueueRef	queue;
static int		running;

static void fill(void *user, AudioQueueRef q, AudioQueueBufferRef buf)
{
	(void)user;
	int n = buf->mAudioDataBytesCapacity / 2;
	dcs_render((short *)buf->mAudioData, n);
	buf->mAudioDataByteSize = n * 2;
	AudioQueueEnqueueBuffer(q, buf, 0, NULL);
}

int audio_start(void)
{
	AudioStreamBasicDescription fmt = { 0 };
	int	i;
	fmt.mSampleRate = AUDIO_RATE;
	fmt.mFormatID = kAudioFormatLinearPCM;
	fmt.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
	fmt.mBitsPerChannel = 16; fmt.mChannelsPerFrame = 1; fmt.mBytesPerFrame = 2;
	fmt.mFramesPerPacket = 1; fmt.mBytesPerPacket = 2;
	if(AudioQueueNewOutput(&fmt, fill, NULL, NULL, NULL, 0, &queue) != noErr) { fprintf(stderr, "[audio] no output queue\n"); return 0; }
	for(i = 0; i < NBUF; i++)
	{
		AudioQueueBufferRef b;
		if(AudioQueueAllocateBuffer(queue, BUFSAMPLES * 2, &b) != noErr) return 0;
		fill(NULL, queue, b);
	}
	if(AudioQueueStart(queue, NULL) != noErr) { fprintf(stderr, "[audio] cannot start output\n"); return 0; }
	running = 1;
	return 1;
}

void audio_stop(void)
{
	if(running) { AudioQueueStop(queue, true); AudioQueueDispose(queue, true); running = 0; }
}
