/*
 * bigtop_script.c - the Big Top movie script's callbacks (0x8012fa20-0x80130bf8; krampus.c ends
 * at 0x8012fa20, the poodle's file begins at 0x80130bf8). The script (bigtop_script in
 * movie_scripts.c) replays a stretch of the movie while a section's enemies are alive, calling one
 * of the functions below each replay: most clear the section's timer once every spawn table slot
 * they name is done (wave_slot_done) and otherwise count it down; some count down only once their
 * slots are done or once one of the toke file's tests (func_8013b1dc-func_8013b49c) says so.
 * sub_8012fa20 is the script entry: every section's replay budget. The exported names are the
 * ones movie_scripts.c uses (generated from names.txt).
 */
#include "spawn.h"

extern int	func_8013b214(void), func_8013b2f8(void), func_8013b36c(void), func_8013b3e0(void);
extern int	func_8013b454(void), func_8013b49c(void);
extern void	func_8013b1dc(void), func_8013b288(void);

int	D_801e6d64;
int	D_801e6d68;
int	D_801e6d7c;
int	D_801e6d80;
int	D_801e6d84;
int	D_801e6d98;
int	D_801e6d9c;
int	D_801e6da0;
int	D_801e6da4;
int	D_801e6db8;
int	D_801e6dbc;
int	D_801e6dc0;
int	D_801e6dc4;
int	D_801e6dc8;
int	D_801e6ddc;
int	D_801e6de0;
int	D_801e6de4;
int	D_801e6de8;
int	D_801e6dec;
int	D_801e6e04;
int	D_801e6e08;
int	D_801e6e0c;
int	D_801e6e10;
int	D_801e6e14;
int	D_801e6e28;
int	D_801e6e2c;
int	D_801e6e30;
int	D_801e6e34;
int	D_801e6e38;
int	D_801e6e4c;
int	D_801e6e50;
int	D_801e6e54;
int	D_801e6e58;
int	D_801e6e5c;
int	D_801e6e74;
int	D_801e6e78;
int	D_801e6e7c;
int	D_801e6e80;
int	D_801e6e84;
int	D_801e6e98;
int	D_801e6e9c;
int	D_801e6ea0;
int	D_801e6ea8;
int	D_801e6eac;
int	D_801e6eb0;
int	D_801fac80;

/* 0x8012fa20: the script entry: every section's replay budget */
void sub_8012fa20(void)
{
	D_801e6d68 = 32;
	D_801e6da4 = 33;
	D_801e6dc8 = 52;
	D_801e6dc0 = 7;
	D_801e6d84 = 32;
	D_801e6dec = 32;
	D_801e6e14 = 32;
	D_801e6e38 = 32;
	D_801e6e5c = 32;
	D_801e6e84 = 32;
	D_801e6d64 = 32;
	D_801e6d80 = 32;
	D_801e6da0 = 32;
	D_801e6dc4 = 32;
	D_801e6de8 = 32;
	D_801e6e10 = 32;
	D_801fac80 = 32;
	D_801e6e34 = 32;
	D_801e6e58 = 32;
	D_801e6e80 = 32;
	D_801e6ea0 = 32;
	D_801e6d7c = 32;
	D_801e6d9c = 32;
	D_801e6de4 = 32;
	D_801e6e0c = 32;
	D_801e6e30 = 32;
	D_801e6e54 = 2;
	D_801e6e7c = 3;
	D_801e6e9c = 11;
	D_801e6eac = 11;
	D_801e6d98 = 4;
	D_801e6dbc = 41;
	D_801e6de0 = 4;
	D_801e6e08 = 41;
	D_801e6e2c = 41;
	D_801e6e50 = 42;
	D_801e6e78 = 4;
	D_801e6e98 = 4;
	D_801e6ea8 = 4;
	D_801e6eb0 = 41;
	D_801e6db8 = 41;
	D_801e6ddc = 4;
	D_801e6e04 = 41;
	D_801e6e28 = 41;
	D_801e6e4c = 41;
	D_801e6e74 = 11;
}

/* 0x8012fbc0: */
void func_8012fbc0(void)
{
	D_801e6d98 = 8;
	D_801e6dbc = 2;
	D_801e6de0 = 8;
	D_801e6e08 = 2;
}

/* 0x8012fbf0: */
void sub_8012fbf0(void)
{
	if(wave_slot_done(1))
	{
		D_801e6d68 = 0;
	}
	else
	{
		D_801e6d68--;
	}
}

/* 0x8012fc3c: */
void sub_8012fc3c(void)
{
	if(wave_slot_done(3) && wave_slot_done(4) && wave_slot_done(5))
	{
		D_801e6d84 = 0;
	}
	else
	{
		D_801e6d84--;
	}
}

/* 0x8012fca8: */
void sub_8012fca8(void)
{
	if(wave_slot_done(8) && wave_slot_done(9) && wave_slot_done(10))
	{
		D_801e6da4 = 0;
	}
	else
	{
		D_801e6da4--;
	}
}

/* 0x8012fd14: */
void sub_8012fd14(void)
{
	if(wave_slot_done(11) && wave_slot_done(12) && wave_slot_done(15))
	{
		D_801e6dc8 = 0;
	}
	else
	{
		D_801e6dc8--;
	}
}

/* 0x8012fd80: */
void sub_8012fd80(void)
{
	if(wave_slot_done(30) && wave_slot_done(31) && wave_slot_done(32) && wave_slot_done(29))
	{
		D_801e6dec = 0;
	}
	else
	{
		D_801e6dec--;
	}
}

/* 0x8012fdfc: */
void sub_8012fdfc(void)
{
	if(wave_slot_done(34) && wave_slot_done(35) && wave_slot_done(36) && wave_slot_done(37))
	{
		D_801e6e14 = 0;
	}
	else
	{
		D_801e6e14--;
	}
}

/* 0x8012fe78: */
void sub_8012fe78(void)
{
	if(wave_slot_done(38))
	{
		D_801e6e38 = 0;
	}
	else
	{
		D_801e6e38--;
	}
}

/* 0x8012fec4: */
void sub_8012fec4(void)
{
	if(wave_slot_done(41) && wave_slot_done(42) && wave_slot_done(43))
	{
		D_801e6e5c = 0;
	}
	else
	{
		D_801e6e5c--;
	}
}

/* 0x8012ff30: */
void sub_8012ff30(void)
{
	if(wave_slot_done(44) && wave_slot_done(46))
	{
		D_801e6e84 = 0;
	}
	else
	{
		D_801e6e84--;
	}
}

/* 0x8012ff8c: */
void sub_8012ff8c(void)
{
	if(wave_slot_done(49) && wave_slot_done(50) && wave_slot_done(51))
	{
		D_801e6d64 = 0;
	}
	else
	{
		D_801e6d64--;
	}
}

/* 0x8012fff8: */
void sub_8012fff8(void)
{
	if(wave_slot_done(53) && wave_slot_done(54) && wave_slot_done(55))
	{
		D_801e6d80 = 0;
	}
	else
	{
		D_801e6d80--;
	}
}

/* 0x80130064: */
void sub_80130064(void)
{
	if(wave_slot_done(59) && wave_slot_done(60) && wave_slot_done(62) && wave_slot_done(63))
	{
		D_801e6da0 = 0;
	}
	else
	{
		D_801e6da0--;
	}
}

/* 0x801300e0: */
void sub_801300e0(void)
{
	if(wave_slot_done(67))
	{
		D_801e6dc4 = 0;
	}
	else
	{
		D_801e6dc4--;
	}
}

/* 0x8013012c: */
void sub_8013012c(void)
{
	if(wave_slot_done(68) && wave_slot_done(69))
	{
		D_801e6de8 = 0;
	}
	else
	{
		D_801e6de8--;
	}
}

/* 0x80130188: */
void sub_80130188(void)
{
	if(wave_slot_done(72) && wave_slot_done(73))
	{
		D_801e6e10 = 0;
	}
	else
	{
		D_801e6e10--;
	}
}

/* 0x801301e4: */
void sub_801301e4(void)
{
	if(wave_slot_done(75) && wave_slot_done(76))
	{
		D_801fac80 = 0;
	}
	else
	{
		D_801fac80--;
	}
}

/* 0x80130240: */
void sub_80130240(void)
{
	if(wave_slot_done(82) && wave_slot_done(83))
	{
		D_801e6e34 = 0;
	}
	else
	{
		D_801e6e34--;
	}
}

/* 0x8013029c: */
void sub_8013029c(void)
{
	if(wave_slot_done(86))
	{
		D_801e6e58 = 0;
	}
	else
	{
		D_801e6e58--;
	}
}

/* 0x801302e8: */
void sub_801302e8(void)
{
	if(wave_slot_done(87))
	{
		D_801e6e80 = 0;
	}
	else
	{
		D_801e6e80--;
	}
}

/* 0x80130334: */
void sub_80130334(void)
{
	if(wave_slot_done(93) && wave_slot_done(96) && wave_slot_done(91) && wave_slot_done(97) && wave_slot_done(98))
	{
		D_801e6ea0 = 0;
	}
	else
	{
		D_801e6ea0--;
	}
}

/* 0x801303c0: */
void sub_801303c0(void)
{
	if(wave_slot_done(100) && wave_slot_done(101))
	{
		D_801e6d7c = 0;
	}
	else
	{
		D_801e6d7c--;
	}
}

/* 0x8013041c: */
void sub_8013041c(void)
{
	if(wave_slot_done(102) && wave_slot_done(103) && wave_slot_done(104))
	{
		D_801e6d9c = 0;
	}
	else
	{
		D_801e6d9c--;
	}
}

/* 0x80130488: */
void sub_80130488(void)
{
	if(wave_slot_done(105))
	{
		D_801e6dc0 = 0;
	}
	else
	{
		D_801e6dc0--;
	}
}

/* 0x801304d4: */
void sub_801304d4(void)
{
	if(wave_slot_done(107))
	{
		D_801e6de4 = 0;
	}
	else
	{
		D_801e6de4--;
	}
}

/* 0x80130520: */
void sub_80130520(void)
{
	if(wave_slot_done(108))
	{
		D_801e6e0c = 0;
	}
	else
	{
		D_801e6e0c--;
	}
}

/* 0x8013056c: */
void sub_8013056c(void)
{
	if(wave_slot_done(109))
	{
		D_801e6e30 = 0;
	}
	else
	{
		D_801e6e30--;
	}
}

/* 0x801305b8: */
void sub_801305b8(void)
{
	if(wave_slot_done(114))
	{
		D_801e6e54--;
	}
}

/* 0x801305f4: */
void sub_801305f4(void)
{
	if(wave_slot_done(115) && wave_slot_done(116) && wave_slot_done(117) && wave_slot_done(118))
	{
		func_8013b1dc();
		D_801e6e7c--;
	}
}

/* 0x80130668: */
void sub_80130668(void)
{
	if(wave_slot_done(119))
	{
		D_801e6e9c = 0;
	}
	else
	{
		D_801e6e9c--;
	}
}

/* 0x801306b4: */
void sub_801306b4(void)
{
	D_801e6eac--;
}

/* 0x801306d0: */
void sub_801306d0(void)
{
	if(func_8013b214())
	{
		D_801e6d98--;
	}
}

/* 0x8013070c: */
void sub_8013070c(void)
{
	if(wave_slot_done(120))
	{
		D_801e6dbc = 0;
	}
	else
	{
		D_801e6dbc--;
	}
}

/* 0x80130758: */
void sub_80130758(void)
{
	if(wave_slot_done(122) && wave_slot_done(123) && wave_slot_done(124) && wave_slot_done(125))
	{
		func_8013b288();
		D_801e6de0--;
	}
}

/* 0x801307cc: */
void sub_801307cc(void)
{
	if(wave_slot_done(127) && wave_slot_done(128) && wave_slot_done(129) && wave_slot_done(126))
	{
		D_801e6e08 = 0;
	}
	else
	{
		D_801e6e08--;
	}
}

/* 0x80130848: */
void sub_80130848(void)
{
	if(wave_slot_done(130) && wave_slot_done(131))
	{
		D_801e6e2c = 0;
	}
	else
	{
		D_801e6e2c--;
	}
}

/* 0x801308a4: */
void sub_801308a4(void)
{
	if(wave_slot_done(132) && wave_slot_done(133) && wave_slot_done(134) && wave_slot_done(135))
	{
		D_801e6e50 = 0;
	}
	else
	{
		D_801e6e50--;
	}
}

/* 0x80130920: */
void sub_80130920(void)
{
	if(wave_slot_done(136) && wave_slot_done(137) && wave_slot_done(138) && wave_slot_done(139))
	{
		D_801e6e78 = 0;
	}
}

/* 0x80130980: */
void sub_80130980(void)
{
	if(func_8013b2f8())
	{
		D_801e6e98--;
	}
}

/* 0x801309bc: */
void sub_801309bc(void)
{
	if(func_8013b36c())
	{
		D_801e6ea8--;
	}
}

/* 0x801309f8: */
void sub_801309f8(void)
{
	if(wave_slot_done(140))
	{
		D_801e6eb0 = 0;
	}
	else
	{
		D_801e6eb0--;
	}
}

/* 0x80130a44: */
void sub_80130a44(void)
{
	if(wave_slot_done(141) && wave_slot_done(142))
	{
		D_801e6db8 = 0;
	}
	else
	{
		D_801e6db8--;
	}
}

/* 0x80130aa0: */
void sub_80130aa0(void)
{
	if(func_8013b3e0())
	{
		D_801e6ddc--;
	}
}

/* 0x80130adc: */
void sub_80130adc(void)
{
	if(wave_slot_done(144) && wave_slot_done(145) && wave_slot_done(146) && wave_slot_done(147))
	{
		D_801e6e04 = 0;
	}
	else
	{
		D_801e6e04--;
	}
}

/* 0x80130b58: */
void sub_80130b58(void)
{
	if(func_8013b454())
	{
		D_801e6e28 = 0;
	}
}

/* 0x80130b88: */
void sub_80130b88(void)
{
	if(func_8013b49c())
	{
		D_801e6e4c = 0;
	}
}

/* 0x80130bb8: */
void func_80130bb8(void)
{
	if(wave_slot_done(113))
	{
		D_801e6e74--;
	}
}
