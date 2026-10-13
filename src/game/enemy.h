/*
 * enemy.h - the enemy record (0xec bytes, allocated by the enemy objects' init functions):
 * the fields known so far, from the draw pass's debug overlay, the nearest-enemy searches
 * and the level sound helpers. Game objects whose data block is an enemy point here.
 */
#ifndef ENEMY_H
#define ENEMY_H

typedef struct enemy
{
	short		id;			/* 0x00: the game object id, written by add_object() */
	unsigned char	pad0[6];
	short		frame;			/* 0x08: animation frame */
	unsigned char	pad1[0xa];
	unsigned short	ai_state;		/* 0x14: 0x6000 = dying/dead */
	unsigned char	pad2[2];
	short		num;			/* 0x18 */
	unsigned char	pad3[0xa];
	float		x, y, z;		/* 0x24 */
	float		rx, ry, rz;		/* 0x30: rotation */
	unsigned char	pad4[0x38];
	int		sound_priority;		/* 0x74 */
	unsigned char	pad5[0x10];
	int		sound_volume;		/* 0x88 */
	unsigned char	pad6[8];
	short		path;			/* 0x94 */
	unsigned char	pad7[0x3e];
	unsigned short	prio_base;		/* 0xd4: priority of its sound n is prio_base - n */
	unsigned short	prio_flags;		/* 0xd6 */
	unsigned char	pad8[0x14];
} enemy_t;					/* sizeof = 0xec */

extern float	camera_pos[3];			/* 0x801de8f0: the view point the enemies are measured from. camera.c
						   defines it as the scalars camera_x/y/z (its code needs them so) and the
						   link aliases the two names; see docs/toolchain.md */

#endif
