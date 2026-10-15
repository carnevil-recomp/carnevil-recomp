/*
 * hhouse_script.c - the Haunted House movie script's callbacks (0x800fa720-0x800fb4a0; the
 * word at 0x800fa71c is padding between files). The script (hhouse_script in movie_scripts.c)
 * replays a stretch of the movie while a section's enemies are alive, calling one of the
 * functions below each replay: most clear the section's timer once every spawn table slot they
 * name is done (wave_slot_done) and otherwise count it down, some only count down or only
 * clear, a few cut the remaining replays short; func_800fa720 gives every section its budget.
 * The boss sections call the callbacks in the Mari file (0x800eef68-0x800ef870), not these.
 * The exported names are still the ones movie_scripts.c uses (generated from names.txt).
 */
#include "spawn.h"
#include "levelsnd.h"
#include "camera.h"

extern void	fade(float target, int ticks, void (*callback)(void));
extern void	movie_script_abort(void);

int	D_801e098c;
int	D_801e6d5c;
int	D_801e6d60;
int	D_801e6d6c;
int	D_801e6d70;
int	D_801e6d74;
int	D_801e6d88;
int	D_801e6d8c;
int	D_801e6d90;
int	D_801e6d94;
int	D_801e6da8;
int	D_801e6dac;
int	D_801e6db0;
int	D_801e6db4;
int	D_801e6dcc;
int	D_801e6dd0;
int	D_801e6dd4;
int	D_801e6dd8;
int	D_801e6df0;
int	D_801e6df4;
int	D_801e6df8;
int	D_801e6dfc;
int	D_801e6e18;
int	D_801e6e1c;
int	D_801e6e20;
int	D_801e6e24;
int	D_801e6e3c;
int	D_801e6e40;
int	D_801e6e44;
int	D_801e6e48;
int	D_801e6e60;
int	D_801e6e64;
int	D_801e6e68;
int	D_801e6e6c;
int	D_801e6e88;
int	D_801e6e8c;
int	D_801e6ea4;
int	D_80212d8c;
int	D_802372e4;

/* 0x800fa720: script entry: each section's replay budget */
void func_800fa720(void)
{
	D_801e6d60 = 30;
	D_801e6d74 = 30;
	D_801e6d94 = 4;
	D_80212d8c = 14;
	D_801e6db4 = 30;
	D_801e6dd8 = 12;
	D_801e6dfc = 30;
	D_801e6e24 = 30;
	D_801e6e48 = 20;
	D_801e6e68 = 30;
	D_801e6d5c = 10;
	D_801e6d6c = 10;
	D_801e6d8c = 10;
	D_801e6db0 = 30;
	D_801e6dd4 = 30;
	D_801e6df8 = 30;
	D_801e6e20 = 10;
	D_801e6e44 = 30;
	D_801e6e6c = 6;
	D_801e6e8c = 30;
	D_801e6d70 = 30;
	D_801e6d90 = 30;
	D_801e6dd0 = 30;
	D_801e6dac = 10;
	D_801e6df4 = 10;
	D_801e6e1c = 40;
	D_801e6e40 = 40;
	D_801e6e64 = 40;
	D_801e6e88 = 40;
	D_801e6ea4 = 30;
	D_801e6d88 = 80;
	D_801e6da8 = 80;
	D_801e6dcc = 40;
	D_801e6df0 = 40;
	D_801e6e18 = 40;
	D_801e6e3c = 40;
	D_801e6e60 = 40;
	D_802372e4 = 0;
	D_801e098c = 1;
}

/* 0x800fa888: slots 0 done: the section ends, else one replay less */
void sub_800fa888(void)
{
	if(wave_slot_done(0))
	{
		D_801e6d60 = 0;
	}
	else
	{
		D_801e6d60--;
	}
}

/* 0x800fa8d4: slots 7-10 all done: one more replay at most, else one replay less */
void sub_800fa8d4(void)
{
	int	done;

	done = wave_slot_done(7);
	done += wave_slot_done(8);
	done += wave_slot_done(9);
	done += wave_slot_done(10);
	if(done >= 4 && D_801e6d74 > 1)
	{
		D_801e6d74 = 1;
	}
	else
	{
		D_801e6d74--;
	}
}

/* 0x800fa958: one replay less (no spawn slot to wait for) */
void sub_800fa958(void)
{
	D_801e6d94--;
}

/* 0x800fa974: slots 19 done: the section ends, else one replay less */
void sub_800fa974(void)
{
	if(wave_slot_done(19))
	{
		D_801e6db4 = 0;
	}
	else
	{
		D_801e6db4--;
	}
}

/* 0x800fa9c0: one replay less (no spawn slot to wait for) */
void sub_800fa9c0(void)
{
	D_801e6dd8--;
}

/* 0x800fa9dc: slots 43, 44, 45, 46, 41, 42 done: the section ends, else one replay less */
void sub_800fa9dc(void)
{
	if(wave_slot_done(43) && wave_slot_done(44) && wave_slot_done(45) && wave_slot_done(46) && wave_slot_done(41) && wave_slot_done(42))
	{
		D_801e6dfc = 0;
	}
	else
	{
		D_801e6dfc--;
	}
}

/* 0x800faa78: slots 47, 48 done: the section ends, else one replay less */
void sub_800faa78(void)
{
	if(wave_slot_done(47) && wave_slot_done(48))
	{
		D_801e6e24 = 0;
	}
	else
	{
		D_801e6e24--;
	}
}

/* 0x800faad4: slots 49, 50, 52 done: the section ends, else one replay less */
void sub_800faad4(void)
{
	if(wave_slot_done(49) && wave_slot_done(50) && wave_slot_done(52))
	{
		D_801e6e48 = 0;
	}
	else
	{
		D_801e6e48--;
	}
}

/* 0x800fab40: slots 53 done: the section ends, else one replay less */
void sub_800fab40(void)
{
	if(wave_slot_done(53))
	{
		D_801e6e68 = 0;
	}
	else
	{
		D_801e6e68--;
	}
}

/* 0x800fab8c: one replay less (no spawn slot to wait for) */
void sub_800fab8c(void)
{
	D_801e6d5c--;
}

/* 0x800faba8: slot 63 done: the section ends (the timer is not counted down here) */
void sub_800faba8(void)
{
	if(wave_slot_done(63))
	{
		D_801e6d6c = 0;
	}
}

/* 0x800fabd8: the next section gets 6 replays */
void sub_800fabd8(void)
{
	D_801e6d8c = 6;
}

/* 0x800fabec: slots 63 done: the section ends, else one replay less */
void sub_800fabec(void)
{
	if(wave_slot_done(63))
	{
		D_801e6d8c = 0;
	}
	else
	{
		D_801e6d8c--;
	}
}

/* 0x800fac38: slot 71 done: at most 4 more replays, else one replay less */
void sub_800fac38(void)
{
	if(wave_slot_done(71))
	{
		if(D_801e6db0 > 4)
		{
			D_801e6db0 = 4;
		}
		else
		{
			D_801e6db0--;
		}
	}
	else
	{
		D_801e6db0--;
	}
}

/* 0x800fac90: slots 73, 74, 75, 76 done: the section ends, else one replay less */
void sub_800fac90(void)
{
	if(wave_slot_done(73) && wave_slot_done(74) && wave_slot_done(75) && wave_slot_done(76))
	{
		D_801e6dd4 = 0;
	}
	else
	{
		D_801e6dd4--;
	}
}

/* 0x800fad0c: slots 77 and 78 done: at most 2 more replays, else one replay less */
void sub_800fad0c(void)
{
	if(wave_slot_done(77) && wave_slot_done(78) && D_801e6df8 > 2)
	{
		D_801e6df8 = 2;
	}
	else
	{
		D_801e6df8--;
	}
}

/* 0x800fad6c: slot 81 done: the section ends (the timer is not counted down here) */
void sub_800fad6c(void)
{
	if(wave_slot_done(81))
	{
		D_801e6e20 = 0;
	}
}

/* 0x800fad9c: slots 95, 96 done: the section ends, else one replay less */
void sub_800fad9c(void)
{
	if(wave_slot_done(95) && wave_slot_done(96))
	{
		D_801e6e44 = 0;
	}
	else
	{
		D_801e6e44--;
	}
}

/* 0x800fadf8: one replay less (no spawn slot to wait for) */
void sub_800fadf8(void)
{
	D_801e6e6c--;
}

/* 0x800fae14: slots 107, 108 done: the section ends, else one replay less */
void sub_800fae14(void)
{
	if(wave_slot_done(107) && wave_slot_done(108))
	{
		D_801e6e8c = 0;
	}
	else
	{
		D_801e6e8c--;
	}
}

/* 0x800fae70: slots 115, 116 done: the section ends, else one replay less */
void sub_800fae70(void)
{
	if(wave_slot_done(115) && wave_slot_done(116))
	{
		D_801e6d70 = 0;
	}
	else
	{
		D_801e6d70--;
	}
}

/* 0x800faecc: slots 119 done: the section ends, else one replay less */
void sub_800faecc(void)
{
	if(wave_slot_done(119))
	{
		D_801e6d90 = 0;
	}
	else
	{
		D_801e6d90--;
	}
}

/* 0x800faf18: not referenced by the script */
void func_800faf18(void)
{
	D_801e6dac = 0;
}

/* 0x800faf28: slots 121, 122, 123, 124 done: the section ends, else one replay less */
void sub_800faf28(void)
{
	if(wave_slot_done(121) && wave_slot_done(122) && wave_slot_done(123) && wave_slot_done(124))
	{
		D_801e6dd0 = 0;
	}
	else
	{
		D_801e6dd0--;
	}
}

/* 0x800fafa4: slot 126 done: the section ends (the timer is not counted down here) */
void sub_800fafa4(void)
{
	if(wave_slot_done(126))
	{
		D_801e6df4 = 0;
	}
}

/* 0x800fafd4: slots 136, 137 done: the section ends, else one replay less */
void sub_800fafd4(void)
{
	if(wave_slot_done(136) && wave_slot_done(137))
	{
		D_801e6e1c = 0;
	}
	else
	{
		D_801e6e1c--;
	}
}

/* 0x800fb030: slots 142, 143 done: the section ends, else one replay less */
void sub_800fb030(void)
{
	if(wave_slot_done(142) && wave_slot_done(143))
	{
		D_801e6e40 = 0;
	}
	else
	{
		D_801e6e40--;
	}
}

/* 0x800fb08c: slots 144, 145, 146, 147 done: the section ends, else one replay less */
void sub_800fb08c(void)
{
	if(wave_slot_done(144) && wave_slot_done(145) && wave_slot_done(146) && wave_slot_done(147))
	{
		D_801e6e64 = 0;
	}
	else
	{
		D_801e6e64--;
	}
}

/* 0x800fb108: slots 148, 149, 150 done: the section ends, else one replay less */
void sub_800fb108(void)
{
	if(wave_slot_done(148) && wave_slot_done(149) && wave_slot_done(150))
	{
		D_801e6e88 = 0;
	}
	else
	{
		D_801e6e88--;
	}
}

/* 0x800fb174: slots 160 done: the section ends, else one replay less */
void sub_800fb174(void)
{
	if(wave_slot_done(160))
	{
		D_801e6ea4 = 0;
	}
	else
	{
		D_801e6ea4--;
	}
}

/* 0x800fb1c0: slots 161 done: the section ends, else one replay less */
void sub_800fb1c0(void)
{
	if(wave_slot_done(161))
	{
		D_801e6d88 = 0;
	}
	else
	{
		D_801e6d88--;
	}
}

/* 0x800fb20c: slots 168 done: the section ends, else one replay less */
void sub_800fb20c(void)
{
	if(wave_slot_done(168))
	{
		D_801e6da8 = 0;
	}
	else
	{
		D_801e6da8--;
	}
}

/* 0x800fb258: slots 170, 171 done: the section ends, else one replay less */
void sub_800fb258(void)
{
	if(wave_slot_done(170) && wave_slot_done(171))
	{
		D_801e6dcc = 0;
	}
	else
	{
		D_801e6dcc--;
	}
}

/* 0x800fb2b4: slots 172, 173 done: the section ends, else one replay less */
void sub_800fb2b4(void)
{
	if(wave_slot_done(172) && wave_slot_done(173))
	{
		D_801e6df0 = 0;
	}
	else
	{
		D_801e6df0--;
	}
}

/* 0x800fb310: slots 174, 175 done: the section ends, else one replay less */
void sub_800fb310(void)
{
	if(wave_slot_done(174) && wave_slot_done(175))
	{
		D_801e6e18 = 0;
	}
	else
	{
		D_801e6e18--;
	}
}

/* 0x800fb36c: slots 177, 178 done: the section ends, else one replay less */
void sub_800fb36c(void)
{
	if(wave_slot_done(177) && wave_slot_done(178))
	{
		D_801e6e3c = 0;
	}
	else
	{
		D_801e6e3c--;
	}
}

/* 0x800fb3c8: slots 185, 186 done: the section ends, else one replay less */
void sub_800fb3c8(void)
{
	if(wave_slot_done(185) && wave_slot_done(186))
	{
		D_801e6e60 = 0;
	}
	else
	{
		D_801e6e60--;
	}
}

/* 0x800fb424: the level's last stretch, once per replay: at the 10th the stream and the
   music fade, the guns go off and the screen fades over 80 frames; the 18th ends the script */
void sub_800fb424(void)
{
	if(++D_802372e4 == 10)
	{
		fade_out_level_stream();
		disable_guns();
		fade_out_level_music();
		fade(0.0f, 80, 0);
	}
	if(D_802372e4 == 18)
	{
		movie_script_abort();
	}
}
