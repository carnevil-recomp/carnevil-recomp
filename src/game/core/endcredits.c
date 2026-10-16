/*
 * endcredits.c - the credits that roll after a won game (0x80140270-0x801412a4).
 *
 * CREDITS.RAW and CREDITSB.RAW in the partition are two 400-pixel-wide pictures stored as
 * 24-bit RGB rows. load_credits streams both, one row at a time, into 256-row rings
 * (the movie player's two screen buffers, msb1 and msb2, reused as RGB565 rows) and puts
 * 256 rows on screen each frame through the LFB: rows of the first picture, except the
 * top and bottom 24, which are rows of the second picture shaded and rippled by a sine
 * (credits_shade_row) into a work buffer. The roll starts at one row a frame; holding
 * either gun's trigger (bit 4) speeds it up to ten, letting go slows it back down. It
 * ends at row 6050, fading out 6039 frames in.
 */
#include <stdio.h>
#include <stdlib.h>
#include <glide/glide.h>
#include "movie.h"

#ifdef PORT_HOST
#define INLINE
#else
#define INLINE inline
#endif

#define CREDITS_WIDTH	400			/* pixels in a row */
#define CREDITS_RING	256			/* rows held of each picture */
#define CREDITS_EDGE	24			/* shaded rows at the top and at the bottom */
#define CREDITS_PRIME	280			/* rows read before the roll starts */
#define CREDITS_END	6050			/* the row the roll stops at */
#define CREDITS_FADE	6039			/* frames into the roll the fade-out starts */

extern void	install_disk_info(void *, void *, void *);
extern void	disable_movie_draw(void), enable_movie_draw(void);
extern void	fade(float, int, void (*)(void));
extern void	sleep(int);
extern int	either_gun_bit4(void);
extern float	sinf(float), floorf(float);
extern movie_buf_t	msb1, msb2;

FILE		*credits_file;			/* 0x801e0eb0: CREDITS.RAW */
FILE		*creditsb_file;			/* 0x802374b4: CREDITSB.RAW */
unsigned short	*credits_edge_rows;		/* 0x8021f0ec: the shaded rows, a third 256-row ring */
unsigned short	*credits_rows;			/* 0x801e5c08: CREDITS.RAW's ring (msb1's buffer) */
unsigned short	*creditsb_rows;			/* 0x801de880: CREDITSB.RAW's ring (msb2's buffer) */
int		credits_row;			/* 0x801e0ec8: rows read so far */
float		credits_target;			/* 0x801e5bd4: read up to this row */
float		credits_speed;			/* 0x801e09f0: rows a frame, 1 to 10 */

#ifndef PORT_HOST
/* the DMA descriptor list is built through the global pointer register, as in movie.c */
register unsigned int	*dma asm("gp");
#endif

/* 0x80140270: a row of 24-bit RGB as RGB565 */
typedef struct { unsigned char r, g, b; } rgb_t;		/* a pixel of the .RAW pictures */

INLINE void credits_rgb_to_565(rgb_t *rgb, unsigned short *out)
{
	rgb_t		*p;
	unsigned short	*q;
	int		i;

	for(i = 0, q = out, p = rgb; i < CREDITS_WIDTH; i++)
	{
		*q++ = ((p->r >> 3) << 11) | ((p->g >> 2) << 5) | (p->b >> 3);
		p++;
	}
}

/* 0x801402c0: read the next row of a picture into its ring slot */
INLINE void credits_read_row(int row, unsigned short *ring, FILE *f)
{
	rgb_t		rgb[CREDITS_WIDTH];
	unsigned short	*out;
	int		slot;

	slot = row % CREDITS_RING;
	out = ring + slot * CREDITS_WIDTH;
	if(fread(rgb, 3, CREDITS_WIDTH, f) != CREDITS_WIDTH)
	{
		printf("bad credits data read on line %d\n", row);
	}
	credits_rgb_to_565(rgb, out);
}

/* 0x8014039c: a row shaded by a sine of t (0..1: dark at the ends of the band, full at
   its middle) and shifted sideways by up to 10 pixels, with linear filtering */
void credits_shade_row(unsigned short *src, unsigned short *dst, float t, float phase)
{
	double		a;
	float		shift, bright, pos, frac, back;
	float		r, g, b, r2, g2, b2;
	float		ro, go, bo;
	unsigned short	c;
	int		i, x;

	a = (1.0 - t) * 3.14159f;
	bright = sinf(a) * 0.65 + 0.35;
	a = a * 8.0;
	bright = bright + sinf(a) * 0.06;
	shift = sinf(t * 3.14159f) * -10.0f;
	for(i = 0; i < CREDITS_WIDTH; i++)
	{
		pos = (double)(i + shift) + 0.5;
		x = pos;
		c = (unsigned)x < CREDITS_WIDTH ? src[x] : 0;
		r = (double)((c >> 11) << 3) / 255.0;
		g = (double)(((c >> 5) & 0x3f) << 2) / 255.0;
		b = (double)((c & 0x1f) << 3) / 255.0;
		if(x < 0 || x + 1 > CREDITS_WIDTH)
		{
			ro = r;
			go = g;
			bo = b;
		}
		else
		{
			c = src[x + 1];
			r2 = (double)((c >> 11) << 3) / 255.0;
			g2 = (double)(((c >> 5) & 0x3f) << 2) / 255.0;
			b2 = (double)((c & 0x1f) << 3) / 255.0;
			frac = pos - floorf(pos);
			back = 1.0 - frac;
			ro = back * r + frac * r2;
			go = back * g + frac * g2;
			bo = back * b + frac * b2;
		}
		ro *= bright;
		go *= bright;
		bo *= bright;
		if(ro > 1.0)
		{
			ro = 1.0f;
		}
		else if(ro < 0.0)
		{
			ro = 0;
		}
		if(go > 1.0)
		{
			go = 1.0f;
		}
		else if(go < 0.0)
		{
			bo = 0;		/* sic: the original clears blue here (0x8014069c) */
		}
		if(bo > 1.0)
		{
			bo = 1.0f;
		}
		else if(bo < 0.0)
		{
			bo = 0;
		}
		c = ((unsigned)(ro * 255.0f) >> 3) << 11;
		c |= (unsigned char)((unsigned)(go * 255.0f) >> 2) << 5;
		c |= (unsigned char)((unsigned)(bo * 255.0f) >> 3);
		*dst++ = c;
	}
}

#ifdef PORT_HOST
extern void	port_lfb_write565(const unsigned short *rows, int w, int h);
#endif

/* 0x801407f8: put 256 rows on screen through the LFB (RGB565, top row first): the rows
   starting at row of the first picture, the top and bottom 24 replaced by shaded rows of
   the second (rows row + 23 down to row, and row + 231 down to row + 208) */
void credits_draw_rows(int row, unsigned short *rows, unsigned short *brows, unsigned short *edge)
{
	unsigned short	*src;
	unsigned short	*out;
	unsigned int	lfb;
	int		i, off, phase;
#ifdef PORT_HOST
	static unsigned short	frame[CREDITS_RING * CREDITS_WIDTH];
#endif

#ifndef PORT_HOST
	grLfbBegin();
	grLfbBypassMode(GR_LFBBYPASS_ENABLE);
	grLfbWriteMode(GR_LFBWRITEMODE_565);
	grLfbOrigin(GR_ORIGIN_UPPER_LEFT);
	lfb = (unsigned int)grLfbGetWritePtr(GR_BUFFER_BACKBUFFER);
#else
	lfb = 0;			/* port_lfb_write565 stands in for the LFB and its DMA */
#endif
	phase = row / CREDITS_END;
	for(i = 0; i < CREDITS_RING; i++)
	{
		off = ((row + i) % CREDITS_RING) * CREDITS_WIDTH;
		src = rows + off;
		if(i < CREDITS_EDGE)
		{
			credits_shade_row(brows + ((row + CREDITS_EDGE - 1 - i) % CREDITS_RING) * CREDITS_WIDTH, out = edge + off, 1.0 - i / 23.0f, phase);
			src = out;
		}
		else if(i >= CREDITS_RING - CREDITS_EDGE)
		{
			credits_shade_row(brows + ((row + CREDITS_RING - CREDITS_EDGE - 1 - i) % CREDITS_RING) * CREDITS_WIDTH, out = edge + off, (i - (CREDITS_RING - CREDITS_EDGE)) / 23.0f, phase);
			src = out;
		}
#ifdef PORT_HOST
		{
			int	k;
			for(k = 0; k < CREDITS_WIDTH; k++)
			{
				frame[i * CREDITS_WIDTH + k] = src[k];
			}
		}
#else
		dma[0] = CREDITS_WIDTH * 2;
		dma[1] = (unsigned int)src & 0x1fffffff;
		dma[2] = (lfb + i * 2048) & 0x1fffffff;
		dma[3] = (unsigned int)(dma + 4);
		dma += 4;
#endif
	}
#ifdef PORT_HOST
	(void)lfb;
	port_lfb_write565(frame, CREDITS_WIDTH, CREDITS_RING);
#else
	grLfbEnd();
#endif
}

/* 0x801409fc: rewind both pictures and read their first 280 rows; the roll starts at one
   row a frame */
void credits_prime(void)
{
	fseek(credits_file, 0, SEEK_SET);
	fseek(creditsb_file, 0, SEEK_SET);
	for(credits_row = 0; credits_row < CREDITS_PRIME; credits_row++)
	{
		credits_read_row(credits_row, credits_rows, credits_file);
		credits_read_row(credits_row, creditsb_rows, creditsb_file);
	}
	credits_target = credits_row;
	credits_speed = 1.0f;
}

/* 0x80140c18: open both pictures, borrow the movie's screen buffers for the rings and
   allocate the shaded rows; 0 (with a message) when something is missing */
INLINE int credits_open(void)
{
	credits_file = fopen("credits.raw", "rb");
	creditsb_file = fopen("creditsb.raw", "rb");
	if(!credits_file || !creditsb_file)
	{
		printf("files not available for credits!\n");
		return(0);
	}
	credits_rows = (unsigned short *)msb1.buffer;
	creditsb_rows = (unsigned short *)msb2.buffer;
	credits_edge_rows = malloc(CREDITS_RING * CREDITS_WIDTH * 2);
	if(!credits_rows || !creditsb_rows || !credits_edge_rows)
	{
		printf("buffers not available for credits!\n");
		return(0);
	}
	credits_prime();
	return(1);
}

/* 0x80140d18: one frame of the roll: read the rows up to the target, draw, then the trigger
   speeds the roll up (by 0.1 a frame, to 10) or lets it slow down (to 1); 0 at the end */
int credits_step(void)
{
	while(credits_row < credits_target)
	{
		credits_read_row(credits_row, credits_rows, credits_file);
		credits_read_row(credits_row, creditsb_rows, creditsb_file);
		credits_row++;
	}
	credits_draw_rows(credits_row - CREDITS_RING, credits_rows, creditsb_rows, credits_edge_rows);
	if(credits_row >= CREDITS_END)
	{
		return(0);
	}
	if(either_gun_bit4())
	{
		credits_speed += 0.1;
	}
	else
	{
		credits_speed -= 0.1;
	}
	if(credits_speed > 10.0)
	{
		credits_speed = 10.0f;
	}
	else if(credits_speed < 1.0)
	{
		credits_speed = 1.0f;
	}
	credits_target += floorf(credits_speed);
	return(1);
}

/* 0x80141020: close both pictures and free the shaded rows */
INLINE void credits_close(void)
{
	if(credits_file)
	{
		fclose(credits_file);
		credits_file = 0;
	}
	if(creditsb_file)
	{
		fclose(creditsb_file);
		creditsb_file = 0;
	}
	credits_rows = 0;
	creditsb_rows = 0;
	if(credits_edge_rows)
	{
		free(credits_edge_rows);
		credits_edge_rows = 0;
	}
}

/* 0x801410a4: after a won game: the screen fades up on the roll, which runs until its last
   row (the fade-out starts 6039 frames in), then the pictures are closed and the movie's
   drawing resumes */
void load_credits(void)
{
	int	frames;

	install_disk_info(0, 0, 0);
	disable_movie_draw();
	frames = 0;
	fade(1.0f, 10, 0);
	if(credits_open())
	{
		do
		{
			sleep(1);
			if(++frames == CREDITS_FADE)
			{
				fade(0.0f, 10, 0);
			}
		} while(credits_step());
		credits_prime();
	}
	credits_close();
	enable_movie_draw();
	sleep(1);
}
