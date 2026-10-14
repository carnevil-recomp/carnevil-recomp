/*
 * texmaps.c - the shared and per-level texmap loaders (0x800d8c60-0x800d90c8). The tables
 * are in texmaps_data.c (generated). What a level gets depends on the violence adjustment
 * (violence_level: 2 and up shows blood, 1 the non-violent splats, 0 smoke) and, for the
 * Freak Show, on adjustment 0x3a (D_801e6ed0).
 */
#include "texmap.h"

extern int	violence_level;				/* 0x801de8a0 */
extern int	D_801e6ed0;				/* adjust.c: adjustment 0x3a */
extern texmap_t	main_weapon_texmap[0x1d], blood_drops_texmap[4], blood_drops_nv_texmap[4], text_texmap[1],
		smoketall_texmap[1], gunsmoke_texmap[5], explosion_texmap[8], blood_texmap[4], bloodnv_texmap[4],
		smokesplat_texmap[4], fire_texmap[5], spark_texmap[5], hhouse_texmap[4], hhouse_blood_texmap[2],
		hhouse_noblood_texmap[2], freakshow_texmap[3], freakshow_alt_texmap[3], freakshow_blood_texmap[3],
		freakshow_noblood_texmap[3], rickity_texmap[1], rickity_blood_texmap[2], rickity_noblood_texmap[2],
		btop_texmap[0x13], blood_btop_texmap[3], noblood_btop_texmap[3];

/* 0x800d8c60: every level: the weapon sprites, smoke, text, the splats and drops, fire */
void load_misc_texmaps(void)
{
	load_texmap(main_weapon_texmap, 0x1d, "main");
	load_texmap(smoketall_texmap, 1, "smoke");
	load_texmap(gunsmoke_texmap, 5, "gunsmoke");
	load_texmap(text_texmap, 1, "text");
	if(violence_level >= 2)
	{
		load_texmap(blood_texmap, 4, "splat");
		load_texmap(blood_drops_texmap, 4, "blood");
	}
	else
	{
		load_texmap(violence_level == 0 ? smokesplat_texmap : bloodnv_texmap, 4, "splat");
		load_texmap(blood_drops_nv_texmap, 4, "blood");
	}
	load_texmap(fire_texmap, 5, "fire");
}

/* 0x800d8d6c */
void load_hhouse_texmap(void)
{
	load_texmap(explosion_texmap, 8, "explosion");
	load_texmap(spark_texmap, 5, "spark");
	load_texmap(hhouse_texmap, 4, "hhou");
	{
		texmap_t *t;

		t = hhouse_noblood_texmap;
		if(violence_level >= 2)
			t = hhouse_blood_texmap;
		load_texmap(t, 2, "hhou blood");
	}
}

/* 0x800d8e00 */
void remove_hhouse_texmap(void)
{
	{
		texmap_t *t;

		t = hhouse_noblood_texmap;
		if(violence_level >= 2)
			t = hhouse_blood_texmap;
		remove_surface(t, 2);
	}
	remove_surface(hhouse_texmap, 4);
	remove_surface(spark_texmap, 5);
	remove_surface(explosion_texmap, 8);
}

/* 0x800d8e70 */
void load_freakshow_texmap(void)
{
	{
		texmap_t *t;

		t = freakshow_alt_texmap;
		if(D_801e6ed0)
			t = freakshow_texmap;
		load_texmap(t, 3, "frek");
	}
	{
		texmap_t *t;

		t = freakshow_noblood_texmap;
		if(violence_level >= 2)
			t = freakshow_blood_texmap;
		load_texmap(t, 3, "frek blood");
	}
}

/* 0x800d8ee8 */
void remove_freakshow_texmap(void)
{
	{
		texmap_t *t;

		t = freakshow_noblood_texmap;
		if(violence_level >= 2)
			t = freakshow_blood_texmap;
		remove_surface(t, 3);
	}
	remove_surface(freakshow_texmap, 3);
}

/* 0x800d8f38 */
void load_rickity_texmap(void)
{
	load_texmap(rickity_texmap, 1, "rick");
	{
		texmap_t *t;

		t = rickity_noblood_texmap;
		if(violence_level >= 2)
			t = rickity_blood_texmap;
		load_texmap(t, 2, "rick blood");
	}
}

/* 0x800d8f9c */
void remove_rickity_texmap(void)
{
	{
		texmap_t *t;

		t = rickity_noblood_texmap;
		if(violence_level >= 2)
			t = rickity_blood_texmap;
		remove_surface(t, 2);
	}
	remove_surface(rickity_texmap, 1);
}

/* 0x800d8fec */
void load_btop_gore_texmaps(void)
{
	load_texmap(explosion_texmap, 8, "explosion");
	load_texmap(btop_texmap, 0x13, "btop");
	{
		texmap_t *t;

		t = noblood_btop_texmap;
		if(violence_level >= 2)
			t = blood_btop_texmap;
		load_texmap(t, 3, "btop blood");
	}
}

/* 0x800d9068 */
void remove_btop_gore_texmaps(void)
{
	{
		texmap_t *t;

		t = noblood_btop_texmap;
		if(violence_level >= 2)
			t = blood_btop_texmap;
		remove_surface(t, 3);
	}
	remove_surface(btop_texmap, 0x13);
	remove_surface(explosion_texmap, 8);
}
