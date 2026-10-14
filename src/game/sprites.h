/*
 * sprites.h - the sprite effects (src/game/sprites.c, 0x800db148-0x800dbf58): short-lived
 * textured quads on two lists. A screen sprite sits at a screen position (the muzzle flashes,
 * the hit marks); a world sprite sits at a world position and faces the camera (the dust a
 * missed shot kicks up, blood drops, the bugs' hit sparks). Both are drawn, updated and
 * reaped once per frame from the draw pass.
 */
#ifndef SPRITES_H
#define SPRITES_H

#include "texmap.h"

#define SPRITE_FRAMES	16

#define SPRITE_ALPHA	1		/* flags: alpha blended (the texture alpha times the sprite's) */
#define SPRITE_RAISED	4		/* world sprites: the quad stands on its position instead of being centred on it */
#define SPRITE_DEAD	0x10000		/* reaped after this frame */

/* a sprite kind: the texture key of its first frame (the frames use consecutive keys) and
   the quad's size and texture extent (24 bytes) */
typedef struct sprite_kind
{
	int	key;
	int	frames;
	float	w, h;
	float	u, v;
} sprite_kind_t;

/* a screen sprite (0x84 bytes) */
typedef struct screen_sprite
{
	float		x, y;			/* 0x00 */
	int		unk08;
	int		kind;			/* 0x0c */
	int		param;			/* 0x10: unused by the sprites themselves */
	surface_t	*surf[SPRITE_FRAMES];	/* 0x14: one per frame */
	float		w, h;			/* 0x54 */
	float		alpha;			/* 0x5c */
	float		u, v;			/* 0x60 */
	int		frame;			/* 0x68 */
	int		loop_frame;		/* 0x6c: where the animation restarts */
	int		last_frame;		/* 0x70 */
	float		mirror;			/* 0x74: non-zero draws the texture mirrored */
	int		count;			/* 0x78 */
	int		flags;			/* 0x7c */
	struct screen_sprite *next;	/* 0x80 */
} screen_sprite_t;

/* a world sprite (0xa0 bytes) */
typedef struct world_sprite
{
	float		pos[3];			/* 0x00 */
	float		rot[3];			/* 0x0c: rot[2] spins the quad in the screen plane */
	float		w, h;			/* 0x18 */
	int		kind;			/* 0x20 */
	int		param;			/* 0x24 */
	surface_t	*surf[SPRITE_FRAMES];	/* 0x28 */
	float		alpha;			/* 0x68 */
	int		frame;			/* 0x6c */
	int		loop_frame;		/* 0x70 */
	int		last_frame;		/* 0x74 */
	float		u, v;			/* 0x78 */
	unsigned int	color;			/* 0x80: the constant colour the texture is scaled by */
	short		depth_bias;		/* 0x84 */
	short		unk86;
	int		count;			/* 0x88 */
	int		flags;			/* 0x8c */
	float		vel[3];			/* 0x90: a drop's velocity (kind 3) */
	struct world_sprite *next;		/* 0x9c */
} world_sprite_t;

extern sprite_kind_t	screen_sprite_kinds[16];	/* 0x8017d230 */
extern sprite_kind_t	world_sprite_kinds[8];		/* 0x8017d3b0 */
extern screen_sprite_t	*screen_sprite_list;		/* 0x801e5918 */
extern world_sprite_t	*world_sprite_list;		/* 0x801e5cb4 */

void	init_screen_sprites(void);
void	add_screen_sprite(float x, float y, int kind, int param);
void	add_screen_sprite_mirror(float x, float y, int kind, int param, float mirror);
void	draw_screen_sprites(void);
void	free_screen_sprites(void);
void	init_world_sprites(void);
void	add_world_sprite(float *pos, int kind, int param);
void	draw_world_sprites(void);
void	free_world_sprites(void);

/* draw2d.c: the quads the sprites are drawn with */
void	draw_quad(float x, float y, float w, float h, float u, float v);
void	draw_quad_mirrored(float x, float y, float w, float h, float u, float v);
void	draw_quad_alpha(float x, float y, float w, float h, float u, float v, float alpha);
void	draw_quad_alpha_mirrored(float x, float y, float w, float h, float u, float v, float alpha);
void	draw_billboard(float *pos, float *rot, float w, float h, float u, float v);
void	draw_billboard_alpha(float *pos, float *rot, float w, float h, float alpha, float u, float v);
void	draw_billboard_raised(float *pos, float *rot, float w, float h, float u, float v);
void	draw_billboard_raised_alpha(float *pos, float *rot, float w, float h, float alpha, float u, float v);

#endif
