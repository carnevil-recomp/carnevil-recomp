/*
 * levelsnd.h - the level sound helpers (src/game/levelsnd.c, 0x800ebb18-0x800ec53c): the
 * music/stream fades and the sound-call wrappers the attract mode, the movie scripts and
 * the enemies use. The priority word of snd_scall_bank(): bit 15 set, 0x2000 = no interrupt,
 * low bits = priority (a lower value wins). Enemies carry their own priority words.
 */
#ifndef LEVELSND_H
#define LEVELSND_H

/* the sound helpers take the player record (priority, volume) or the enemy record
   (prio_base, prio_flags): the same offsets on the original, so the code is unchanged, and
   the real types on the host where the records hold 8-byte pointers */
struct player_stats;
struct enemy;

extern int	track_volume, track_fade, stream_volume, stream_fade, cue_priority_rotor;

void	reset_stream_volume(void);
void	fade_out_level_stream(void), fade_in_level_stream(void);
void	level_sound_fades(void);
void	clean_up_level_sounds(void);
void	play_enemy_sound(struct enemy *obj, int scall, char *bank, int n);
void	play_enemy_sound_noint(struct enemy *obj, int scall, char *bank, int n);
void	start_level_music(int scall, char *bank);
void	play_level_music(int scall, char *bank);
void	play_level_speech(int scall, char *bank);
void	stop_level_music(void), fade_out_level_music(void);
void	play_rotating_cue(int scall, char *bank), play_rotating_cue_noint(int scall, char *bank);
void	play_rotating_cue_flags(int scall, char *bank, int flags), play_rotating_cue_flags_noint(int scall, char *bank, int flags);
void	play_cue_flags(int scall, char *bank, int flags), play_cue_flags_noint(int scall, char *bank, int flags);
void	play_generic_sound(struct player_stats *obj, int scall);
void	play_bank_sound(struct player_stats *obj, int scall, char *bank);
void	play_generic_rotating_cue(struct player_stats *obj, int scall);
void	play_level_cue(int scall, char *bank);
void	stop_level_cue(void), stop_level_cue_and_stream(void);
void	play_attract_cue(int scall, char *bank), stop_attract_cue(void);
unsigned int	next_cue_priority(void), cue_priority(int slot);
void	play_bigtop_cue(int n, int slot);
void	play_tokken2_sound(int scall, int n), play_bigtop2_sound(int scall, int n), play_skeleton_sound(int scall, int n);
void	play_bigtop2_sound2(int scall, int n), play_jesbig2_sound(int scall, int n);
void	play_bigtop2_cue(int scall);

#endif
