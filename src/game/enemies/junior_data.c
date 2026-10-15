/* junior_data.c - Junior's tables (the Freak Show boss, the big baby; with the BIG BABY
   adjustment off the teddy bear): his texmaps and submeshes for both models (0x80197848-
   0x801983e8), the puke blob's texmaps, submesh, spin and spawn offset (0x801983e8-0x80198480),
   the mouth-shape runs his voice lines are lip-synced with (0x80198480-0x80198f08, -1 ends a
   run), the run for each "baby" bank call (0x80198f08) and his voice lines (0x80198f8c).
   Generated with tools/extract/extract_tables.py and the mouth dump below: edit those, not this.
     extract_tables.py texmap baby_texmap 0x80197848 42
     extract_tables.py submesh baby_anim_blob 0x80197d88 15
     extract_tables.py texmap bear_texmap 0x80197f68 21
     extract_tables.py submesh bear_anim_blob 0x80198208 15
     extract_tables.py texmap puke_texmap 0x801983e8 3
     extract_tables.py submesh puke_submesh 0x80198448 1
     extract_tables.py floats puke_spin 0x80198468 1 3
     extract_tables.py floats puke_offset 0x80198474 1 3 */
#include "texmap.h"
#include "model.h"

texmap_t	baby_texmap[42] =		/* 0x80197848 */
{
	{ (int)0x00003500, 1, (int)0x00ffffff, 0, "babyulg1.WMS" },
	{ (int)0x00003501, 1, (int)0x00ffffff, 0, "babyurm1.WMS" },
	{ (int)0x00003502, 1, (int)0x00ffffff, 0, "babyurm2.WMS" },
	{ (int)0x00003503, 1, (int)0x00ffffff, 0, "babyllg1.WMS" },
	{ (int)0x00003504, 1, (int)0x00ffffff, 0, "babylrm1.WMS" },
	{ (int)0x00003505, 1, (int)0x00ffffff, 0, "babylrm2.WMS" },
	{ (int)0x00003506, 1, (int)0x00ffffff, 0, "babyhnd2.WMS" },
	{ (int)0x00003507, 1, (int)0x00ffffff, 0, "babybod.WMS" },
	{ (int)0x00003508, 1, (int)0x00ffffff, 0, "babyfoot.WMS" },
	{ (int)0x00003509, 1, (int)0x00ffffff, 0, "babyftbm.WMS" },
	{ (int)0x0000350a, 1, (int)0x00ffffff, 0, "babypelv.WMS" },
	{ (int)0x0000350b, 1, (int)0x00ffffff, 0, "babydiap.WMS" },
	{ (int)0x0000350c, 1, (int)0x00ffffff, 0, "babyulg2.WMS" },
	{ (int)0x0000350d, 1, (int)0x00ffffff, 0, "babyllg2.WMS" },
	{ (int)0x0000350e, 1, (int)0x00ffffff, 0, "babyhnd1.WMS" },
	{ (int)0x0000350f, 1, (int)0x00ffffff, 0, "babyfac1.WMS" },
	{ (int)0x00003510, 1, (int)0x00ffffff, 0, "babyeye.WMS" },
	{ (int)0x00003511, 1, (int)0x00ffffff, 0, "babymu01.WMS" },
	{ (int)0x00003512, 1, (int)0x00ffffff, 0, "babyear1.WMS" },
	{ (int)0x00003513, 1, (int)0x00ffffff, 0, "babyear.WMS" },
	{ (int)0x00003514, 1, (int)0x00ffffff, 0, "babyfot2.WMS" },
	{ (int)0x00003509, 1, (int)0x00ffffff, 0, "babyftbm.WMS" },
	{ (int)0xf000350b, 1, (int)0x00ffffff, 0, "babyd_05.WMS" },
	{ (int)0xf0003504, 1, (int)0x00ffffff, 0, "babyd_04.WMS" },
	{ (int)0xf0003505, 1, (int)0x00ffffff, 0, "babyd_04.WMS" },
	{ (int)0xf0003501, 1, (int)0x00ffffff, 0, "babyd_03.WMS" },
	{ (int)0xf0003502, 1, (int)0x00ffffff, 0, "babyd_03.WMS" },
	{ (int)0xf0003507, 1, (int)0x00ffffff, 0, "babyd_02.WMS" },
	{ (int)0xf000350f, 1, (int)0x00ffffff, 0, "babyd_01.WMS" },
	{ (int)0xf100350f, 1, (int)0x00ffffff, 0, "babyd_01.WMS" },
	{ (int)0xf200350f, 1, (int)0x00ffffff, 0, "babyd_01.WMS" },
	{ (int)0xf400350f, 1, (int)0x00ffffff, 0, "babyd_01.WMS" },
	{ (int)0x01003511, 1, (int)0x00ffffff, 0, "babymu02.WMS" },
	{ (int)0x02003511, 1, (int)0x00ffffff, 0, "babymu03.WMS" },
	{ (int)0x04003511, 1, (int)0x00ffffff, 0, "babymu04.WMS" },
	{ (int)0xf1003511, 1, (int)0x00ffffff, 0, "babymu02.WMS" },
	{ (int)0xf2003511, 1, (int)0x00ffffff, 0, "babymu03.WMS" },
	{ (int)0xf4003511, 1, (int)0x00ffffff, 0, "babymu04.WMS" },
	{ (int)0xf0003503, 1, (int)0x00ffffff, 0, "babyd_04.WMS" },
	{ (int)0xf000350d, 1, (int)0x00ffffff, 0, "babyd_04.WMS" },
	{ (int)0xf0003500, 1, (int)0x00ffffff, 0, "babyd_03.WMS" },
	{ (int)0xf000350c, 1, (int)0x00ffffff, 0, "babyd_03.WMS" },
};

submesh_t	baby_anim_blob[15] =		/* 0x80197d88 */
{
	{ 0, 79, 0, 0, 41, 0, 41, 0.391167015f, { -0.0225000009f, 0.179499999f, 0.00949999969f } },
	{ 80, 139, 0, 42, 73, 42, 73, 0.344999999f, { -0.023f, 0.246000007f, 0.00300000003f } },
	{ 140, 199, 0, 74, 105, 74, 105, 0.344999999f, { 0.023f, 0.246000007f, 0.00300000003f } },
	{ 200, 277, 0, 106, 146, 106, 146, 0.325832993f, { 0.0f, 0.206499994f, -0.0439999998f } },
	{ 278, 343, 0, 147, 181, 147, 183, 0.303166986f, { 0.000500000024f, 0.222499996f, 0.00449999981f } },
	{ 344, 409, 0, 182, 216, 184, 218, 0.303166986f, { -0.000500000024f, 0.222499996f, 0.00449999981f } },
	{ 410, 638, 0, 217, 350, 219, 352, 0.326332986f, { 0.188500002f, -0.139500007f, 0.00499999989f } },
	{ 639, 704, 0, 351, 385, 353, 387, 0.710667014f, { 0.0f, 0.597500026f, 0.0344999991f } },
	{ 705, 786, 0, 386, 428, 388, 430, 0.338833004f, { 0.191f, 0.0109999999f, 0.182500005f } },
	{ 787, 884, 0, 429, 484, 431, 488, 0.84216702f, { 0.0f, 0.230499998f, -0.0240000002f } },
	{ 885, 964, 0, 485, 526, 489, 530, 0.391167015f, { 0.0f, 0.179499999f, 0.00800000038f } },
	{ 965, 1042, 0, 527, 567, 531, 571, 0.325500011f, { 0.00650000013f, 0.217500001f, -0.056499999f } },
	{ 1043, 1271, 0, 568, 701, 572, 705, 0.325832993f, { 0.190500006f, 0.136999995f, 0.00499999989f } },
	{ 1272, 1623, 0, 702, 898, 706, 902, 0.682166994f, { 0.444499999f, -0.00100000005f, 0.152999997f } },
	{ 1624, 1705, 0, 899, 941, 903, 945, 0.338f, { 0.187000006f, -0.023f, 0.181999996f } },
};

texmap_t	bear_texmap[21] =		/* 0x80197f68 */
{
	{ (int)0x00003500, 1, (int)0x00ffffff, 0, "bearfur1.WMS" },
	{ (int)0x00003501, 1, (int)0x00ffffff, 0, "bearfur.WMS" },
	{ (int)0x00003500, 1, (int)0x00ffffff, 0, "bearfur1.WMS" },
	{ (int)0x00003501, 1, (int)0x00ffffff, 0, "bearfur.WMS" },
	{ (int)0x00003500, 1, (int)0x00ffffff, 0, "bearfur1.WMS" },
	{ (int)0x00003501, 1, (int)0x00ffffff, 0, "bearfur.WMS" },
	{ (int)0x00003501, 1, (int)0x00ffffff, 0, "bearfur.WMS" },
	{ (int)0x00003502, 1, (int)0x00ffffff, 0, "bearchst.WMS" },
	{ (int)0x00003502, 1, (int)0x00ffffff, 0, "bearchst.WMS" },
	{ (int)0x00003503, 1, (int)0x00ffffff, 0, "bearbutn.WMS" },
	{ (int)0x00003501, 1, (int)0x00ffffff, 0, "bearfur.WMS" },
	{ (int)0x00003500, 1, (int)0x00ffffff, 0, "bearfur1.WMS" },
	{ (int)0x00003501, 1, (int)0x00ffffff, 0, "bearfur.WMS" },
	{ (int)0x00003501, 1, (int)0x00ffffff, 0, "bearfur.WMS" },
	{ (int)0x00003504, 1, (int)0x00ffffff, 0, "bearhed2.WMS" },
	{ (int)0x00003505, 1, (int)0x00ffffff, 0, "beartung.WMS" },
	{ (int)0x00003506, 1, (int)0x00ffffff, 0, "beartoth.WMS" },
	{ (int)0x00003507, 1, (int)0x00ffffff, 0, "bearear.WMS" },
	{ (int)0x00003508, 1, (int)0x00ffffff, 0, "beareye3.WMS" },
	{ (int)0x00003509, 1, (int)0x00ffffff, 0, "bearnoz.WMS" },
	{ (int)0x00003501, 1, (int)0x00ffffff, 0, "bearfur.WMS" },
};

submesh_t	bear_anim_blob[15] =		/* 0x80198208 */
{
	{ 0, 79, 0, 0, 41, 0, 41, 0.391167015f, { -0.0225000009f, 0.179499999f, 0.00949999969f } },
	{ 80, 139, 0, 42, 73, 42, 73, 0.344999999f, { -0.023f, 0.246000007f, 0.00300000003f } },
	{ 140, 199, 0, 74, 105, 74, 105, 0.344999999f, { 0.023f, 0.246000007f, 0.00300000003f } },
	{ 200, 277, 0, 106, 146, 106, 146, 0.325832993f, { 0.0f, 0.206499994f, -0.0439999998f } },
	{ 278, 343, 0, 147, 181, 147, 181, 0.303166986f, { 0.000500000024f, 0.222499996f, 0.00449999981f } },
	{ 344, 409, 0, 182, 216, 182, 216, 0.303166986f, { -0.000500000024f, 0.222499996f, 0.00449999981f } },
	{ 979, 1174, 0, 533, 646, 534, 647, 0.340499997f, { 0.193000004f, -0.174500003f, 0.0419999994f } },
	{ 606, 671, 0, 331, 365, 331, 365, 0.710667014f, { 0.0f, 0.597500026f, 0.0344999991f } },
	{ 1668, 1749, 0, 945, 987, 942, 986, 0.338f, { 0.187000006f, 0.0195000004f, 0.182500005f } },
	{ 672, 820, 0, 366, 449, 366, 450, 0.814666986f, { -0.0109999999f, 0.208499998f, -0.0914999992f } },
	{ 821, 900, 0, 450, 491, 451, 492, 0.391167015f, { 0.0f, 0.179499999f, 0.00800000038f } },
	{ 901, 978, 0, 492, 532, 493, 533, 0.325500011f, { 0.00650000013f, 0.217500001f, -0.056499999f } },
	{ 410, 605, 0, 217, 330, 217, 330, 0.340499997f, { 0.193000004f, 0.174500003f, 0.0419999994f } },
	{ 1257, 1667, 0, 690, 944, 693, 941, 0.785667002f, { 0.496499985f, -0.00499999989f, 0.335500002f } },
	{ 1175, 1256, 0, 647, 689, 648, 692, 0.338f, { 0.187000006f, -0.0195000004f, 0.182500005f } },
};

texmap_t	puke_texmap[3] =		/* 0x801983e8 */
{
	{ (int)0x00003800, 1, (int)0x00ffffff, 0, "puke2.WMS" },
	{ (int)0x10003800, 1, (int)0x00ffffff, 0, "puke_o.WMS" },
	{ (int)0x20003800, 1, (int)0x00ffffff, 0, "puke_g.WMS" },
};

submesh_t	puke_submesh[1] =		/* 0x80198448 */
{
	{ 0, 47, 0, 0, 25, 0, 25, 0.100332998f, { -0.00800000038f, 0.00449999981f, -0.00449999981f } },
};

float	puke_spin[1 * 3] =		/* 0x80198468 */
{
	0.0f, 0.0f, 0.200000003f,
};

float	puke_offset[1 * 3] =		/* 0x80198474 */
{
	0.200000003f, 0.0f, 0.850000024f,
};

/* 0x80198480: the mouth-shape runs (0-3, -1 ends a run), one a frame while he speaks */
int	junior_mouth_runs[674] =
{
	-1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 1, 2, 2, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2,
	3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 3, 3,
	3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 2, 1, 1, 0, -1, -1,
	0, 1, 1, 2, 2, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1,
	1, 0, -1, -1, 0, 1, 1, 2, 2, 3, 3, 2, 3, 3, 3, 3,
	3, 2, 3, 2, 1, 2, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	2, 3, 3, 2, 3, 3, 2, 3, 3, 2, 2, 1, 1, 0, -1, 0,
	1, 1, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3,
	3, 3, 2, 2, 1, 1, 0, -1, 0, 1, 1, 2, 2, 3, 3, 3,
	2, 3, 3, 2, 1, 2, 3, 3, 3, 2, 3, 3, 3, 2, 3, 3,
	3, 2, 2, 1, 1, 0, -1, 0, 1, 1, 2, 2, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 2, 3, 3, 2, 2, 1, 1, 0, -1, 0,
	1, 1, 2, 2, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3,
	3, 3, 3, 2, 2, 1, 1, 0, -1, 0, 1, 1, 2, 2, 3, 3,
	3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1,
	1, 0, -1, 0, 1, 1, 2, 2, 3, 3, 3, 3, 3, 3, 3, 2,
	3, 3, 2, 2, 1, 1, 0, -1, 0, 1, 1, 2, 2, 3, 3, 3,
	3, 3, 2, 1, 0, -1, 0, 1, 1, 2, 2, 3, 3, 3, 3, 2,
	3, 3, 3, 3, 3, 3, 2, 1, 0, -1, 0, 1, 1, 2, 2, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 2, 2, 1, 1,
	0, -1, 0, 1, 1, 2, 2, 3, 3, 2, 3, 3, 3, 3, 3, 3,
	3, 3, 2, 1, 0, -1, 0, 1, 1, 2, 2, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 2, 2, 1, 1, 0, -1, 0, 1, 1, 2, 2, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2,
	1, 1, 0, -1, 0, 1, 1, 2, 2, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1,
	1, 0, -1, 0, 1, 1, 2, 2, 3, 3, 3, 3, 2, 3, 3, 3,
	3, 2, 3, 3, 3, 3, 1, 3, 2, 1, 2, 3, 3, 3, 3, 3,
	2, 3, 3, 3, 3, 3, 3, 2, 1, 2, 2, 3, 3, 2, 3, 2,
	1, 2, 3, 3, 3, 2, 3, 3, 3, 3, 2, 2, 1, 1, 0, -1,
	0, 1, 1, 2, 2, 3, 3, 3, 3, 3, 3, 2, 1, 2, 3, 3,
	3, 2, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 2, 2, 1, 1, 0, -1, 0, 1, 1, 2, 2, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1, 1,
	0, -1,
};

/* 0x80198f08: the run for each "baby" bank call (calls without a run point at a lone -1) */
int	*junior_mouth[30] =
{
	&junior_mouth_runs[0],	/* call 0 */
	&junior_mouth_runs[1],	/* call 1 */
	&junior_mouth_runs[2],	/* call 2 */
	&junior_mouth_runs[3],	/* call 3 */
	&junior_mouth_runs[4],	/* call 4 */
	&junior_mouth_runs[5],	/* call 5 */
	&junior_mouth_runs[6],	/* call 6 */
	&junior_mouth_runs[7],	/* call 7 */
	&junior_mouth_runs[8],	/* call 8 */
	&junior_mouth_runs[9],	/* call 9 */
	&junior_mouth_runs[63],	/* call 10 */
	&junior_mouth_runs[64],	/* call 11 */
	&junior_mouth_runs[115],	/* call 12 */
	&junior_mouth_runs[116],	/* call 13 */
	&junior_mouth_runs[191],	/* call 14 */
	&junior_mouth_runs[232],	/* call 15 */
	&junior_mouth_runs[263],	/* call 16 */
	&junior_mouth_runs[287],	/* call 17 */
	&junior_mouth_runs[313],	/* call 18 */
	&junior_mouth_runs[339],	/* call 19 */
	&junior_mouth_runs[360],	/* call 20 */
	&junior_mouth_runs[374],	/* call 21 */
	&junior_mouth_runs[394],	/* call 22 */
	&junior_mouth_runs[418],	/* call 23 */
	&junior_mouth_runs[438],	/* call 24 */
	&junior_mouth_runs[474],	/* call 25 */
	&junior_mouth_runs[500],	/* call 26 */
	&junior_mouth_runs[531],	/* call 27 */
	&junior_mouth_runs[592],	/* call 28 */
	&junior_mouth_runs[633],	/* call 29 */
};

/* 0x80198f8c: his voice lines, the "baby" calls junior_speak picks from */
int	junior_voices[14] = { 0xb, 0xe, 0xf, 0x10, 0x11, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b };
