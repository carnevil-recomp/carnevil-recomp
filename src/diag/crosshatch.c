/*
 * crosshatch.c - the monitor menu's cross hatch (0x801041e8-0x8010440c): lines an eighth of the
 * screen apart, the last on the far edge, and a dot in the middle of each square.
 */
#include <goose/sprites.h>
#include "diag.h"

extern void	del1c(int, int);

static image_info_t	hatch_ii = { 0, 1.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F };

/* 0x801041e8 */
int crosshatch(void)
{
	float	x, y;

	/* the vertical lines (their height is the screen's width, which covers it) */
	hatch_ii.h = hres;
	for(x = 0.0F; x < hres; x += hres / 8.0F)
		beginobj(&hatch_ii, x, 0.0F, 1.0F, 0);
	x -= 1.0F;
	beginobj(&hatch_ii, x, 0.0F, 1.0F, 0);

	/* the horizontal lines */
	hatch_ii.h = 1.0F;
	hatch_ii.w = hres;
	for(y = 0.0F; y < vres; y += vres / 8.0F)
		beginobj(&hatch_ii, 0.0F, y, 1.0F, 0);
	y -= 1.0F;
	beginobj(&hatch_ii, 0.0F, y, 1.0F, 0);

	/* the dots */
	hatch_ii.h = 1.0F;
	hatch_ii.w = 1.0F;
	for(x = hres / 8.0F / 2.0F; x < hres; x += hres / 8.0F)
		for(y = vres / 8.0F / 2.0F; y < vres; y += vres / 8.0F)
			beginobj(&hatch_ii, x, y, 1.0F, 0);
	wait_prompt(0);
	del1c(0, 0);
	return 0;
}
