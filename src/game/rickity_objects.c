/*
 * rickity_objects.c - the Rickety Town objects (part of the file at 0x8010fe84-0x80110270:
 * the thrown food, presents and coal; load_junk and the object code are not decompiled yet).
 */
#include "texmap.h"

extern texmap_t	rickitytown_food_texmap[8], rickitytown_present_texmap[4], coal_texmap[1];

/* 0x8010fe84 */
void load_rickity_object_texmaps(void)
{
	load_texmap(rickitytown_food_texmap, 8, "food");
	load_texmap(rickitytown_present_texmap, 4, "pres");
	load_texmap(coal_texmap, 1, "coal");
}
