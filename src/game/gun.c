/*
 * gun.c - light-gun interface reset (0x800d4ac8 kss_reset_, 0x800d4b98 kss_reset_w_calib_).
 * The gun board's control byte lives at 0xb7f00000; the calibration constants are the
 * factory defaults, kss_reset_w_calib_ takes them from the CMOS instead.
 */
#include <stdio.h>

extern void	cmos_calibration_(void);		/* 0x800d4d00 */

float	gun_cal[4];				/* 0x801e70bc: P1 x scale, y scale, x offset, y offset */
float	gun_cal2[4];				/* 0x801e70e0: P2 */
int	D_801f8848;

#define GUN_CONTROL	(*(signed char *)0xb7f00000)

/* 0x800d4ac8 */
int kss_reset_(void)
{
	int	v;

	gun_cal[0] = 4.0235300f;
	gun_cal[1] = 0.94736803f;
	gun_cal[2] = 285.52942f;
	gun_cal[3] = 25.052631f;
	gun_cal2[0] = 4.0117650f;
	gun_cal2[1] = 0.89473701f;
	gun_cal2[2] = 271.76471f;
	v = GUN_CONTROL;
	gun_cal2[3] = 39.105263f;
	printf("NSS reset = %x\n", v);
	GUN_CONTROL = v | 0x10;
	D_801f8848 = 0;
	return(2);
}

/* 0x800d4b98 */
int kss_reset_w_calib_(void)
{
	int	v;

	v = GUN_CONTROL;
	printf("NSS reset = %x\n", v);
	GUN_CONTROL = v | 0x10;
	cmos_calibration_();
	D_801f8848 = 0;
	return(2);
}
