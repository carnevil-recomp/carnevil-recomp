/* movie_scripts.c - the attract-mode movie scripts (0x801130b8-0x80113218 in GAME.EXE),
   one contiguous stream: midway/attract/attract3/gallery are entry points into it. See movie.c. */
#include "movie.h"

extern int	script_fade_out_108();
extern int	script_fade_out_40();
extern int	script_fade_out_40b();
extern int	script_level_end_stats();
extern int	script_end_cleanup();
extern int	script_fade_out_90();
extern int	func_8012fa20();
extern int	func_8012fbf0();
extern int	D_801e6e70;
extern int	D_8018db90;

script_word_t	movie_scripts[] =
{
	/* intro_script (0x801130b8) */
	1,
	2545,
	3,
	2475,
	1,
	2545,
	3,
	2475,
	1,
	2545,
	3,
	2475,
	1,
	2545,
	3,
	2475,
	1,
	2545,
	5,
	SCRIPT_FN(script_fade_out_108),
	3,
	2475,
	1,
	2545,
	3,
	2475,
	1,
	2545,
	9,
	/* attract2_script (0x8011312c) */
	1,
	701,
	3,
	631,
	1,
	701,
	3,
	631,
	1,
	701,
	3,
	631,
	1,
	701,
	5,
	SCRIPT_FN(script_fade_out_40),
	3,
	631,
	1,
	701,
	9,
	/* end_script (0x80113180) */
	3,
	190,
	1,
	535,
	24,
	200,
	1,
	1290,
	5,
	SCRIPT_FN(script_level_end_stats),
	24,
	100,
	5,
	SCRIPT_FN(script_end_cleanup),
	24,
	90,
	9,
	/* end_script_loop (0x801131c4) */
	3,
	0,
	1,
	5,
	2,
	SCRIPT_LABEL(67),
	9,
	/* midway_script (0x801131e0) */
	1,
	180,
	9,
	/* attract_script (0x801131ec) */
	1,
	428,
	9,
	/* attract3_script (0x801131f8) */
	1,
	423,
	9,
	/* gallery_script (0x80113204) */
	1,
	60,
	10,
	1000,
	9,
};

