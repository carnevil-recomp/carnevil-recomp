/*
 * hiscore.c - the high score table and its entry screens (the file runs 0x800e4c70-0x800e7184;
 * built -O0 with `register`). The table lives in CMOS adjustments 0x3e-0x47 (scores) and
 * 0x48-0x51 (packed initials). With the HIGH SCORE AUTOGRAPH adjustment (0x36) on, a player
 * who makes the table signs it with the gun on a 256x128 board and the signature becomes that
 * place's picture (HSTD01-10.WMS); with it off the place is entered as initials.
 *
 * Still assembly in this file: create_hstd_tex 0x800e6358, the initials entry 0x800e6670,
 * make_highscore_screen 0x800e69b8, 0x800e70b0, 0x800e7130, 0x800e7184.
 */
#include <stdio.h>
#include <stdlib.h>
#include <glide/glide.h>
#include "objects.h"
#include "credits.h"
#include "texmap.h"

extern int	remove(const char *);

extern int	get_adjustment(int, int *), set_adjustment(int, int);
extern void	graphical_printf(char *, char *, ...);
extern void	sleep(int);
extern void	fade(float, int, void (*)(void));
extern void	black_screen(void), normal_screen(void);
extern void	func_800e7568(void);				/* attract.c: stop the movie */
extern void	draw_string_centered(int font, char *str, float x, float y, unsigned int color);
extern void	draw_quad(float x, float y, float w, float h, float u, float v);
extern int	increment_audit(int);
extern void	set_frame_factor(int);
extern void	stream_stop(void);
extern void	set_bgnd_color(unsigned int);
extern void	specific_gamma_load(float, float);
extern void	create_hstd_tex(unsigned short *, int);
extern void	func_800e6670(int);				/* the initials entry */

extern char	text_buffer[];
extern int	high_score_autograph;			/* adjust.c: adjustment 0x36 */
extern player_stats_t *player_stats_g;			/* shoot.c: the player signing */

void	draw_high_scores(game_object_t *);
void	write_scores(void);
void	file_copy(char *, char *);
void	autograph_entry(int);
void	draw_autograph_board(game_object_t *);

typedef struct high_score
{
	char		name[4];
	unsigned int	score;
} high_score_t;

high_score_t	high_scores[10];			/* 0x8022b198 */
unsigned int	score;					/* 0x801e0988: the score being entered */
int		high_score_scroll;			/* 0x801e5dbc: the attract table's offset */
int		autograph_timer;			/* 0x801e09d0: frames left to sign */
int		autograph_done;				/* 0x801e09e0 */
unsigned short	*autograph_ink;				/* 0x801f8834: ink per pixel, 256 x 128 */
unsigned int	*autograph_pixels;			/* 0x801facc0: the board as LFB words */

/* the board's pixel x, y (rows of 256) in the ink and the frame-buffer words */
#define INK(x, y)	(*(autograph_ink + (y) * 256 + (x)))
#define PIXEL(x, y)	(*(autograph_pixels + (y) * 256 + (x)))
char		autograph_player[12];			/* 0x801facd0: "PLAYER 1" or "PLAYER 2" */

static high_score_t default_scores[10] =	/* 0x8017de08 */
{
	{ "JCK", 300000 }, { "SAM", 270000 }, { "DIO", 240000 }, { "SLC", 210000 }, { "MRF", 180000 },
	{ "MRT", 150000 }, { "ROW", 120000 }, { "JAS", 90000 }, { "KEV", 60000 }, { "MJL", 30000 },
};

/* the ten places' pictures (0x8017de58) and the signing board, autograph or initials */
texmap_t	hstd_texmap[10] =
{
	{ 0x7100, 1, 0xffffff, 0, "HSTD01.WMS" }, { 0x7101, 1, 0xffffff, 0, "HSTD02.WMS" },
	{ 0x7102, 1, 0xffffff, 0, "HSTD03.WMS" }, { 0x7103, 1, 0xffffff, 0, "HSTD04.WMS" },
	{ 0x7104, 1, 0xffffff, 0, "HSTD05.WMS" }, { 0x7105, 1, 0xffffff, 0, "HSTD06.WMS" },
	{ 0x7106, 1, 0xffffff, 0, "HSTD07.WMS" }, { 0x7107, 1, 0xffffff, 0, "HSTD08.WMS" },
	{ 0x7108, 1, 0xffffff, 0, "HSTD09.WMS" }, { 0x7109, 1, 0xffffff, 0, "HSTD10.WMS" },
};
texmap_t	ehstd_texmap[1] = { { 0x7110, 1, 0xffffff, 0, "HIGHDRAM.WMS" } };	/* 0x8017df98 */
texmap_t	ehstd2_texmap[1] = { { 0x7110, 1, 0xffffff, 0, "OUIGA.WMS" } };	/* 0x8017dfb8 */

/* 0x800e4c70 */
void load_hstd_texmap(void)
{
	load_texmap(hstd_texmap, 10, "hstd");
}

/* 0x800e4ca4 */
void remove_hstd_texmap(void)
{
	remove_surface(hstd_texmap, 10);
}

/* 0x800e4cd0: the entry screen's board */
void load_ehstd_texmap(void)
{
	if(high_score_autograph)
	{
		load_texmap(ehstd_texmap, 1, "ehstd");
	}
	else
	{
		load_texmap(ehstd2_texmap, 1, "ehstd");
	}
}

/* 0x800e4d38 */
void remove_ehstd_texmap(void)
{
	if(high_score_autograph)
	{
		remove_surface(ehstd_texmap, 1);
	}
	else
	{
		remove_surface(ehstd2_texmap, 1);
	}
}

/* 0x800e4d90: attract step 5: the table scrolls up from below the screen, two lines a frame,
   holds for a second and fades out */
int attract_high_scores(int arg)
{
	short	obj;

	func_800e7568();
	high_score_scroll = 490;
	black_screen();
	sleep(2);
	fade(1.0f, 5, 0);
	obj = add_object1_wrapper(0x20, draw_high_scores, 0, 0);
	high_score_scroll = 490;
	while(!(high_score_scroll < -63))
	{
		sleep(1);
		high_score_scroll -= 2;
	}
	sleep(60);
	fade(0.0f, 10, 0);
	sleep(9);
	remove_game_object(obj);
	return(1);
}

/* 0x800e4eac */
void reset_high_scores(void)
{
	register int	i;
	char	src[16], dst[16];

	printf("reseting high scores\n");
	for(i = 0; i < 10; i++)
	{
		high_scores[i].name[0] = default_scores[i].name[0];
		high_scores[i].name[1] = default_scores[i].name[1];
		high_scores[i].name[2] = default_scores[i].name[2];
		high_scores[i].name[3] = 0;
		high_scores[i].score = default_scores[i].score;
		graphical_printf(src, "HSTDDF%02d.WMS", i + 1);
		graphical_printf(dst, "HSTD%02d.WMS", i + 1);
		file_copy(src, dst);
	}
	write_scores();
}

/* 0x800e5014 */
void load_high_scores(void)
{
	register int	i;
	unsigned int	packed;
	int	score;

	for(i = 0; i < 10; i++)
	{
		get_adjustment(0x48 + i, (int *)&packed);
		get_adjustment(0x3e + i, &score);
		high_scores[i].name[0] = (packed & 0xff0000) >> 16;
		high_scores[i].name[1] = (packed & 0xff00) >> 8;
		high_scores[i].name[2] = packed & 0xff;
		high_scores[i].name[3] = 0;
		high_scores[i].score = score;
	}
}

/* 0x800e5114 */
void write_scores(void)
{
	register int	i;
	int	packed, score;

	for(i = 0; i < 10; i++)
	{
		packed = ((high_scores[i].name[3] & 0xff) << 24) | ((high_scores[i].name[0] & 0xff) << 16) | ((high_scores[i].name[1] & 0xff) << 8) | (high_scores[i].name[2] & 0xff);
		score = high_scores[i].score;
		set_adjustment(0x48 + i, packed);
		set_adjustment(0x3e + i, score);
	}
}

/* 0x800e521c: the attract table's draw callback: with autographs each place is its score and
   its signature picture, 70 apart; otherwise its score and initials, 30 apart, scrolling at
   half speed; lines are drawn only between -64 and 320 */
void draw_high_scores(game_object_t *obj)
{
	int	i;
	float	y;

	if(high_score_autograph)
	{
		for(i = 0; i < 10; i++)
		{
			y = 230.0f - (float)i * 70.0f + (float)high_score_scroll;
			if(-64.0f < y && y < 320.0f)
			{
				graphical_printf(text_buffer, "%ld", high_scores[i].score);
				draw_string_centered(1, text_buffer, 300.0f, y - 12.0f, -1);
				guTexSource(find_surface(0x7100 + i)->handle);
				guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
				draw_quad(100.0f, y, 128.0f, 64.0f, 255.0f, 128.0f);
			}
		}
	}
	else
	{
		for(i = 0; i < 10; i++)
		{
			y = 250.0f - (float)i * 30.0f + (float)(high_score_scroll / 2);
			if(-64.0f < y && y < 320.0f)
			{
				graphical_printf(text_buffer, "%ld", high_scores[i].score);
				draw_string_centered(1, text_buffer, 320.0f, y - 12.0f, -1);
				graphical_printf(text_buffer, "%s", high_scores[i].name);
				draw_string_centered(1, text_buffer, 100.0f, y - 12.0f, -1);
			}
		}
	}
	draw_string_centered(1, "HIGH SCORES", 200.0f, 230.0f, -1);
}

/* 0x800e5560: at game over, a player whose score makes the table enters it: the place is the
   first one the score reaches, found from the bottom */
void check_high_score(player_stats_t *p)
{
	int	place;
	int	i;

	if(p->score == 0)
	{
		return;
	}
	if(p->score < high_scores[9].score)
	{
		return;
	}
	normal_screen();
	player_stats_g = p;
	increment_audit(0x23);
	if(p == &player_stats[0])
	{
		graphical_printf(autograph_player, "PLAYER 1");
	}
	else
	{
		graphical_printf(autograph_player, "PLAYER 2");
	}
	if(!(p->score < high_scores[0].score))
	{
		score = p->score;
		if(high_score_autograph)
		{
			autograph_entry(0);
			write_scores();
		}
		else
		{
			func_800e6670(0);
			write_scores();
		}
	}
	else
	{
		for(i = 9; i > 0; i--)
		{
			if(!(p->score < high_scores[i].score) && p->score < high_scores[i - 1].score)
			{
				place = i;
			}
		}
		score = p->score;
		if(high_score_autograph)
		{
			autograph_entry(place);
			write_scores();
		}
		else
		{
			func_800e6670(place);
			write_scores();
		}
	}
}

/* 0x800e5788: twenty seconds to sign with the gun (game phase 0x100: gun_vblank inks while
   the trigger is held), then the signature becomes the place's picture */
void autograph_entry(int place)
{
	short	obj;
	int	x;
	int	y;
	int	phase;

	autograph_done = 0;
	autograph_timer = 1200;
	autograph_ink = malloc(0x10000);
	autograph_pixels = malloc(0x20000);
	for(y = 0; y < 128; y++)
	{
		for(x = 0; x < 256; x++)
		{
			INK(x, y) = 0;
		}
	}
	phase = game_phase;
	game_phase = 0x100;
	set_frame_factor(1);
	func_800e7568();
	stream_stop();
	set_bgnd_color(0xff808080);
	obj = add_object1_wrapper(0x5000, draw_autograph_board, 0, 0);
	while(!autograph_done)
	{
		sleep(1);
		autograph_timer--;
		if(autograph_timer < 0)
		{
			autograph_done = 1;
		}
	}
	create_hstd_tex(autograph_ink, place);
	free(autograph_ink);
	free(autograph_pixels);
	set_bgnd_color(0xff000000);
	remove_game_object(obj);
	set_frame_factor(3);
	game_phase = phase;
	specific_gamma_load(0.0f, 1.0f);
}

/* 0x800e59b8: ink at a gun position (the board's corner is 72, 64 on screen): 350 on the
   pixel, 200 on its four neighbours, 80 on the diagonals; nothing within 2 of the edge */
void autograph_ink_at(float sx, float sy)
{
	int	x;
	int	y;

	x = (int)sx - 72;
	y = (int)sy - 64;
	if(x < 2 || !(x < 255) || y < 2 || !(y < 127))
	{
		return;
	}
	INK(x, y) = INK(x, y) + 350;
	INK(x, y + 1) = INK(x, y + 1) + 200;
	INK(x, y - 1) = INK(x, y - 1) + 200;
	INK(x + 1, y) = INK(x + 1, y) + 200;
	INK(x - 1, y) = INK(x - 1, y) + 200;
	INK(x + 1, y + 1) = INK(x + 1, y + 1) + 80;
	INK(x - 1, y + 1) = INK(x - 1, y + 1) + 80;
	INK(x + 1, y - 1) = INK(x + 1, y - 1) + 80;
	INK(x - 1, y - 1) = INK(x - 1, y - 1) + 80;
}

#ifdef PORT_HOST
extern void	port_lfb_write(const unsigned int *src, int x, int y, int w, int h);
#else
/* the DMA descriptor list is built through the global pointer register, as in movie.c */
register unsigned int	*dma asm("gp");
#endif

/* 0x800e5d94: the signing board's draw callback: the board picture, then the ink written
   straight into the frame buffer at 72, 64 as 555 red (full depth); ink over 255 runs into
   the row below it; the player for the first two seconds and the seconds left */
void draw_autograph_board(game_object_t *obj)
{
	unsigned int	lfb;
	unsigned int	*row;
	int	y;
	int	x;
	int	v;

	guTexSource(find_surface(0x7110)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
	draw_quad(200.0f, 128.0f, 400.0f, 256.0f, 255.0f, 255.0f);
	grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);
#ifndef PORT_HOST
	grDisableAllEffects();
	grDitherMode(GR_DITHER_DISABLE);
	grLfbWriteMode(GR_LFBWRITEMODE_555_DEPTH);
	grLfbConstantAlpha(0xff);
#endif
	grConstantColorValue(0xff000000);
	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
#ifndef PORT_HOST
	grLfbBypassMode(GR_LFBBYPASS_ENABLE);
#endif
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
#ifndef PORT_HOST
	grLfbBegin();
	lfb = (unsigned int)grLfbGetWritePtr(GR_BUFFER_BACKBUFFER);
	lfb += 0x40000;
	lfb += 0x120;
#endif
	for(y = 127; y >= 0; y--)
	{
		for(x = 0; x < 256; x++)
		{
			v = INK(x, y);
			if(v < 256)
			{
				v &= 0xf8;
				PIXEL(x, y) = (v << 7) | 0xffff0000;
			}
			else
			{
				PIXEL(x, y) = 0xffffec00;
				if(y - 1 > 0)
				{
					v = INK(x, y) - 255;
					INK(x, y) = 255;
					INK(x, y - 1) = INK(x, y - 1) + v;
				}
			}
		}
	}
#ifdef PORT_HOST
	(void)row; (void)lfb;
	port_lfb_write(autograph_pixels, 72, 64, 256, 128);
#else
	y = 0;
	while(y < 128)
	{
		row = autograph_pixels + y * 256;
		dma[0] = 0x400;
		dma[1] = (unsigned int)row & 0x1fffffff;
		dma[2] = lfb & 0x1fffffff;
		dma[3] = (unsigned int)(dma + 4);
		dma += 4;
		lfb += 0x1000;
		y++;
	}
	grLfbEnd();
#endif
	grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
	if(autograph_timer / 60 >= 18)
	{
		draw_string_centered(1, autograph_player, 200.0f, 128.0f, 0xff0000);
	}
	graphical_printf(text_buffer, "%d", autograph_timer / 60);
	draw_string_centered(1, text_buffer, 188.0f, 10.0f, 0);
}

/* 0x800e6208: copy a high-score photo (a 40-byte .WMS header and 32K 16-bit texels) */
void file_copy(char *readfile, char *writefile)
{
	FILE	*f;
	char	header[0x28];
	unsigned short *texels;

	remove(writefile);
	texels = malloc(0x10000);
	f = fopen(readfile, "rb");
	if(f)
	{
		fread(header, 0x28, 1, f);
		fread(texels, 2, 0x8000, f);
		fclose(f);
	}
	else
	{
		printf("Could not open readfile %s in file_copy\n", readfile);
	}
	f = fopen(writefile, "wb");
	if(f)
	{
		fwrite(header, 0x28, 1, f);
		fwrite(texels, 2, 0x8000, f);
		fclose(f);
	}
	else
	{
		printf("Could not open writefile %s in file_copy\n", writefile);
	}
	free(texels);
}
