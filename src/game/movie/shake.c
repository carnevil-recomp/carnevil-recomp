/*
 * shake.c - the movie's vertical offset: screen shake and the one-shot scroll, and the
 * explosion sprite object that shares their file (0x800f3580-0x800f38c4; the file starts
 * at 0x800f3420 with code not yet decompiled).
 */
#include <stdlib.h>
#include <glide/glide.h>
#include "objects.h"
#include "movie.h"

extern void	draw_point_sprite_color(int, float *, float, float, unsigned int);	/* draw2d.c */

struct shake { int frames; float amp; };

/* screen shake / scroll (0x800f3580) */
int		shake_count;			/* 0x801e0e6c */
float		shake_amp;			/* 0x801e09dc */
float		shake_decay;			/* 0x80212dc4 */
/* 0x8018f118: the shakes: frames and the starting amplitude in lines */
struct shake	shake_table[3] = { { 4, 2.0f }, { 8, 8.0f }, { 12, 8.0f } };
int		scroll_request;			/* 0x8018f130 */
int		scroll_value = 4;		/* 0x8018f134 */
int		movie_scroll_center = 128;	/* 0x8017dfd8 */


/* 0x800f3580: vertical offset of the movie for the screen shake and one-shot scroll */
void update_movie_scroll(void)
{
	int	a;

	if(shake_count)
	{
		a = (int)shake_amp;
		movie.scroll = a;
		if(shake_count & 1)
		{
			movie.scroll = -a;
		}
		shake_amp = shake_amp - shake_decay;
		shake_count--;
	}
	else if(scroll_request)
	{
		movie.scroll = scroll_value;
		scroll_request = 0;
		scroll_value = -scroll_value;
	}
	else
	{
		movie.scroll = 0;
	}
	movie_scroll_center = (int)((float)movie.scroll + 128.0f);
}


/* 0x800f3668: start shake n: the amplitude dies away to nothing over its frames */
void start_shake(int n)
{
	shake_count = shake_table[n].frames;
	shake_amp = shake_table[n].amp;
	shake_decay = shake_amp / (float)shake_count;
}

/* 0x800f3654: scroll the movie once, next frame */
void request_scroll(void)
{
	scroll_request = 1;
}

/* the explosion (object 0x1001): the eight EXPLOSION frames (keys 0x5011..) as a point
   sprite two units across, one frame each, drawn without touching the depth buffer. The
   record is the size of an enemy's */
typedef struct explosion
{
	short		id, unk2, unk4, unk6;	/* 0x00 */
	short		frame;			/* 0x08 */
	short		unk0a, unk0c, unk0e, unk10, unk12;
	unsigned short	state, flags;		/* 0x14 */
	int		unk18, unk1c;
	unsigned int	colour;			/* 0x20: set but not drawn with */
	float		x, y, z;		/* 0x24 */
	int		unk30, unk34, unk38, unk3c;
	float		alpha;			/* 0x40: 196 */
	float		unk44;			/* 0x44: -12, unused */
	float		size;			/* 0x48: 2 */
	int		rest[(0xec - 0x4c) / 4];
} explosion_t;

static void	explosion_draw(game_object_t *), explosion_think(game_object_t *), explosion_remove(game_object_t *);
static int	explosion_init(game_object_t *);
static void	place_explosion(short, float *, unsigned int);

/* 0x800f36b0 */
void add_explosion(float *pos)
{
	short	id;

	id = add_object1_wrapper(0x1001, explosion_draw, explosion_init, explosion_think);
	if(id > 0)
	{
		place_explosion(id, pos, 0xffffffff);
	}
}

/* 0x800f370c */
static void explosion_draw(game_object_t *o)
{
	explosion_t	*e;

	e = (explosion_t *)o->data;
	grDepthMask(0);
	draw_point_sprite_color(e->frame + 0x5011, &e->x, e->size, e->alpha, 0xffffff);
	grDepthMask(1);
}

/* 0x800f3764 */
static void explosion_think(game_object_t *o)
{
	explosion_t	*e;

	e = (explosion_t *)o->data;
	e->frame++;
	if(e->frame < 8)
	{
		return;
	}
	remove_game_object(o->id);
}

/* 0x800f37ac */
static int explosion_init(game_object_t *o)
{
	explosion_t	*e;

	o->data = malloc(sizeof(explosion_t));
	if(o->data == 0)
	{
		return 1;
	}
	o->remove = explosion_remove;
	e = (explosion_t *)o->data;
	e->frame = 0;
	e->alpha = 196.0f;
	e->unk44 = -12.0f;
	e->state = 0;
	e->flags = 0;
	e->size = 2.0f;
	e->x = 0.0f;
	e->y = 0.0f;
	e->z = 0.0f;
	e->colour = 0xffffff;
	return 0;
}

/* 0x800f3844 */
static void explosion_remove(game_object_t *o)
{
	free(o->data);
}

/* 0x800f3868 */
static void place_explosion(short id, float *pos, unsigned int colour)
{
	explosion_t	*e;

	e = get_object_data(id);
	e->frame = 0;
	e->x = pos[0];
	e->y = pos[1];
	e->colour = colour;
	e->z = pos[2];
}
