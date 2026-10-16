/* pickups_data.c - the six pickups' texmap and submesh tables (0x80191d78-0x80191fd0) and the
   health pickup's pulse, generated with
     tools/extract/extract_tables.py texmap shotgun_texmap 0x80191d78 2
     tools/extract/extract_tables.py submesh shotgun_submesh 0x80191db8 1
     tools/extract/extract_tables.py texmap health_texmap 0x80191dd8 1
     tools/extract/extract_tables.py submesh health_submesh 0x80191df8 1
     tools/extract/extract_tables.py texmap ammo_texmap 0x80191e70 2
     tools/extract/extract_tables.py submesh ammo_submesh 0x80191eb0 1
     tools/extract/extract_tables.py texmap acid_texmap 0x80191ed0 1
     tools/extract/extract_tables.py submesh acid_submesh 0x80191ef0 1
     tools/extract/extract_tables.py texmap machinegun_texmap 0x80191f10 2
     tools/extract/extract_tables.py submesh machinegun_submesh 0x80191f50 1
     tools/extract/extract_tables.py texmap flamethrower_texmap 0x80191f70 2
     tools/extract/extract_tables.py submesh flamethrower_submesh 0x80191fb0 1
   (health_pulse, the 22 floats at 0x80191e18, copied by hand: the beat reads the first 21, the
   last is a 0.0 the table carries). Edit those, not this. */
#include "texmap.h"
#include "model.h"

texmap_t	shotgun_texmap[2] =		/* 0x80191d78 */
{
	{ (int)0x00002800, 1, (int)0x00ffffff, 0, "shot02.WMS" },
	{ (int)0x00002801, 1, (int)0x00ffffff, 0, "shot01.WMS" },
};

submesh_t	shotgun_submesh[1] =		/* 0x80191db8 */
{
	{ 0, 51, 0, 0, 27, 0, 27, 0.193333f, { 0.0f, 0.0f, 0.0f } },
};

texmap_t	health_texmap[1] =		/* 0x80191dd8 */
{
	{ (int)0x00002000, 1, (int)0x00ffffff, 0, "health.WMS" },
};

submesh_t	health_submesh[1] =		/* 0x80191df8 */
{
	{ 0, 67, 0, 0, 35, 0, 39, 0.301499993f, { -0.00150000001f, 0.3495f, 0.00650000013f } },
};

/* 0x80191e18: the health pickup's beat: its scale (times 0.5) frame by frame */
float		health_pulse[22] =
{
	1.0f, 1.0f, 1.08000004f, 1.15999997f, 1.24000001f, 1.12f, 1.0f, 1.0f, 1.0f, 1.08000004f, 1.15999997f, 1.24000001f, 1.12f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f
};

texmap_t	ammo_texmap[2] =		/* 0x80191e70 */
{
	{ (int)0x00002200, 1, (int)0x00ffffff, 0, "ammo04.WMS" },
	{ (int)0x00002201, 1, (int)0x00ffffff, 0, "ammo01.WMS" },
};

submesh_t	ammo_submesh[1] =		/* 0x80191eb0 */
{
	{ 0, 143, 0, 0, 73, 0, 73, 0.151832998f, { 0.0f, 0.0f, 0.0535000004f } },
};

texmap_t	acid_texmap[1] =		/* 0x80191ed0 */
{
	{ (int)0x00002400, 1, (int)0x00ffffff, 0, "acidsyd.WMS" },
};

submesh_t	acid_submesh[1] =		/* 0x80191ef0 */
{
	{ 0, 93, 0, 0, 48, 0, 65, 0.176667005f, { 0.0250000004f, 0.103500001f, 0.00850000046f } },
};

texmap_t	machinegun_texmap[2] =		/* 0x80191f10 */
{
	{ (int)0x00002600, 1, (int)0x00ffffff, 0, "ammosyd.WMS" },
	{ (int)0x00002601, 1, (int)0x00ffffff, 0, "ammobot.WMS" },
};

submesh_t	machinegun_submesh[1] =		/* 0x80191f50 */
{
	{ 0, 271, 0, 0, 143, 0, 36, 0.256832987f, { -0.0189999994f, 0.296499997f, 0.057f } },
};

texmap_t	flamethrower_texmap[2] =		/* 0x80191f70 */
{
	{ (int)0x00002a00, 1, (int)0x00ffffff, 0, "flametnk.WMS" },
	{ (int)0x00002a01, 1, (int)0x00ffffff, 0, "flamtop.WMS" },
};

submesh_t	flamethrower_submesh[1] =		/* 0x80191fb0 */
{
	{ 0, 183, 0, 0, 93, 0, 78, 0.272666991f, { 0.00600000005f, 0.0815000013f, -0.0445000008f } },
};
