/*
 * hhouse_innocent.c - the innocent girl's file (0x80105388-0x801058bc and on: the Haunted
 * House's innocent, object 0x2005, and the textures both innocents share). Only the texture
 * loader is in C so far; the rest of the file is still assembly and its object stubbed.
 * The game loads these textures once at boot (main_proc), so Rickety Town's girl
 * (rickity_innocent.c) draws with them too; her own copy of the table is never loaded.
 */
#include "texmap.h"

extern texmap_t	inng_texmap[0x17];		/* hhouse_innocent_data.c */

/* 0x80105794 */
void load_inng_texmap(void)
{
	load_texmap(inng_texmap, 0x17, "inng");
}
