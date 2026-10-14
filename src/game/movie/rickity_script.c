/*
 * rickity_script.c - the Rickety Town movie script's callbacks (0x80123f88-0x80124c68).
 * The script (rickity_town_script in movie_scripts.c) replays a short stretch of the
 * movie while the enemies of a section are alive: each replay calls one of the gate
 * functions below, which clears the section's timer once every spawn table slot it
 * names is done (wave_slot_done) and otherwise counts the timer down, so a section
 * ends after at most 32 replays (rickity_script_init). Timers 23-30 and the five
 * counters are set up but never read: their helpers are not referenced anywhere.
 */
#include "spawn.h"

int	rickity_gate10_timer;		/* 0x801e6b7c */
int	rickity_gate1_timer;		/* 0x801e6b80 */
int	rickity_gate20_timer;		/* 0x801e6b90 */
int	rickity_gate11_timer;		/* 0x801e6b94 */
int	rickity_gate2_timer;		/* 0x801e6b98 */
int	rickity_gate21_timer;		/* 0x801e6bb0 */
int	rickity_gate12_timer;		/* 0x801e6bb4 */
int	rickity_gate3_timer;		/* 0x801e6bb8 */
int	rickity_gate23_timer;		/* 0x801e6bd0 */
int	rickity_gate13_timer;		/* 0x801e6bd4 */
int	rickity_gate4_timer;		/* 0x801e6bd8 */
int	rickity_gate24_timer;		/* 0x801e6bf0 */
int	rickity_gate14_timer;		/* 0x801e6bf4 */
int	rickity_gate5_timer;		/* 0x801e6bf8 */
int	rickity_gate25_timer;		/* 0x801e6c14 */
int	rickity_gate15_timer;		/* 0x801e6c20 */
int	rickity_gate6_timer;		/* 0x801e6c24 */
int	rickity_gate26_timer;		/* 0x801e6c38 */
int	rickity_gate16_timer;		/* 0x801e6c40 */
int	rickity_gate7_timer;		/* 0x801e6c44 */
int	rickity_gate27_timer;		/* 0x801e6c54 */
int	rickity_gate17_timer;		/* 0x801e6c5c */
int	rickity_gate8_timer;		/* 0x801e6c60 */
int	rickity_gate28_timer;		/* 0x801e6c70 */
int	rickity_gate18_timer;		/* 0x801e6c78 */
int	rickity_gate9_timer;		/* 0x801e6c7c */
int	rickity_gate29_timer;		/* 0x801e6c88 */
int	rickity_gate19_timer;		/* 0x801e6c90 */
int	rickity_gate30_timer;		/* 0x801e6d58 */
int	rickity_gate22_timer;		/* 0x80212d70 */

int	D_801e6bac;			/* counter 1: 12 at level start, never read */
int	D_801e6bcc;			/* counter 2: 12 at level start, never read */
int	D_801e6bec;			/* counter 3: 12 at level start, never read */
int	D_801e6c10;			/* counter 4: 12 at level start, never read */
int	D_801e6c34;			/* counter 5: 12 at level start, never read */

/* 0x80123f88: script entry: every section gets 32 replays (section 3 gets 33) */
void rickity_script_init(void)
{
	rickity_gate1_timer = 32;
	rickity_gate2_timer = 32;
	rickity_gate3_timer = 33;
	rickity_gate4_timer = 32;
	rickity_gate5_timer = 32;
	rickity_gate6_timer = 32;
	rickity_gate7_timer = 32;
	rickity_gate8_timer = 32;
	rickity_gate9_timer = 32;
	rickity_gate10_timer = 32;
	rickity_gate11_timer = 32;
	rickity_gate12_timer = 32;
	rickity_gate13_timer = 32;
	rickity_gate14_timer = 32;
	rickity_gate15_timer = 32;
	rickity_gate16_timer = 32;
	rickity_gate17_timer = 32;
	rickity_gate18_timer = 32;
	rickity_gate19_timer = 32;
	rickity_gate20_timer = 32;
	rickity_gate21_timer = 32;
	rickity_gate22_timer = 32;
	rickity_gate23_timer = 32;
	rickity_gate24_timer = 32;
	rickity_gate25_timer = 32;
	rickity_gate26_timer = 32;
	rickity_gate27_timer = 32;
	rickity_gate28_timer = 32;
	rickity_gate29_timer = 32;
	rickity_gate30_timer = 32;
	D_801e6bac = 12;
	D_801e6bcc = 12;
	D_801e6bec = 12;
	D_801e6c10 = 12;
	D_801e6c34 = 12;
}

/* 0x801240b4: unreferenced */
void func_801240b4(void)
{
	D_801e6bac = 8;
	D_801e6bcc = 2;
	D_801e6bec = 8;
	D_801e6c10 = 2;
}

/* 0x801240e4: section 1: slots 1, 2, 0 */
void rickity_gate1(void)
{
	if(wave_slot_done(1) && wave_slot_done(2) && wave_slot_done(0))
	{
		rickity_gate1_timer = 0;
	}
	else
	{
		rickity_gate1_timer--;
	}
}

/* 0x80124150: section 2: slots 3, 4, 8 */
void rickity_gate2(void)
{
	if(wave_slot_done(3) && wave_slot_done(4) && wave_slot_done(8))
	{
		rickity_gate2_timer = 0;
	}
	else
	{
		rickity_gate2_timer--;
	}
}

/* 0x801241bc: section 3: slots 15, 16, 17, 18, 19, 20 */
void rickity_gate3(void)
{
	if(wave_slot_done(15) && wave_slot_done(16) && wave_slot_done(17) && wave_slot_done(18) && wave_slot_done(19) && wave_slot_done(20))
	{
		rickity_gate3_timer = 0;
	}
	else
	{
		rickity_gate3_timer--;
	}
}

/* 0x80124258: section 4: slots 22, 23 */
void rickity_gate4(void)
{
	if(wave_slot_done(22) && wave_slot_done(23))
	{
		rickity_gate4_timer = 0;
	}
	else
	{
		rickity_gate4_timer--;
	}
}

/* 0x801242b4: section 5: slots 24, 25, 26 */
void rickity_gate5(void)
{
	if(wave_slot_done(24) && wave_slot_done(25) && wave_slot_done(26))
	{
		rickity_gate5_timer = 0;
	}
	else
	{
		rickity_gate5_timer--;
	}
}

/* 0x80124320: section 6: slots 28, 29, 30, 31, 32 */
void rickity_gate6(void)
{
	if(wave_slot_done(28) && wave_slot_done(29) && wave_slot_done(30) && wave_slot_done(31) && wave_slot_done(32))
	{
		rickity_gate6_timer = 0;
	}
	else
	{
		rickity_gate6_timer--;
	}
}

/* 0x801243ac: section 7: slots 33, 34, 35, 36, 37 */
void rickity_gate7(void)
{
	if(wave_slot_done(33) && wave_slot_done(34) && wave_slot_done(35) && wave_slot_done(36) && wave_slot_done(37))
	{
		rickity_gate7_timer = 0;
	}
	else
	{
		rickity_gate7_timer--;
	}
}

/* 0x80124438: section 8: slots 38, 39, 40 */
void rickity_gate8(void)
{
	if(wave_slot_done(38) && wave_slot_done(39) && wave_slot_done(40))
	{
		rickity_gate8_timer = 0;
	}
	else
	{
		rickity_gate8_timer--;
	}
}

/* 0x801244a4: section 9: slots 42, 43, 44 */
void rickity_gate9(void)
{
	if(wave_slot_done(42) && wave_slot_done(43) && wave_slot_done(44))
	{
		rickity_gate9_timer = 0;
	}
	else
	{
		rickity_gate9_timer--;
	}
}

/* 0x80124510: section 10: slots 45, 46, 47, 48, 49 */
void rickity_gate10(void)
{
	if(wave_slot_done(45) && wave_slot_done(46) && wave_slot_done(47) && wave_slot_done(48) && wave_slot_done(49))
	{
		rickity_gate10_timer = 0;
	}
	else
	{
		rickity_gate10_timer--;
	}
}

/* 0x8012459c: section 11: slots 51, 52, 53, 54, 55 */
void rickity_gate11(void)
{
	if(wave_slot_done(51) && wave_slot_done(52) && wave_slot_done(53) && wave_slot_done(54) && wave_slot_done(55))
	{
		rickity_gate11_timer = 0;
	}
	else
	{
		rickity_gate11_timer--;
	}
}

/* 0x80124628: section 12: slots 62, 63, 66 */
void rickity_gate12(void)
{
	if(wave_slot_done(62) && wave_slot_done(63) && wave_slot_done(66))
	{
		rickity_gate12_timer = 0;
	}
	else
	{
		rickity_gate12_timer--;
	}
}

/* 0x80124694: section 13: slots 68, 70 */
void rickity_gate13(void)
{
	if(wave_slot_done(68) && wave_slot_done(70))
	{
		rickity_gate13_timer = 0;
	}
	else
	{
		rickity_gate13_timer--;
	}
}

/* 0x801246f0: section 14: slots 72, 73, 74, 75, 76, 77 */
void rickity_gate14(void)
{
	if(wave_slot_done(72) && wave_slot_done(73) && wave_slot_done(74) && wave_slot_done(75) && wave_slot_done(76) && wave_slot_done(77))
	{
		rickity_gate14_timer = 0;
	}
	else
	{
		rickity_gate14_timer--;
	}
}

/* 0x8012478c: section 15: slots 80, 81, 82, 83, 84, 85, 86 */
void rickity_gate15(void)
{
	if(wave_slot_done(80) && wave_slot_done(81) && wave_slot_done(82) && wave_slot_done(83) && wave_slot_done(84) && wave_slot_done(85) && wave_slot_done(86))
	{
		rickity_gate15_timer = 0;
	}
	else
	{
		rickity_gate15_timer--;
	}
}

/* 0x80124838: section 16: slots 87, 88, 89 */
void rickity_gate16(void)
{
	if(wave_slot_done(87) && wave_slot_done(88) && wave_slot_done(89))
	{
		rickity_gate16_timer = 0;
	}
	else
	{
		rickity_gate16_timer--;
	}
}

/* 0x801248a4: section 17: slots 95, 91, 92, 93, 94, 96, 97 */
void rickity_gate17(void)
{
	if(wave_slot_done(95) && wave_slot_done(91) && wave_slot_done(92) && wave_slot_done(93) && wave_slot_done(94) && wave_slot_done(96) && wave_slot_done(97))
	{
		rickity_gate17_timer = 0;
	}
	else
	{
		rickity_gate17_timer--;
	}
}

/* 0x80124950: section 18: slots 98 */
void rickity_gate18(void)
{
	if(wave_slot_done(98))
	{
		rickity_gate18_timer = 0;
	}
	else
	{
		rickity_gate18_timer--;
	}
}

/* 0x8012499c: section 19: slots 99 */
void rickity_gate19(void)
{
	if(wave_slot_done(99))
	{
		rickity_gate19_timer = 0;
	}
	else
	{
		rickity_gate19_timer--;
	}
}

/* 0x801249e8: section 20: slots 124 */
void rickity_gate20(void)
{
	if(wave_slot_done(124))
	{
		rickity_gate20_timer = 0;
	}
	else
	{
		rickity_gate20_timer--;
	}
}

/* 0x80124a34: section 21: slots 125, 126, 127 */
void rickity_gate21(void)
{
	if(wave_slot_done(125) && wave_slot_done(126) && wave_slot_done(127))
	{
		rickity_gate21_timer = 0;
	}
	else
	{
		rickity_gate21_timer--;
	}
}

/* 0x80124aa0: section 22: slots 131, 132 */
void rickity_gate22(void)
{
	if(wave_slot_done(131) && wave_slot_done(132))
	{
		rickity_gate22_timer = 0;
	}
	else
	{
		rickity_gate22_timer--;
	}
}

/* 0x80124afc: unreferenced */
void dec_rickity_gate23_timer(void)
{
	rickity_gate23_timer--;
}

/* 0x80124b18: unreferenced */
void dec_rickity_gate24_timer(void)
{
	rickity_gate24_timer--;
}

/* 0x80124b34: unreferenced */
void dec_rickity_gate25_timer(void)
{
	rickity_gate25_timer--;
}

/* 0x80124b50: unreferenced */
void dec_rickity_gate26_timer(void)
{
	rickity_gate26_timer--;
}

/* 0x80124b6c: unreferenced */
void dec_rickity_gate27_timer(void)
{
	rickity_gate27_timer--;
}

/* 0x80124b88: unreferenced */
void dec_rickity_gate28_timer(void)
{
	rickity_gate28_timer--;
}

/* 0x80124ba4: unreferenced */
void dec_rickity_gate29_timer(void)
{
	rickity_gate29_timer--;
}

/* 0x80124bc0: unreferenced */
void dec_rickity_gate30_timer(void)
{
	rickity_gate30_timer--;
}

/* 0x80124bdc: unreferenced */
void func_80124bdc(void)
{
	D_801e6bac--;
}

/* 0x80124bf8: unreferenced */
void func_80124bf8(void)
{
	D_801e6bcc--;
}

/* 0x80124c14: unreferenced */
void func_80124c14(void)
{
	D_801e6bec--;
}

/* 0x80124c30: unreferenced */
void func_80124c30(void)
{
	D_801e6c10--;
}

/* 0x80124c4c: unreferenced */
void func_80124c4c(void)
{
	D_801e6c34--;
}
