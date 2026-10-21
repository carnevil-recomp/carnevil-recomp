/*
 * input.c - the buttons the menus and tests read (0x801188c8-0x801189d8, data
 * 0x80167d68-0x80167d98), each a function returning nonzero while it is held, reached
 * through pointers so a game can map them to its own controls. CarnEvil has the coin door
 * only: no joysticks, the volume buttons move through menus.
 */
#include "diag.h"

extern int	get_dip_coin_current(void);

#define DIP_TEST	0x10
#define DIP_SERVICE	0x40
#define DIP_VOL_DOWN	0x800
#define DIP_VOL_UP	0x1000

/* 0x801188c8 */
static int vol_down_held(void)
{
	return get_dip_coin_current() & DIP_VOL_DOWN;
}

/* 0x801188e8 */
static int vol_up_held(void)
{
	return get_dip_coin_current() & DIP_VOL_UP;
}

/* 0x80118908 */
static int left_held(void)
{
	return 0;
}

/* 0x80118910 */
static int right_held(void)
{
	return 0;
}

/* 0x80118918 */
static int test_held(void)
{
	return get_dip_coin_current() & DIP_TEST;
}

/* 0x80118938 */
static int service_held(void)
{
	return get_dip_coin_current() & DIP_SERVICE;
}

/* 0x80118958 */
static int exit_held(void)
{
	return 0;
}

/* 0x80118960 */
static int unused_held(void)
{
	return 0;
}

/* 0x80118968 */
static int sw8_held(void)
{
	return 0;
}

/* 0x80118970 */
static int vol_up_held2(void)
{
	return get_dip_coin_current() & DIP_VOL_UP;
}

/* 0x80118990 */
static int vol_down_held2(void)
{
	return get_dip_coin_current() & DIP_VOL_DOWN;
}

/* 0x801189b0 */
static int both_volume_held(void)
{
	return (get_dip_coin_current() & (DIP_VOL_UP|DIP_VOL_DOWN)) == (DIP_VOL_UP|DIP_VOL_DOWN);
}

int	(*sw_up)(void) = vol_down_held;
int	(*sw_down)(void) = vol_up_held;
int	(*sw_left)(void) = left_held;
int	(*sw_right)(void) = right_held;
int	(*sw_select)(void) = test_held;
int	(*sw_back)(void) = service_held;
int	(*sw_exit)(void) = exit_held;
int	(*sw_unused)(void) = unused_held;
int	(*sw_8)(void) = sw8_held;
int	(*sw_vol_up)(void) = vol_up_held2;
int	(*sw_vol_down)(void) = vol_down_held2;
int	(*sw_volume_both)(void) = both_volume_held;
