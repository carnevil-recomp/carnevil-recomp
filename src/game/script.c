/*
 * script.c - the movie script interpreter (0x80110270-0x80110b04): a script is a list of
 * words (opcodes, arguments, labels, function pointers) walked by do_script() once per
 * frame to decide which frame the loader reads next. The scripts themselves are in
 * movie_scripts.c (attract) and the level files.
 */
#include <stdio.h>
#include "movie.h"
#include "credits.h"			/* game_mode (0x80212e14), player_stats (0x801e0f64) */
#include "waves.h"

extern void	fade(float, int, void (*)(void));
extern int	end_of_wave(void);
extern void	increment_audit(int);
extern void	stop_level_cue_and_stream(void), play_level_cue(int, char *);
extern void	play_level_music(int, char *), play_level_speech(int, char *), start_level_music(int, char *);
extern void	stop_level_music(void), fade_out_level_music(void), fade_out_level_stream(void), func_800d4968(void);
extern void	play_bigtop_cue(int, int), play_bigtop2_cue(int);
extern void	stream_set_loop(int, int), stream_jump(int, int, int);	/* 0x801400a4, 0x801400dc */
extern void	*func_800d6758();
extern short	add_object1_wrapper(int, void (*)(), int, int);
extern void	level_end_stats_wrapper();

/* script interpreter */
script_word_t	*movie_script_g1;		/* 0x801e09d8: start of the current script */
script_word_t	*movie_script_g2;		/* 0x801e177c: next word to execute */
int		script_stop_frame;		/* 0x8023740c: play until this frame */
int		script_delay;			/* 0x80212e94: ticks to hold the current frame */
int		script_abort;			/* 0x801e5bdc: end the movie (end_of_wave) */
int		script_skip;			/* 0x801e5b24: jump to the stop frame */


/* 0x80110270 */
void movie_script_init(script_word_t *script)
{
	movie_script_g1 = script;
	movie_script_g2 = script;
	script_stop_frame = 0;
	script_delay = 0;
	script_abort = 0;
	script_skip = 0;
}

/* 0x801102a8, 0x801102bc */
void movie_script_abort(void) { script_abort = 1; }
void movie_script_skip(void) { script_skip = 1; }


/* ---- the script interpreter (0x801102d0) ---- */

#define NEXT()		(*movie_script_g2++)

/* returns the frame to read next, MOVIE_END when the movie is over. The cases are in the
   original's order (its jump table is indexed by opcode) */
int do_script(int frame)
{
	script_word_t	target;
	script_word_t	arg, arg2, arg3;
	int		(*fn)();
	int		next;
	int		keyword;

	if(script_abort)
	{
		end_of_wave();
		return(MOVIE_END);
	}
	if(debug_mode & DEBUG_CHARACTERS)
	{
		return(frame);
	}
	if(game_mode != 1 && !(player_stats[0].state & 0x44) && !(player_stats[1].state & 0x44))
	{
		return(frame);
	}
	if(script_delay > 0)
	{
		script_delay--;
		return(frame);
	}
	if(script_skip)
	{
		frame = script_stop_frame;
		script_skip = 0;
	}
	next = frame + 1;
	if(frame < script_stop_frame)
	{
		return(next);
	}
	for(;;)
	{
		keyword = NEXT();
		switch(keyword)
		{
		case 1:		/* play until frame N */
			script_stop_frame = NEXT();
			return(next);
		case 3:		/* seek */
			next = NEXT();
			break;
		case 2:		/* goto */
			target = NEXT();
			movie_script_g2 = (script_word_t *)target;
			break;
		case 5:		/* call */
			((void (*)())NEXT())();
			break;
		case 6:
			play_level_music(NEXT(), "generic1");
			break;
		case 19:
			arg = NEXT();
			arg2 = NEXT();
			arg3 = NEXT();
			stream_jump(arg, arg2, arg3);
			break;
		case 25:
			arg = NEXT();
			arg2 = NEXT();
			stream_set_loop(arg, arg2);
			break;
		case 12:
			play_level_music(NEXT(), "hhouse1");
			break;
		case 14:
			play_level_music(NEXT(), "freaksho");
			break;
		case 16:
			play_level_speech(NEXT(), "freaksho");
			break;
		case 17:
			start_level_music(NEXT(), "freaksho");
			break;
		case 21:
			play_level_music(NEXT(), "rickity");
			break;
		case 22:
			play_level_speech(NEXT(), "rickity");
			break;
		case 23:
			start_level_music(NEXT(), "rickity");
			break;
		case 26:
			play_level_music(NEXT(), "bigtop");
			break;
		case 29:
			arg = NEXT();
			if((unsigned)(arg - 0x11) < 2)
			{
				play_bigtop2_cue(arg);
			}
			else
			{
				play_level_music(arg, "bigtop2");
			}
			break;
		case 27:
			start_level_music(NEXT(), "bigtop");
			break;
		case 28:
			play_bigtop_cue(NEXT(), 0);
			break;
		case 15:
			stop_level_music();
			break;
		case 18:
			fade_out_level_music();
			break;
		case 13:
			play_level_music(NEXT(), "chastity");
			break;
		case 7:		/* *var = 0 */
			arg = NEXT();
			*(int *)arg = 0;
			break;
		case 8:		/* (*var)++ */
			arg = NEXT();
			(*(int *)arg)++;
			break;
		case 4:		/* if(*var) goto */
			arg = NEXT();
			target = NEXT();
			if(*(int *)arg)
			{
				movie_script_g2 = (script_word_t *)target;
			}
			break;
		case 10:	/* hold N ticks with the loader paused */
			arg = NEXT();
			script_delay = arg;
			movie_pause();
			return(next);
		case 24:	/* hold N ticks */
			arg = NEXT();
			script_delay = arg;
			return(next);
		case 11:	/* if(fn()) goto */
			fn = (int (*)())NEXT();
			target = NEXT();
			if(fn())
			{
				movie_script_g2 = (script_word_t *)target;
			}
			break;
		case 20:	/* audits: the level reached and the ending seen */
			arg = NEXT();
			switch((int)arg)
			{
			case 20: increment_audit(0x3b); break;
			case 30: increment_audit(0x41); break;
			case 31: increment_audit(0x42); break;
			case 32: increment_audit(0x43); break;
			case 33: increment_audit(0x44); break;
			case 40: increment_audit(0x48); break;
			case 41: increment_audit(0x49); break;
			case 42: increment_audit(0x4a); break;
			case 43: increment_audit(0x4b); break;
			}
			break;
		case 9:		/* end */
			return(MOVIE_END);
		default:
			printf("Unknown keyword %d in movie script\n", keyword);
			break;
		}
	}
}

/* script helpers called through opcode 5 (0x801109c0-0x80110ab0) */
void script_fade_out_108(void) { fade(0.0f, 108, 0); }
void script_fade_out_40(void) { fade(0.0f, 40, 0); }
void script_fade_out_40b(void) { fade(0.0f, 40, 0); }
void script_level_end_stats(void)
{
	func_800d6758();
	add_object1_wrapper(0x800, level_end_stats_wrapper, 0, 0);
}
void script_end_cleanup(void)
{
	fade_out_level_stream();
	func_800d4968();
	fade_out_level_music();
	fade(0.0f, 80, 0);
}
void script_fade_out_90(void) { fade(0.0f, 90, 0); }

/* 0x80110ad8: fade in over 90 frames */
void script_fade_in_90(void) { fade(1.0f, 90, 0); }
