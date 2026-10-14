/*
 * enemy.h - the enemy record (0xec bytes, allocated by the enemy objects' init functions).
 * Game objects whose data block is an enemy point here. The fields come from the draw pass's
 * debug overlay, the nearest-enemy searches, the level sound helpers, the spawn constructor,
 * the model renderer and Krinkle's own code.
 */
#ifndef ENEMY_H
#define ENEMY_H

typedef struct enemy
{
	short		id;			/* 0x00: the game object id, written by add_object() */
	short		unk2, unk4;
	short		unk6;			/* 0x06: from the spawn entry */
	short		frame;			/* 0x08: animation frame */
	unsigned short	flags2;			/* 0x0a: the state bits 0x100/0x80/0x40/0x20 stripped by the set-up */
	short		frame_from;		/* 0x0c: a blend runs from this frame ... */
	short		frame_to;		/* 0x0e: ... to this one */
	short		blend_len;		/* 0x10: over this many draws */
	short		blend_pos;		/* 0x12 */
	unsigned short	ai_state;		/* 0x14: 0x6000 = dying/dead; starts as the spawn entry says */
	unsigned short	flags;			/* 0x16: 0x8000 set by the spawn, 2 = blending, 1 = drawn once */
	short		num;			/* 0x18: index in the spawn table */
	short		unk1a;
	void		*effects;		/* 0x1c: attached effects (run after the model is drawn) */
	int		unk20;
	float		x, y, z;		/* 0x24 */
	float		rx, ry, rz;		/* 0x30: rotation */
	float		health;			/* 0x3c */
	float		alpha;			/* 0x40: 0..255, for the alpha-tested draw mode */
	int		unk44;
	float		scale[3];		/* 0x48 */
	int		unk54;
	struct player_stats *target;	/* 0x58: the player it goes for */
	struct group	*groups;		/* 0x5c: one per submesh (model.h) */
	float		vel[3];			/* 0x60: added to the position each draw */
	short		resume_frame;		/* 0x6c: a hit reaction blends back to this frame ... */
	short		resume_state;		/* 0x6e: ... and this state */
	float		goal[3];		/* 0x70: where the steering walks it to */
	float		radius;			/* 0x7c: unkcc times the y scale */
	int		unk80;
	struct group	*hit_group;		/* 0x84 */
	struct player_stats *killer;	/* 0x88: the player whose shot killed it */
	int		unk8c;
	short		unk90, unk92;		/* 0x90: the spawn entry's path and unk2a */
	short		path;			/* 0x94 */
	short		unk96;
	float		unk98;			/* 0x98: from the spawn entry */
	int		unk9c, unka0, unka4, unka8, unkac, unkb0, unkb4, unkb8, unkbc, unkc0, unkc4, unkc8;
	float		unkcc;			/* 0xcc: 0.92 for Krinkle */
	int		unkd0;
	unsigned short	prio_base;		/* 0xd4: priority of its sound n is prio_base - n */
	unsigned short	prio_flags;		/* 0xd6 */
	int		unkd8, unkdc, unke0, unke4, unke8;
} enemy_t;					/* sizeof = 0xec */

extern float	camera_pos[3];			/* 0x801de8f0: the view point the enemies are measured from. camera.c
						   defines it as the scalars camera_x/y/z (its code needs them so) and the
						   link aliases the two names; see docs/toolchain.md */

#endif
