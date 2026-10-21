/*
 * point.c - one flat-coloured pixel as a sprite (0x801018c8-0x80101968).
 */
#include <goose/sprites.h>
#include "diag.h"

/* 0x801018c8 */
sprite_info_t *draw_point(float x, float y, float z, int color, int id)
{
	image_info_t	ii = { 0, 1.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F };
	sprite_info_t	*s;

	s = beginobj(&ii, x, y, z, 0);
	if(s)
	{
		s->state.constant_color = color;
		s->id = id;
	}
	return s;
}
