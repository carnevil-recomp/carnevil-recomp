/*
 * sprites.c - the sprite effects (0x800db148-0x800dbf58): see sprites.h. A sprite is created
 * at a position with a kind; the kind table gives its textures and size, and the creator and
 * the per-frame update give each kind its behaviour:
 *
 *   screen 0-3   the pistol and shotgun muzzle flashes, halving each frame until 8 pixels
 *   screen 4-5   animated flashes (5 frames) shrinking by a quarter each frame
 *   screen 6-15  the hit marks, held four frames and faded 25 a frame
 *   world  0, 6, 9  one-shot animations spun at random (0 is dimmed when violence is off)
 *   world  1     a one-shot animation in a random constant colour
 *   world  2, 5, 10  translucent raised animations (the dust of a miss, a splash)
 *   world  3     a drop: flies with its velocity and fades 12 a frame
 *   world  4, 11 shown for a single frame
 *   world  7     fades 10 a frame; 8 spins 0.3 a frame
 *
 * The lists are singly linked through the record; the reaper walks them with a pointer to
 * the previous record, treating the list head as the previous record's link.
 */
#include <stddef.h>
#include <stdlib.h>
#include <glide/glide.h>
#include "sprites.h"

extern int	free_mem_left;				/* 0x801e0ee8 */
extern int	violence_level;				/* 0x801de8a0 */
extern int	randrng(int);
extern float	func_800fe8dc(float range);		/* a random float in +-range */

#define PREV_OF(head, type)	((type *)((char *)&(head) - offsetof(type, next)))

#ifdef PORT_HOST
/* the port does not load the shared level texmaps yet (load_misc_texmaps, 0x800d8c60, is a
   stub), so a sprite's surface can be missing: draw it untextured instead of crashing */
#define SPRITE_TEX(s)	((s)->surf[(s)->frame] ? (s)->surf[(s)->frame]->handle : 0)
#else
#define SPRITE_TEX(s)	((s)->surf[(s)->frame]->handle)
#endif

/* 0x8017d230: the muzzle flashes (0x5050-0x5054, 0x5031 x5) and the hit marks (0x6101-0x610a) */
sprite_kind_t	screen_sprite_kinds[16] =
{
	{ 0x5050, 1, 64.0f, 64.0f, 255.0f, 255.0f },
	{ 0x5051, 1, 64.0f, 64.0f, 255.0f, 255.0f },
	{ 0x5052, 1, 64.0f, 64.0f, 255.0f, 255.0f },
	{ 0x5053, 1, 64.0f, 64.0f, 255.0f, 255.0f },
	{ 0x5054, 1, 64.0f, 64.0f, 255.0f, 255.0f },
	{ 0x5031, 5, 64.0f, 64.0f, 255.0f, 255.0f },
	{ 0x6101, 1, 128.0f, 128.0f, 255.0f, 127.0f },
	{ 0x6103, 1, 128.0f, 128.0f, 255.0f, 255.0f },
	{ 0x6102, 1, 128.0f, 128.0f, 255.0f, 255.0f },
	{ 0x6105, 1, 128.0f, 128.0f, 255.0f, 255.0f },
	{ 0x6104, 1, 128.0f, 128.0f, 255.0f, 255.0f },
	{ 0x6106, 1, 64.0f, 64.0f, 255.0f, 255.0f },
	{ 0x6107, 1, 128.0f, 128.0f, 255.0f, 255.0f },
	{ 0x6108, 1, 75.0f, 150.0f, 127.0f, 255.0f },
	{ 0x6109, 1, 148.0f, 148.0f, 255.0f, 255.0f },
	{ 0x610a, 1, 128.0f, 128.0f, 255.0f, 127.0f },
};

/* 0x8017d3b0: sizes in world units */
sprite_kind_t	world_sprite_kinds[8] =
{
	{ 0x5021, 4, 4.0f, 4.0f, 255.0f, 255.0f },
	{ 0x5068, 4, 3.0f, 3.0f, 255.0f, 255.0f },
	{ 0x5009, 5, 2.5f, 5.0f, 128.0f, 255.0f },
	{ 0x600c, 1, 2.5f, 2.5f, 255.0f, 255.0f },
	{ 0x7000, 1, 3.5f, 3.5f, 255.0f, 255.0f },
	{ 0x5001, 5, 1.5f, 3.0f, 128.0f, 255.5f },
	{ 0x5041, 5, 4.0f, 4.0f, 255.0f, 255.0f },
	{ 0x6104, 1, 5.0f, 5.0f, 255.0f, 255.0f },
};

screen_sprite_t	*screen_sprite_list;		/* 0x801e5918 */
world_sprite_t	*world_sprite_list;		/* 0x801e5cb4 */

/* ---- screen sprites ---- */

/* 0x800db148: at level load */
void init_screen_sprites(void)
{
	screen_sprite_list = 0;
}

/* 0x800db158: a flash needs 16K of heap free, a hit mark more than 64K */
void add_screen_sprite(float x, float y, int kind, int param)
{
	screen_sprite_t	*s;
	sprite_kind_t	*k;
	int		i;

	if(kind < 6)
	{
		if(free_mem_left < 0x4000)
			return;
	}
	else if(free_mem_left <= 0xffff)
		return;
	s = malloc(sizeof(screen_sprite_t));
	if(s == 0)
		return;
	k = &screen_sprite_kinds[kind];
	s->x = x;
	s->y = y;
	s->kind = kind;
	s->param = param;
	s->frame = 0;
	s->loop_frame = 0;
	s->last_frame = k->frames - 1;
	s->w = k->w;
	s->h = k->h;
	s->u = k->u;
	s->flags = 2;
	s->count = 0;
	s->mirror = 0.0f;
	s->alpha = 255.0f;
	s->v = k->v;
	for(i = 0; i < k->frames; i++)
		s->surf[i] = find_surface(k->key + i);
	if(kind < 5)
	{
		s->count = 10;
		s->alpha = 255.0f;
		s->flags |= SPRITE_ALPHA;
	}
	else if(kind >= 6 && kind < 16)
	{
		s->count = 0;
		s->alpha = 255.0f;
		s->flags |= SPRITE_ALPHA;
	}
	s->next = screen_sprite_list;
	screen_sprite_list = s;
}

/* 0x800db320: as above with the mirror flag, and no heap check */
void add_screen_sprite_mirror(float x, float y, int kind, int param, float mirror)
{
	screen_sprite_t	*s;
	sprite_kind_t	*k;
	int		i;

	s = malloc(sizeof(screen_sprite_t));
	if(s == 0)
		return;
	k = &screen_sprite_kinds[kind];
	s->x = x;
	s->y = y;
	s->kind = kind;
	s->param = param;
	s->frame = 0;
	s->loop_frame = 0;
	s->last_frame = k->frames - 1;
	s->w = k->w;
	s->h = k->h;
	s->u = k->u;
	s->flags = 2;
	s->count = 0;
	s->mirror = mirror;
	s->alpha = 255.0f;
	s->v = k->v;
	for(i = 0; i < k->frames; i++)
		s->surf[i] = find_surface(k->key + i);
	if(kind < 5)
	{
		s->count = 10;
		s->alpha = 255.0f;
		s->flags |= SPRITE_ALPHA;
	}
	else if(kind >= 6 && kind < 16)
	{
		s->count = 0;
		s->alpha = 255.0f;
		s->flags |= SPRITE_ALPHA;
	}
	s->next = screen_sprite_list;
	screen_sprite_list = s;
}

/* 0x800db4b8: draw every screen sprite, step it, and reap the dead */
void draw_screen_sprites(void)
{
	screen_sprite_t	*s, *prev, *next;

	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(4);
	s = screen_sprite_list;
	prev = PREV_OF(screen_sprite_list, screen_sprite_t);
	while(s)
	{
		guTexSource(SPRITE_TEX(s));
		if(s->mirror == 0.0f)
		{
			if(s->flags & SPRITE_ALPHA)
			{
				guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA);
				draw_quad_alpha(s->x, s->y, s->w, s->h, s->u, s->v, s->alpha);
			}
			else
			{
				guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
				draw_quad(s->x, s->y, s->w, s->h, s->u, s->v);
			}
		}
		else
		{
			if(s->flags & SPRITE_ALPHA)
			{
				guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA);
				draw_quad_alpha_mirrored(s->x, s->y, s->w, s->h, s->u, s->v, s->alpha);
			}
			else
			{
				guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
				draw_quad_mirrored(s->x, s->y, s->w, s->h, s->u, s->v);
			}
		}
		next = s->next;
		switch(s->kind)
		{
		case 0:
		case 1:
		case 2:
		case 3:
			s->w *= 0.5f;
			s->h *= 0.5f;
			if(s->w <= 8.0f)
				s->flags |= SPRITE_DEAD;
			break;
		case 4:
		case 5:
			s->frame++;
			if(s->last_frame < s->frame)
				s->frame = s->loop_frame;
			s->w *= 0.75f;
			s->h *= 0.75f;
			if(s->w <= 8.0f)
				s->flags |= SPRITE_DEAD;
			break;
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			s->count++;
			if(s->count >= 4)
			{
				s->alpha -= 25.0f;
				if(s->alpha < 0.0f)
					s->flags |= SPRITE_DEAD;
			}
			break;
		}
		if(s->flags & SPRITE_DEAD)
		{
			free(s);
			prev->next = next;
		}
		else
			prev = s;
		s = next;
	}
	grAlphaTestReferenceValue(0);
}

/* 0x800db7a8: at level unload */
void free_screen_sprites(void)
{
	screen_sprite_t	*s, *next;

	s = screen_sprite_list;
	while(s)
	{
		next = s->next;
		free(s);
		s = next;
		PREV_OF(screen_sprite_list, screen_sprite_t)->next = s;
	}
}

/* ---- world sprites ---- */

/* 0x800db800 */
void init_world_sprites(void)
{
	world_sprite_list = 0;
}

/* 0x800db810: needs more than 32K of heap free */
void add_world_sprite(float *pos, int kind, int param)
{
	world_sprite_t	*s;
	sprite_kind_t	*k;
	int		i;

	if(free_mem_left <= 0x7fff)
		return;
	s = malloc(sizeof(world_sprite_t));
	if(s == 0)
		return;
	k = &world_sprite_kinds[kind];
	s->pos[0] = pos[0];
	s->pos[1] = pos[1];
	s->rot[0] = 0.0f;
	s->rot[1] = 0.0f;
	s->rot[2] = 0.0f;
	s->pos[2] = pos[2];
	s->w = k->w;
	s->kind = kind;
	s->param = param;
	s->frame = 0;
	s->loop_frame = 0;
	s->alpha = 255.0f;
	s->h = k->h;
	s->last_frame = k->frames - 1;
	s->u = k->u;
	s->flags = 2;
	s->count = 0;
	s->depth_bias = 0;
	s->unk86 = 0;
	s->color = 0xffffffff;
	s->v = k->v;
	for(i = 0; i < k->frames; i++)
		s->surf[i] = find_surface(k->key + i);
	switch(kind)
	{
	case 0:
		s->count = 0;
		s->depth_bias = (short)0x8000;
		if(violence_level == 0)
			s->alpha = 125.0f;
		else
			s->alpha = 255.0f;
		s->rot[2] = func_800fe8dc(6.2831855f);
		s->flags |= SPRITE_ALPHA;
		break;
	case 1:
		s->count = 0;
		s->depth_bias = (short)0x8000;
		s->alpha = 100.0f;
		s->color = ((randrng(255) & 0xff) << 16) | ((randrng(255) & 0xff) << 8) | 0xff000000 | (randrng(255) & 0xff);
		s->flags |= SPRITE_ALPHA;
		break;
	case 2:
		s->count = 0;
		s->depth_bias = (short)0x8000;
		s->alpha = 128.0f;
		s->flags |= SPRITE_ALPHA | SPRITE_RAISED;
		break;
	case 3:
		s->alpha = func_800fe8dc(40.0f) + 198.0f;
		s->flags |= SPRITE_ALPHA;
		s->vel[0] = func_800fe8dc(0.35f);
		s->vel[1] = func_800fe8dc(0.3f) + 0.5f;
		s->vel[2] = func_800fe8dc(0.35f);
		break;
	case 4:
	case 11:
		s->depth_bias = (short)0x8000;
		break;
	case 5:
		s->count = 0;
		s->depth_bias = (short)0x8000;
		s->alpha = 255.0f;
		s->flags |= SPRITE_ALPHA | SPRITE_RAISED;
		break;
	case 6:
	case 9:
		s->count = 0;
		s->depth_bias = (short)0x8000;
		s->alpha = 255.0f;
		s->rot[2] = func_800fe8dc(6.2831855f);
		s->flags |= SPRITE_ALPHA;
		break;
	case 7:
		s->count = 0;
		s->depth_bias = (short)0x8000;
		s->alpha = 255.0f;
		s->flags |= SPRITE_ALPHA;
		break;
	case 8:
		s->count = 0;
		s->depth_bias = 0;
		s->rot[2] = 0.0f;
		s->alpha = 255.0f;
		break;
	case 10:
		s->depth_bias = (short)0x8000;
		s->alpha = 200.0f;
		s->flags |= SPRITE_ALPHA | SPRITE_RAISED;
		break;
	}
	s->next = world_sprite_list;
	world_sprite_list = s;
}

/* 0x800dbb80: draw every world sprite (no depth writes), step it, and reap the dead */
void draw_world_sprites(void)
{
	world_sprite_t	*s, *prev, *next;

	guColorCombineFunction(GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(4);
	grDepthMask(FXFALSE);
	s = world_sprite_list;
	prev = PREV_OF(world_sprite_list, world_sprite_t);
	while(s)
	{
		guTexSource(SPRITE_TEX(s));
		grDepthBiasLevel(s->depth_bias);
		grConstantColorValue(s->color);
		if(s->flags & SPRITE_RAISED)
		{
			if(s->flags & SPRITE_ALPHA)
			{
				guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA);
				draw_billboard_raised_alpha(s->pos, s->rot, s->w, s->h, s->alpha, s->u, s->v);
			}
			else
			{
				guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
				draw_billboard_raised(s->pos, s->rot, s->w, s->h, s->u, s->v);
			}
		}
		else
		{
			if(s->flags & SPRITE_ALPHA)
			{
				guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA);
				draw_billboard_alpha(s->pos, s->rot, s->w, s->h, s->alpha, s->u, s->v);
			}
			else
			{
				guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
				draw_billboard(s->pos, s->rot, s->w, s->h, s->u, s->v);
			}
		}
		next = s->next;
		switch(s->kind)
		{
		case 0:
		case 1:
		case 2:
		case 6:
		case 9:
		case 10:
		case 11:
			s->frame++;
			if(s->last_frame < s->frame)
			{
				s->frame = s->loop_frame;
				s->flags |= SPRITE_DEAD;
			}
			break;
		case 3:
			s->pos[0] += s->vel[0];
			s->pos[1] += s->vel[1];
			s->alpha -= 12.0f;
			s->pos[2] += s->vel[2];
			if(s->alpha <= 0.0f)
				s->flags |= SPRITE_DEAD;
			break;
		case 4:
			s->flags |= SPRITE_DEAD;
			break;
		case 5:
			s->pos[0] += func_800fe8dc(0.1f);
			s->pos[2] += func_800fe8dc(0.1f);
			s->count++;
			if(s->count != 0)
			{
				s->count = 0;
				s->frame++;
				if(s->last_frame < s->frame)
				{
					s->pos[1] += 6.0f;
					s->frame = s->loop_frame;
				}
			}
			s->alpha -= 19.0f;
			if(s->alpha <= 0.0f)
				s->flags |= SPRITE_DEAD;
			break;
		case 7:
			s->alpha -= 10.0f;
			if(s->alpha <= 0.0f)
				s->flags |= SPRITE_DEAD;
			break;
		case 8:
			s->rot[2] += 0.3f;
			break;
		}
		if(s->flags & SPRITE_DEAD)
		{
			free(s);
			prev->next = next;
		}
		else
			prev = s;
		s = next;
	}
	grAlphaTestReferenceValue(0);
	grDepthBiasLevel(0);
	grDepthMask(FXTRUE);
}

/* 0x800dbf04: at level unload */
void free_world_sprites(void)
{
	world_sprite_t	*s, *next;

	s = world_sprite_list;
	while(s)
	{
		next = s->next;
		free(s);
		s = next;
		PREV_OF(world_sprite_list, world_sprite_t)->next = s;
	}
}
