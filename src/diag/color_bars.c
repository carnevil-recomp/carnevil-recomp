/*
 * color_bars.c - the monitor menu's colour bars (0x801040a0-0x801041e8): eight columns, each a
 * ramp of 32 blocks from black up to its colour.
 */
#include <goose/sprites.h>
#include "diag.h"

extern void	del1c(int, int);

static image_info_t	bar_ii = { 0, 32.0F, 32.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F };
static int	bar_start[8] = { 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000 };
static int	bar_step[8] = { 0x000800, 0x080000, 0x000008, 0x000000, 0x080808, 0x080800, 0x080008, 0x000808 };

/* 0x801040a0 */
int color_bars(void)
{
	float	x, y;
	int	i, color, step;

	bar_ii.w = hres / 8.0F;
	bar_ii.h = vres / 32.0F;
	for(x = 0.0F, i = 0; x < hres; x += hres / 8.0F, i++)
	{
		color = bar_start[i];
		step = bar_step[i];
		for(y = 0.0F; y < vres; y += vres / 32.0F)
		{
			beginobj(&bar_ii, x, y, 1.0F, 0)->state.constant_color = color;
			color += step;
		}
	}
	wait_prompt(0);
	del1c(0, 0);
	return 0;
}
