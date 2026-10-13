/*
 * shoot.c - the light-gun shots (0x800d4f6c-0x800d6768: sampling the guns, the hit tests, the
 * players' weapons). Only three small helpers are in C so far.
 */
#include "credits.h"			/* player_stats_t */
#include "camera.h"			/* guns */

int	D_801f8848;			/* 1 while a level plays (set at its start, cleared with the game) */

/* 0x800d6718 */
void set_player_gun(player_stats_t *p)
{
	if(p->index == 0)
	{
		p->gun = &guns[0];
	}
	else
	{
		p->gun = &guns[1];
	}
}

/* 0x800d6744, 0x800d6758 */
void func_800d6744(void)
{
	D_801f8848 = 1;
}

void func_800d6758(void)
{
	D_801f8848 = 0;
}
