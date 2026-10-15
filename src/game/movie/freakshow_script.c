/*
 * freakshow_script.c - the Freak Show movie script's callbacks (0x801153a0-0x801163e8; the word
 * at 0x8011539c is padding between files). The script (freakshow_script in movie_scripts.c)
 * replays a stretch of the movie while a section's enemies are alive, calling one of the
 * functions below each replay: most clear the section's timer once every spawn table slot they
 * name is done (wave_slot_done) and otherwise count it down, a few only count down or only
 * clear; freakshow_script_init gives every section its budget, freakshow_script_ending is the ending. The boss
 * callbacks (sub_80121ed4-sub_8012225c) are in junior's file. func_80115500, the four timers
 * func_801162e0-func_80116334 and func_801163cc have no caller in the image.
 * The exported names are still the ones movie_scripts.c uses (generated from names.txt).
 */
#include "spawn.h"
#include "levelsnd.h"
#include "camera.h"

extern void	fade(float target, int ticks, void (*callback)(void));
extern void	movie_script_abort(void);

int	D_801e6b74;
int	D_801e6b78;
int	D_801e6b84;
int	D_801e6b88;
int	D_801e6b8c;
int	D_801e6b9c;
int	D_801e6ba0;
int	D_801e6ba4;
int	D_801e6ba8;
int	D_801e6bbc;
int	D_801e6bc0;
int	D_801e6bc4;
int	D_801e6bc8;
int	D_801e6bdc;
int	D_801e6be0;
int	D_801e6be4;
int	D_801e6be8;
int	D_801e6bfc;
int	D_801e6c00;
int	D_801e6c04;
int	D_801e6c08;
int	D_801e6c1c;
int	D_801e6c28;
int	D_801e6c2c;
int	D_801e6c30;
int	D_801e6c3c;
int	D_801e6c48;
int	D_801e6c4c;
int	D_801e6c50;
int	D_801e6c58;
int	D_801e6c64;
int	D_801e6c68;
int	D_801e6c6c;
int	D_801e6c74;
int	D_801e6c80;
int	D_801e6c84;
int	D_801e6c8c;
int	D_801e6c94;
int	D_802371f0;

/* 0x801153a0: script entry: each section's replay budget (32 for most, 200, 30, 34, 12, 8, 4),
   the ending's count cleared */
void freakshow_script_init(void)
{
	D_801e6b78 = 32;
	D_801e6b8c = 32;
	D_801e6ba8 = 32;
	D_801e6bc8 = 32;
	D_801e6be8 = 32;
	D_801e6c08 = 32;
	D_801e6c30 = 32;
	D_801e6c50 = 32;
	D_801e6c6c = 32;
	D_801e6b74 = 32;
	D_801e6b88 = 32;
	D_801e6ba4 = 32;
	D_801e6bc4 = 32;
	D_801e6be4 = 32;
	D_801e6c04 = 32;
	D_801e6c2c = 32;
	D_801e6c4c = 200;
	D_801e6c68 = 30;
	D_801e6c84 = 30;
	D_801e6b84 = 30;
	D_801e6ba0 = 30;
	D_801e6bc0 = 34;
	D_801e6c48 = 12;
	D_801e6c80 = 8;
	D_801e6be0 = 34;
	D_801e6c00 = 34;
	D_801e6c28 = 34;
	D_801e6c64 = 34;
	D_801e6c94 = 34;
	D_801e6b9c = 34;
	D_801e6bbc = 34;
	D_801e6bdc = 34;
	D_801e6bfc = 34;
	D_801e6c1c = 4;
	D_801e6c3c = 4;
	D_801e6c58 = 4;
	D_801e6c74 = 4;
	D_801e6c8c = 0;
	D_802371f0 = 1;
}

/* 0x80115500: four of the budgets set again (8 and 2); nothing calls it */
void func_80115500(void)
{
	D_801e6b9c = 8;
	D_801e6bbc = 2;
	D_801e6bdc = 8;
	D_801e6bfc = 2;
}

/* 0x80115530: slots 0, 1, 2 done: the section ends, else one replay less */
void sub_80115530(void)
{
	if(wave_slot_done(0) && wave_slot_done(1) && wave_slot_done(2))
	{
		D_801e6b78 = 0;
	}
	else
	{
		D_801e6b78--;
	}
}

/* 0x8011559c: slots 4, 5 done: the section ends, else one replay less */
void sub_8011559c(void)
{
	if(wave_slot_done(4) && wave_slot_done(5))
	{
		D_801e6b8c = 0;
	}
	else
	{
		D_801e6b8c--;
	}
}

/* 0x801155f8: slots 7, 8, 9, 10 done: the section ends, else one replay less */
void sub_801155f8(void)
{
	if(wave_slot_done(7) && wave_slot_done(8) && wave_slot_done(9) && wave_slot_done(10))
	{
		D_801e6ba8 = 0;
	}
	else
	{
		D_801e6ba8--;
	}
}

/* 0x80115674: slots 13, 14, 15, 16, 17 done: the section ends, else one replay less */
void sub_80115674(void)
{
	if(wave_slot_done(13) && wave_slot_done(14) && wave_slot_done(15) && wave_slot_done(16) && wave_slot_done(17))
	{
		D_801e6bc8 = 0;
	}
	else
	{
		D_801e6bc8--;
	}
}

/* 0x80115700: slots 20, 22, 23 done: the section ends, else one replay less */
void sub_80115700(void)
{
	if(wave_slot_done(20) && wave_slot_done(22) && wave_slot_done(23))
	{
		D_801e6be8 = 0;
	}
	else
	{
		D_801e6be8--;
	}
}

/* 0x8011576c: slot 24 done: the section ends, else one replay less */
void sub_8011576c(void)
{
	if(wave_slot_done(24))
	{
		D_801e6c08 = 0;
	}
	else
	{
		D_801e6c08--;
	}
}

/* 0x801157b8: slots 27, 28, 29 done: the section ends, else one replay less */
void sub_801157b8(void)
{
	if(wave_slot_done(27) && wave_slot_done(28) && wave_slot_done(29))
	{
		D_801e6c30 = 0;
	}
	else
	{
		D_801e6c30--;
	}
}

/* 0x80115824: slots 30, 31, 32, 33, 35 done: the section ends, else one replay less */
void sub_80115824(void)
{
	if(wave_slot_done(30) && wave_slot_done(31) && wave_slot_done(32) && wave_slot_done(33) && wave_slot_done(35))
	{
		D_801e6c50 = 0;
	}
	else
	{
		D_801e6c50--;
	}
}

/* 0x801158b0: slots 39, 40, 42, 44 done: the section ends, else one replay less */
void sub_801158b0(void)
{
	if(wave_slot_done(39) && wave_slot_done(40) && wave_slot_done(42) && wave_slot_done(44))
	{
		D_801e6c6c = 0;
	}
	else
	{
		D_801e6c6c--;
	}
}

/* 0x8011592c: slots 47, 48, 49 done: the section ends, else one replay less */
void sub_8011592c(void)
{
	if(wave_slot_done(47) && wave_slot_done(48) && wave_slot_done(49))
	{
		D_801e6b74 = 0;
	}
	else
	{
		D_801e6b74--;
	}
}

/* 0x80115998: slots 51, 52 done: the section ends, else one replay less */
void sub_80115998(void)
{
	if(wave_slot_done(51) && wave_slot_done(52))
	{
		D_801e6b88 = 0;
	}
	else
	{
		D_801e6b88--;
	}
}

/* 0x801159f4: slots 67, 68, 69 done: the section ends, else one replay less */
void sub_801159f4(void)
{
	if(wave_slot_done(67) && wave_slot_done(68) && wave_slot_done(69))
	{
		D_801e6ba4 = 0;
	}
	else
	{
		D_801e6ba4--;
	}
}

/* 0x80115a60: slot 71 done: the section ends, else one replay less */
void sub_80115a60(void)
{
	if(wave_slot_done(71))
	{
		D_801e6bc4 = 0;
	}
	else
	{
		D_801e6bc4--;
	}
}

/* 0x80115aac: slots 72, 73, 74 done: the section ends, else one replay less */
void sub_80115aac(void)
{
	if(wave_slot_done(72) && wave_slot_done(73) && wave_slot_done(74))
	{
		D_801e6be4 = 0;
	}
	else
	{
		D_801e6be4--;
	}
}

/* 0x80115b18: slots 76, 77 done: the section ends, else one replay less */
void sub_80115b18(void)
{
	if(wave_slot_done(76) && wave_slot_done(77))
	{
		D_801e6c04 = 0;
	}
	else
	{
		D_801e6c04--;
	}
}

/* 0x80115b74: slots 82, 84 done: the section ends, else one replay less */
void sub_80115b74(void)
{
	if(wave_slot_done(82) && wave_slot_done(84))
	{
		D_801e6c2c = 0;
	}
	else
	{
		D_801e6c2c--;
	}
}

/* 0x80115bd0: slots 90, 92, 93 done: the section ends, else one replay less */
void sub_80115bd0(void)
{
	if(wave_slot_done(90) && wave_slot_done(92) && wave_slot_done(93))
	{
		D_801e6c4c = 0;
	}
	else
	{
		D_801e6c4c--;
	}
}

/* 0x80115c3c: slots 95, 96, 97, 98 done: the section ends, else one replay less */
void sub_80115c3c(void)
{
	if(wave_slot_done(95) && wave_slot_done(96) && wave_slot_done(97) && wave_slot_done(98))
	{
		D_801e6c68 = 0;
	}
	else
	{
		D_801e6c68--;
	}
}

/* 0x80115cb8: slots 100, 102 done: the section ends, else one replay less */
void sub_80115cb8(void)
{
	if(wave_slot_done(100) && wave_slot_done(102))
	{
		D_801e6c84 = 0;
	}
	else
	{
		D_801e6c84--;
	}
}

/* 0x80115d14: slots 104, 105, 107, 108, 109 done: the section ends, else one replay less */
void sub_80115d14(void)
{
	if(wave_slot_done(104) && wave_slot_done(105) && wave_slot_done(107) && wave_slot_done(108) && wave_slot_done(109))
	{
		D_801e6b84 = 0;
	}
	else
	{
		D_801e6b84--;
	}
}

/* 0x80115da0: slots 110, 111, 112, 113, 114, 115, 116, 117 done: the section ends, else one replay less */
void sub_80115da0(void)
{
	if(wave_slot_done(110) && wave_slot_done(111) && wave_slot_done(112) && wave_slot_done(113) && wave_slot_done(114) && wave_slot_done(115) && wave_slot_done(116) && wave_slot_done(117))
	{
		D_801e6ba0 = 0;
	}
	else
	{
		D_801e6ba0--;
	}
}

/* 0x80115e5c: slot 118 done: the section ends (no count down) */
void sub_80115e5c(void)
{
	if(wave_slot_done(118))
	{
		D_801e6bc0 = 0;
	}
}

/* 0x80115e8c: slots 122, 123, 124, 120, 121 done: the section ends, else one replay less */
void sub_80115e8c(void)
{
	if(wave_slot_done(122) && wave_slot_done(123) && wave_slot_done(124) && wave_slot_done(120) && wave_slot_done(121))
	{
		D_801e6be0 = 0;
	}
	else
	{
		D_801e6be0--;
	}
}

/* 0x80115f18: slot 125 done: the section ends, else one replay less */
void sub_80115f18(void)
{
	if(wave_slot_done(125))
	{
		D_801e6c00 = 0;
	}
	else
	{
		D_801e6c00--;
	}
}

/* 0x80115f64: slots 129, 130, 131 done: the section ends, else one replay less */
void sub_80115f64(void)
{
	if(wave_slot_done(129) && wave_slot_done(130) && wave_slot_done(131))
	{
		D_801e6c28 = 0;
	}
	else
	{
		D_801e6c28--;
	}
}

/* 0x80115fd0: slots 133, 134 done: the section ends, else one replay less */
void sub_80115fd0(void)
{
	if(wave_slot_done(133) && wave_slot_done(134))
	{
		D_801e6c48 = 0;
	}
	else
	{
		D_801e6c48--;
	}
}

/* 0x8011602c: slots 136, 137, 138 done: the section ends, else one replay less */
void sub_8011602c(void)
{
	if(wave_slot_done(136) && wave_slot_done(137) && wave_slot_done(138))
	{
		D_801e6c64 = 0;
	}
	else
	{
		D_801e6c64--;
	}
}

/* 0x80116098: one replay less (no spawn slot to wait for) */
void sub_80116098(void)
{
	D_801e6c80--;
}

/* 0x801160b4: slots 140, 141, 142 done: the section ends, else one replay less */
void sub_801160b4(void)
{
	if(wave_slot_done(140) && wave_slot_done(141) && wave_slot_done(142))
	{
		D_801e6c94 = 0;
	}
	else
	{
		D_801e6c94--;
	}
}

/* 0x80116120: slots 152, 153 done: the section ends, else one replay less */
void sub_80116120(void)
{
	if(wave_slot_done(152) && wave_slot_done(153))
	{
		D_801e6b9c = 0;
	}
	else
	{
		D_801e6b9c--;
	}
}

/* 0x8011617c: slots 168, 169, 170 done: the section ends, else one replay less */
void sub_8011617c(void)
{
	if(wave_slot_done(168) && wave_slot_done(169) && wave_slot_done(170))
	{
		D_801e6bbc = 0;
	}
	else
	{
		D_801e6bbc--;
	}
}

/* 0x801161e8: slots 172, 173, 174, 175, 176 done: the section ends, else one replay less */
void sub_801161e8(void)
{
	if(wave_slot_done(172) && wave_slot_done(173) && wave_slot_done(174) && wave_slot_done(175) && wave_slot_done(176))
	{
		D_801e6bdc = 0;
	}
	else
	{
		D_801e6bdc--;
	}
}

/* 0x80116274: slots 177, 178, 179 done: the section ends, else one replay less */
void sub_80116274(void)
{
	if(wave_slot_done(177) && wave_slot_done(178) && wave_slot_done(179))
	{
		D_801e6bfc = 0;
	}
	else
	{
		D_801e6bfc--;
	}
}

/* 0x801162e0, 0x801162fc, 0x80116318, 0x80116334: one replay less; nothing calls them */
void func_801162e0(void)
{
	D_801e6c1c--;
}

void func_801162fc(void)
{
	D_801e6c3c--;
}

void func_80116318(void)
{
	D_801e6c58--;
}

void func_80116334(void)
{
	D_801e6c74--;
}

/* 0x80116350: the ending: on the 10th replay the stream and music fade out, the guns go off
   and the screen fades over 80 frames; on the 18th the script ends */
void freakshow_script_ending(void)
{
	if(++D_801e6c8c == 10)
	{
		fade_out_level_stream();
		disable_guns();
		fade_out_level_music();
		fade(0.0f, 80, 0);
	}
	if(D_801e6c8c == 18)
	{
		movie_script_abort();
	}
}

/* 0x801163cc: the counter freakshow_script_init sets to 1, one less; nothing calls it */
void func_801163cc(void)
{
	D_802371f0--;
}
