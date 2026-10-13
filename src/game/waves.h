/*
 * waves.h - the "wave" ids the game passes around (load_wave_proc, load_level_spawn,
 * stream_select_wave, think_pass, ...) and the bits of the debug flags word (game_state[3],
 * 0x80212e20). A wave is a level or an attract sequence; bit 24 marks a playable level.
 */
#ifndef WAVES_H
#define WAVES_H

#define WAVE_LEVEL		0x1000000	/* set on the four levels */
#define WAVE_HHOUSE		0x1020000	/* the haunted house (movie partition 4) */
#define WAVE_RICKITY		0x1040000	/* Rickety Town (partition 8, music stream partition 7) */
#define WAVE_FREAKSHOW		0x1060000	/* the freak show (partition 6, stream partition 5) */
#define WAVE_BIGTOP		0x1070000	/* the big top (partition 10, stream partition 9) */
#define WAVE_ATTRACT		0xa0000		/* the attract loop's banks */
#define WAVE_ATTRACT_B		0xb0000		/* two more non-level waves load_level_spawn accepts */
#define WAVE_ATTRACT_C		0xc0000
#define WAVE_BIGTOP_LOAD	0xd0000		/* the big top's loading music stream (btopload_proc) */

/* debug_mode bits (only the ones whose effect is known) */
#define DEBUG_ON		0x1		/* free play, draws the level's 3D in the attract, die list bypass */
#define DEBUG_RTP_FPS		0x4		/* the triangle/DMA counters and the FPS */
#define DEBUG_NO_CAMERA		0x10		/* update_camera leaves the camera alone */
#define DEBUG_TEXMAP_LOG	0x200		/* texmap loads print the texture memory */
#define DEBUG_SPAWN_LOG		0x400		/* spawn table messages */
#define DEBUG_FRAME_TEXT	0x2000		/* "FRAME n" over the movie */
#define DEBUG_HEAP		0x8000		/* the heap display */
#define DEBUG_CHARACTERS	0x10000		/* follow an enemy; the script holds the frame */
#define DEBUG_POINT		0x20000		/* nudge and show a positioned thing */
#define DEBUG_VECTORS		0x80000		/* nudge and show the two debug points */

#endif
