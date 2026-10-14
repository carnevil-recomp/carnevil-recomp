/*
 * text.c - CarnEvil's own text renderer (0x800dbf9c-0x800de6d8): strings are drawn as quads from
 * the FONT0001.WMS texture using per-font glyph tables (w, h, u, v per character ',' .. 'Z').
 * The GOOSE fonts/ostrings modules are linked but never used by the game.
 */
#include <glide/glide.h>
#include "texmap.h"

/* textured vertex with iterated RGB (the SDK header only has the ARGB form) */
typedef struct
{
	float	x;
	float	y;
	float	oow;
	float	sow;
	float	tow;
	float	r;
	float	g;
	float	b;
} MidVertexWithTRGB;

typedef struct glyph
{
	int	w;
	int	h;
	int	u;		/* left texel column */
	int	v;		/* bottom texel row (t runs upward) */
} glyph_t;

#define FIRST_GLYPH	0x2c		/* ',' */
#define NUM_GLYPHS	0x2f		/* ',' .. 'Z' */

static glyph_t	font0_glyphs[NUM_GLYPHS] =
{
	{ 5, 12, 0, 255 },	/* ',' */
	{ 6, 12, 5, 255 },	/* '-' */
	{ 4, 12, 11, 255 },	/* '.' */
	{ 5, 12, 15, 255 },	/* '/' */
	{ 7, 12, 20, 255 },	/* '0' */
	{ 7, 12, 27, 255 },	/* '1' */
	{ 9, 12, 34, 255 },	/* '2' */
	{ 7, 12, 43, 255 },	/* '3' */
	{ 8, 12, 50, 255 },	/* '4' */
	{ 8, 12, 58, 255 },	/* '5' */
	{ 8, 12, 66, 255 },	/* '6' */
	{ 7, 12, 74, 255 },	/* '7' */
	{ 8, 12, 81, 255 },	/* '8' */
	{ 8, 12, 89, 255 },	/* '9' */
	{ 4, 12, 97, 255 },	/* ':' */
	{ 4, 12, 101, 255 },	/* ';' */
	{ 8, 12, 105, 255 },	/* '<' */
	{ 8, 12, 113, 255 },	/* '=' */
	{ 8, 12, 121, 255 },	/* '>' */
	{ 8, 12, 129, 255 },	/* '?' */
	{ 13, 12, 137, 255 },	/* '@' */
	{ 9, 12, 150, 255 },	/* 'A' */
	{ 9, 12, 159, 255 },	/* 'B' */
	{ 10, 12, 168, 255 },	/* 'C' */
	{ 10, 12, 178, 255 },	/* 'D' */
	{ 9, 12, 188, 255 },	/* 'E' */
	{ 8, 12, 197, 255 },	/* 'F' */
	{ 10, 12, 205, 255 },	/* 'G' */
	{ 10, 12, 215, 255 },	/* 'H' */
	{ 4, 12, 225, 255 },	/* 'I' */
	{ 7, 12, 229, 255 },	/* 'J' */
	{ 10, 12, 236, 255 },	/* 'K' */
	{ 7, 12, 246, 255 },	/* 'L' */
	{ 11, 12, 0, 243 },	/* 'M' */
	{ 10, 12, 11, 243 },	/* 'N' */
	{ 10, 12, 21, 243 },	/* 'O' */
	{ 9, 12, 31, 243 },	/* 'P' */
	{ 10, 12, 40, 243 },	/* 'Q' */
	{ 10, 12, 50, 243 },	/* 'R' */
	{ 8, 12, 60, 243 },	/* 'S' */
	{ 10, 12, 68, 243 },	/* 'T' */
	{ 9, 12, 78, 243 },	/* 'U' */
	{ 9, 12, 87, 243 },	/* 'V' */
	{ 13, 12, 96, 243 },	/* 'W' */
	{ 8, 12, 109, 243 },	/* 'X' */
	{ 10, 12, 117, 243 },	/* 'Y' */
	{ 8, 12, 127, 243 },	/* 'Z' */
};
static glyph_t	font1_glyphs[NUM_GLYPHS] =
{
	{ 9, 26, 135, 243 },	/* ',' */
	{ 9, 26, 144, 243 },	/* '-' */
	{ 8, 26, 153, 243 },	/* '.' */
	{ 13, 26, 161, 243 },	/* '/' */
	{ 13, 26, 174, 243 },	/* '0' */
	{ 9, 26, 187, 243 },	/* '1' */
	{ 13, 26, 196, 243 },	/* '2' */
	{ 13, 26, 209, 243 },	/* '3' */
	{ 13, 26, 222, 243 },	/* '4' */
	{ 13, 26, 235, 243 },	/* '5' */
	{ 14, 26, 0, 217 },	/* '6' */
	{ 11, 26, 14, 217 },	/* '7' */
	{ 13, 26, 25, 217 },	/* '8' */
	{ 14, 26, 38, 217 },	/* '9' */
	{ 8, 26, 52, 217 },	/* ':' */
	{ 8, 26, 60, 217 },	/* ';' */
	{ 18, 26, 68, 217 },	/* '<' */
	{ 20, 26, 86, 217 },	/* '=' */
	{ 19, 26, 106, 217 },	/* '>' */
	{ 14, 26, 125, 217 },	/* '?' */
	{ 22, 26, 139, 217 },	/* '@' */
	{ 13, 26, 161, 217 },	/* 'A' */
	{ 11, 26, 174, 217 },	/* 'B' */
	{ 11, 26, 185, 217 },	/* 'C' */
	{ 10, 26, 196, 217 },	/* 'D' */
	{ 11, 26, 206, 217 },	/* 'E' */
	{ 11, 26, 217, 217 },	/* 'F' */
	{ 11, 26, 228, 217 },	/* 'G' */
	{ 13, 26, 239, 217 },	/* 'H' */
	{ 8, 26, 0, 191 },	/* 'I' */
	{ 12, 26, 8, 191 },	/* 'J' */
	{ 11, 26, 20, 191 },	/* 'K' */
	{ 11, 26, 31, 191 },	/* 'L' */
	{ 13, 26, 42, 191 },	/* 'M' */
	{ 13, 26, 55, 191 },	/* 'N' */
	{ 11, 26, 68, 191 },	/* 'O' */
	{ 10, 26, 79, 191 },	/* 'P' */
	{ 13, 26, 89, 191 },	/* 'Q' */
	{ 11, 26, 102, 191 },	/* 'R' */
	{ 11, 26, 113, 191 },	/* 'S' */
	{ 12, 26, 124, 191 },	/* 'T' */
	{ 14, 26, 136, 191 },	/* 'U' */
	{ 12, 26, 150, 191 },	/* 'V' */
	{ 15, 26, 162, 191 },	/* 'W' */
	{ 12, 26, 177, 191 },	/* 'X' */
	{ 12, 26, 189, 191 },	/* 'Y' */
	{ 11, 26, 201, 191 },	/* 'Z' */
};
static glyph_t	font2_glyphs[NUM_GLYPHS] =
{
	{ 8, 26, 212, 191 },	/* ',' */
	{ 10, 26, 220, 191 },	/* '-' */
	{ 8, 26, 230, 191 },	/* '.' */
	{ 13, 26, 238, 191 },	/* '/' */
	{ 12, 26, 0, 165 },	/* '0' */
	{ 10, 26, 12, 165 },	/* '1' */
	{ 12, 26, 22, 165 },	/* '2' */
	{ 12, 26, 34, 165 },	/* '3' */
	{ 12, 26, 46, 165 },	/* '4' */
	{ 12, 26, 58, 165 },	/* '5' */
	{ 12, 26, 70, 165 },	/* '6' */
	{ 12, 26, 82, 165 },	/* '7' */
	{ 12, 26, 94, 165 },	/* '8' */
	{ 12, 26, 106, 165 },	/* '9' */
	{ 7, 26, 118, 165 },	/* ':' */
	{ 8, 26, 125, 165 },	/* ';' */
	{ 16, 26, 133, 165 },	/* '<' */
	{ 19, 26, 149, 165 },	/* '=' */
	{ 16, 26, 168, 165 },	/* '>' */
	{ 12, 26, 184, 165 },	/* '?' */
	{ 24, 26, 196, 165 },	/* '@' */
	{ 15, 26, 220, 165 },	/* 'A' */
	{ 12, 26, 235, 165 },	/* 'B' */
	{ 12, 26, 0, 139 },	/* 'C' */
	{ 12, 26, 12, 139 },	/* 'D' */
	{ 12, 26, 24, 139 },	/* 'E' */
	{ 12, 26, 36, 139 },	/* 'F' */
	{ 12, 26, 48, 139 },	/* 'G' */
	{ 14, 26, 60, 139 },	/* 'H' */
	{ 10, 26, 74, 139 },	/* 'I' */
	{ 12, 26, 84, 139 },	/* 'J' */
	{ 14, 26, 96, 139 },	/* 'K' */
	{ 12, 26, 110, 139 },	/* 'L' */
	{ 15, 26, 122, 139 },	/* 'M' */
	{ 13, 26, 137, 139 },	/* 'N' */
	{ 12, 26, 150, 139 },	/* 'O' */
	{ 12, 26, 162, 139 },	/* 'P' */
	{ 12, 26, 174, 139 },	/* 'Q' */
	{ 13, 26, 186, 139 },	/* 'R' */
	{ 12, 26, 199, 139 },	/* 'S' */
	{ 12, 26, 211, 139 },	/* 'T' */
	{ 12, 26, 223, 139 },	/* 'U' */
	{ 14, 26, 235, 139 },	/* 'V' */
	{ 15, 26, 0, 113 },	/* 'W' */
	{ 13, 26, 15, 113 },	/* 'X' */
	{ 14, 26, 28, 113 },	/* 'Y' */
	{ 12, 26, 42, 113 },	/* 'Z' */
};

glyph_t		*font_glyphs[3] = { font0_glyphs, font1_glyphs, font2_glyphs };
texmap_t	font_texmap[1] = { { 0x3ffe, 1, 0xffffff, 0, "FONT0001.WMS" } };
float		font_space_width[3] = { 10.0f, 10.0f, 10.0f };
float		font_spacing[3] = { 0.0f, 0.0f, -2.0f };
int		font_texture_handle;		/* 0x801e6e00 */

extern void	grAlphaTestReferenceValue(FxU8);
extern void	draw_tri(MidVertex *, MidVertex *, MidVertex *);
extern void	draw_tri_rgb(MidVertexWithTRGB *, MidVertexWithTRGB *, MidVertexWithTRGB *);	/* 0x800d1d8c */
extern void	draw_tri_alpha(MidVertexWithTA *, MidVertexWithTA *, MidVertexWithTA *);	/* 0x800d2664 */
extern float	get_screen_pos(float *, float *);
extern unsigned int strlen(char *);
char		text_buffer[256];		/* 0x801e7100: scratch string shared with the attract credits */

void load_font_texmap(void)
{
	load_texmap(font_texmap, 1, "font");
	font_texture_handle = find_surface(0x3ffe)->handle;
}

/* glyph quad: (x, y) top-left on screen, texel rectangle u..u+w, v-h..v (t runs upward) */
#define GLYPH_INDEX(c)	((signed char)(c) - FIRST_GLYPH)
#define IS_GLYPH(c)	((unsigned char)((c) - FIRST_GLYPH) < NUM_GLYPHS)
#define OOW		0.90909088f	/* 1/w for every text vertex */

float		string_width(int, char *);
void		draw_string_alpha(int, char *, float, float, unsigned int, float);

/* 0x800dbf9c: plain string at (x, y) */
void draw_string(int font, char *str, float x, float y)
{
	MidVertex	v0, v1, v2, v3;
	glyph_t		*g;

	guTexSource(find_surface(0x3ffe)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(0x40);
	v0.oow = OOW;
	v1.oow = OOW;
	v2.oow = OOW;
	v3.oow = OOW;
	while(*str)
	{
		if(IS_GLYPH(*str))
		{
			g = &font_glyphs[font][GLYPH_INDEX(*str)];
			v0.x = x;
			v0.y = y;
			v0.sow = ((float)g->u + 0.5f) * OOW;
			v2.tow = ((float)(g->v - g->h) + 1.55f) * OOW;
			v1.x = x + (float)g->w;
			v1.y = y;
			v1.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v3.tow = ((float)(g->v - g->h) + 1.55f) * OOW;
			v2.x = x;
			v2.y = y + (float)g->h;
			v2.sow = ((float)g->u + 0.5f) * OOW;
			v0.tow = ((float)g->v - 0.55f) * OOW;
			v3.x = x + (float)g->w;
			v3.y = y + (float)g->h;
			v3.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v1.tow = ((float)g->v - 0.55f) * OOW;
			draw_tri(&v0, &v1, &v3);
			draw_tri(&v0, &v3, &v2);
			x += (float)g->w;
		}
		else
		{
			x += 10.0f;
		}
		str++;
	}
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
}

/* 0x800dc24c: plain string centred on a projected point */
void draw_string_at(int font, char *str, int pos)
{
	MidVertex	v0, v1, v2, v3;
	glyph_t		*g;
	float		x, y;
	float		width;
	float		screen[3];	/* get_screen_pos writes x, y, w */
	char		*start;

	if(get_screen_pos((float *)pos, screen) == -1.0f)
	{
		return;
	}
	guTexSource(find_surface(0x3ffe)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(0x40);
	width = 0.0f;
	v0.oow = OOW;
	v1.oow = OOW;
	v2.oow = OOW;
	v3.oow = OOW;
	start = str;
	while(*str)
	{
		if(IS_GLYPH(*str))
		{
			g = &font_glyphs[font][GLYPH_INDEX(*str)];
			width += (float)g->w;
		}
		else
		{
			width += 10.0f;
		}
		str++;
	}
	x = screen[0] - width * 0.5f;
	str = start;
	y = screen[1];
	while(*str)
	{
		if(IS_GLYPH(*str))
		{
			g = &font_glyphs[font][GLYPH_INDEX(*str)];
			v0.x = x;
			v0.y = y;
			v0.sow = ((float)g->u + 0.5f) * OOW;
			v2.tow = ((float)(g->v - g->h) + 1.55f) * OOW;
			v1.x = x + (float)g->w;
			v1.y = y;
			v1.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v3.tow = ((float)(g->v - g->h) + 1.55f) * OOW;
			v2.x = x;
			v2.y = y + (float)g->h;
			v2.sow = ((float)g->u + 0.5f) * OOW;
			v0.tow = ((float)g->v - 0.55f) * OOW;
			v3.x = x + (float)g->w;
			v3.y = y + (float)g->h;
			v3.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v1.tow = ((float)g->v - 0.55f) * OOW;
			draw_tri(&v0, &v1, &v3);
			draw_tri(&v0, &v3, &v2);
			x += (float)g->w;
		}
		else
		{
			x += 10.0f;
		}
		str++;
	}
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
}

/* 0x800dc5a4: coloured string centred on a projected point */
void draw_string_at_color(int font, char *str, int pos, unsigned int color)
{
	MidVertex	v0, v1, v2, v3;
	glyph_t		*g;
	float		x, y;
	float		width;
	float		screen[3];	/* get_screen_pos writes x, y, w */
	char		*start;

	if(get_screen_pos((float *)pos, screen) == -1.0f)
	{
		return;
	}
	guTexSource(find_surface(0x3ffe)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB);
	grConstantColorValue(color);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(0x40);
	width = 0.0f;
	v0.oow = OOW;
	v1.oow = OOW;
	v2.oow = OOW;
	v3.oow = OOW;
	start = str;
	while(*str)
	{
		if(IS_GLYPH(*str))
		{
			g = &font_glyphs[font][GLYPH_INDEX(*str)];
			width += (float)g->w;
		}
		else
		{
			width += 10.0f;
		}
		str++;
	}
	x = screen[0] - width * 0.5f;
	str = start;
	y = screen[1];
	while(*str)
	{
		if(IS_GLYPH(*str))
		{
			g = &font_glyphs[font][GLYPH_INDEX(*str)];
			v0.x = x;
			v0.y = y;
			v0.sow = ((float)g->u + 0.5f) * OOW;
			v2.tow = ((float)(g->v - g->h) + 1.55f) * OOW;
			v1.x = x + (float)g->w;
			v1.y = y;
			v1.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v3.tow = ((float)(g->v - g->h) + 1.55f) * OOW;
			v2.x = x;
			v2.y = y + (float)g->h;
			v2.sow = ((float)g->u + 0.5f) * OOW;
			v0.tow = ((float)g->v - 0.55f) * OOW;
			v3.x = x + (float)g->w;
			v3.y = y + (float)g->h;
			v3.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v1.tow = ((float)g->v - 0.55f) * OOW;
			draw_tri(&v0, &v1, &v3);
			draw_tri(&v0, &v3, &v2);
			x += (float)g->w;
		}
		else
		{
			x += 10.0f;
		}
		str++;
	}
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
}

/* 0x800dc908: string in a constant colour, with per-font spacing */
void draw_string_color(int font, char *str, float x, float y, unsigned int color)
{
	MidVertex	v0, v1, v2, v3;
	glyph_t		*g;

	guTexSource(find_surface(0x3ffe)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB);
	grConstantColorValue(color);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(0x40);
	v0.oow = OOW;
	v1.oow = OOW;
	v2.oow = OOW;
	v3.oow = OOW;
	while(*str)
	{
		if(IS_GLYPH(*str))
		{
			g = &font_glyphs[font][GLYPH_INDEX(*str)];
			v0.x = x;
			v0.y = y;
			v0.sow = ((float)g->u + 0.5f) * OOW;
			v2.tow = ((float)(g->v - g->h) + 0.5f) * OOW;
			v1.x = x + (float)g->w;
			v1.y = y;
			v1.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v3.tow = ((float)(g->v - g->h) + 0.5f) * OOW;
			v2.x = x;
			v2.y = y + (float)g->h;
			v2.sow = ((float)g->u + 0.5f) * OOW;
			v0.tow = ((float)g->v - 0.5f) * OOW;
			v3.x = x + (float)g->w;
			v3.y = y + (float)g->h;
			v3.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v1.tow = ((float)g->v - 0.5f) * OOW;
			draw_tri(&v0, &v1, &v3);
			draw_tri(&v0, &v3, &v2);
			x += (float)g->w + font_spacing[font];
		}
		else
		{
			x += font_space_width[font];
		}
		str++;
	}
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
}

extern float	get_screen_pos(float *, float *);		/* 0x800d07a8: world point -> screen x,y; -1 when off screen */

/* 0x800dcbcc: string with a colour gradient: rgb[0..2]/[3..5] run along the top edge,
   rgb[6..8]/[9..11] along the bottom (chroma-keyed, iterated RGB) */
void draw_string_gradient(int font, char *str, float x, float y, float *rgb)
{
	MidVertexWithTRGB	v0, v1, v2, v3;
	glyph_t		*g;
	float		k;
	float		n;
	float		t;
	float		dr0, dg0, db0;
	float		dr1, dg1, db1;

	grChromakeyMode(GR_CHROMAKEY_ENABLE);
	grChromakeyValue(0x101010);
	guTexSource(find_surface(0x3ffe)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB);
	k = OOW;
	v0.oow = k;
	v1.oow = k;
	v2.oow = k;
	v3.oow = k;
	n = 1.0f / (float)strlen(str);
	dr0 = (rgb[3] - rgb[0]) * n;
	dg0 = (rgb[4] - rgb[1]) * n;
	db0 = (rgb[5] - rgb[2]) * n;
	dr1 = (rgb[9] - rgb[6]) * n;
	dg1 = (rgb[10] - rgb[7]) * n;
	db1 = (rgb[11] - rgb[8]) * n;
	t = 0.0f;
	while(*str)
	{
		if(IS_GLYPH(*str))
		{
			g = &font_glyphs[font][GLYPH_INDEX(*str)];
			v0.x = x;
			v0.y = y;
			v0.sow = ((float)g->u + 0.5f) * k;
			v2.tow = ((float)(g->v - g->h) + 1.5f) * k;
			v0.r = rgb[0] + t * dr0;
			v0.g = rgb[1] + t * dg0;
			v0.b = rgb[2] + t * db0;
			v1.x = x + (float)g->w;
			v1.y = y;
			v1.sow = ((float)(g->u + g->w) - 0.5f) * k;
			t += 1.0f;
			v3.tow = ((float)(g->v - g->h) + 1.5f) * k;
			v1.r = rgb[0] + t * dr0;
			v1.g = rgb[1] + t * dg0;
			v1.b = rgb[2] + t * db0;
			v2.x = x;
			v2.y = y + (float)g->h;
			v2.sow = ((float)g->u + 0.5f) * k;
			t -= 1.0f;
			v0.tow = ((float)g->v - 1.5f) * k;
			v2.r = rgb[6] + t * dr1;
			v2.g = rgb[7] + t * dg1;
			v2.b = rgb[8] + t * db1;
			v3.x = x + (float)g->w;
			v3.y = y + (float)g->h;
			v3.sow = ((float)(g->u + g->w) - 0.5f) * k;
			t += 1.0f;
			v1.tow = ((float)g->v - 1.5f) * k;
			v3.r = rgb[6] + t * dr1;
			v3.g = rgb[7] + t * dg1;
			v3.b = rgb[8] + t * db1;
			draw_tri_rgb(&v0, &v1, &v3);
			draw_tri_rgb(&v0, &v3, &v2);
			x += (float)g->w;
		}
		else
		{
			x += 10.0f;
		}
		str++;
	}
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
}

/* 0x800dd020: float -> "[-]ddddd.dddd" */
void float_to_string(char *buf, float v)
{
	int	d;

	if(v < 0.0f)
	{
		*buf++ = '-';
		v = -v;
	}
	if(v >= 10000.0f)
	{
		d = v / 10000.0f;
		v -= d * 10000;
		*buf++ = d + '0';
		d = v / 1000.0f;
		v -= d * 1000;
		*buf++ = d + '0';
		d = v / 100.0f;
		v -= d * 100;
		*buf++ = d + '0';
		d = v / 10.0f;
		v -= d * 10;
		*buf++ = d + '0';
	}
	else if(v >= 1000.0f)
	{
		d = v / 1000.0f;
		v -= d * 1000;
		*buf++ = d + '0';
		d = v / 100.0f;
		v -= d * 100;
		*buf++ = d + '0';
		d = v / 10.0f;
		v -= d * 10;
		*buf++ = d + '0';
	}
	else if(v >= 100.0f)
	{
		d = v / 100.0f;
		v -= d * 100;
		*buf++ = d + '0';
		d = v / 10.0f;
		v -= d * 10;
		*buf++ = d + '0';
	}
	else if(v >= 10.0f)
	{
		d = v / 10.0f;
		v -= d * 10;
		*buf++ = d + '0';
	}
	else if(v < 1.0f)
	{
		*buf++ = '0';
		goto fraction;
	}
	d = v;
	*buf++ = d + '0';
	v -= d;
fraction:
	*buf++ = '.';
	d = v / 0.1f;
	v -= d * 0.1f;
	*buf++ = d + '0';
	d = v / 0.01f;
	v -= d * 0.01f;
	*buf++ = d + '0';
	d = v / 0.001f;
	v -= d * 0.001f;
	*buf++ = d + '0';
	d = v / 0.0001f;
	*buf++ = d + '0';
	*buf = 0;
}

/* 0x800dd404: debug line: label then three angles (radians) printed in degrees */
void draw_vector_degrees(char *label, float *v, int y)
{
	draw_string(0, label, 10.0f, (float)y);
	float_to_string(text_buffer, v[0] * 57.2957f);
	draw_string(0, text_buffer, 90.0f, (float)y);
	float_to_string(text_buffer, v[1] * 57.2957f);
	draw_string(0, text_buffer, 190.0f, (float)y);
	float_to_string(text_buffer, v[2] * 57.2957f);
	draw_string(0, text_buffer, 290.0f, (float)y);
}

/* 0x800dd4e8: debug line: label then three floats */
void draw_vector(char *label, float *v, int y)
{
	draw_string(0, label, 10.0f, (float)y);
	float_to_string(text_buffer, v[0]);
	draw_string(0, text_buffer, 90.0f, (float)y);
	float_to_string(text_buffer, v[1]);
	draw_string(0, text_buffer, 190.0f, (float)y);
	float_to_string(text_buffer, v[2]);
	draw_string(0, text_buffer, 290.0f, (float)y);
}

/* 0x800dd5a4: short -> decimal string */
void short_to_string(char *buf, short v)
{
	if(v < 0)
	{
		*buf++ = '-';
		v = -v;
	}
	if(v >= 10000)
	{
		*buf++ = v / 10000 + '0';
		v %= 10000;
		*buf++ = v / 1000 + '0';
		v %= 1000;
		*buf++ = v / 100 + '0';
		v %= 100;
		*buf++ = v / 10 + '0';
		v %= 10;
		*buf++ = v + '0';
	}
	else if(v >= 1000)
	{
		*buf++ = v / 1000 + '0';
		v %= 1000;
		*buf++ = v / 100 + '0';
		v %= 100;
		*buf++ = v / 10 + '0';
		v %= 10;
		*buf++ = v + '0';
	}
	else if(v >= 100)
	{
		*buf++ = v / 100 + '0';
		v %= 100;
		*buf++ = v / 10 + '0';
		v %= 10;
		*buf++ = v + '0';
	}
	else if(v >= 10)
	{
		*buf++ = v / 10 + '0';
		v %= 10;
		*buf++ = v + '0';
	}
	else
	{
		*buf++ = v + '0';
	}
	*buf = 0;
}

/* 0x800dd8c0: leftover from an older fixed-width font: A-Z are 0..25, '-'..'9' are 26..38,
   anything else 40; '.' (27) is 6 wide, the rest 10. Only draws when centred. */
void draw_string_old(int font, char *str, float x, float y, int centered)
{
	float	width;
	char	*start;
	int	idx;

	width = 0.0f;
	start = str;
	while(*str)
	{
		if((unsigned char)(*str - 'A') < 26)
		{
			idx = *str - 'A';
		}
		else if((unsigned char)(*str - '-') < 13)
		{
			idx = *str - '-' + 26;
		}
		else
		{
			idx = 40;
		}
		if(idx == 27)
		{
			width += 6.0f;
		}
		else
		{
			width += 10.0f;
		}
		str++;
	}
	if(centered & 1)
	{
		draw_string(0, start, x - width / 2.0f, y);
	}
}

/* 0x800dd990: plain string at half size */
void draw_string_half(int font, char *str, float x, float y)
{
	MidVertex	v0, v1, v2, v3;
	glyph_t		*g;

	guTexSource(find_surface(0x3ffe)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(0x40);
	v0.oow = OOW;
	v1.oow = OOW;
	v2.oow = OOW;
	v3.oow = OOW;
	while(*str)
	{
		if(IS_GLYPH(*str))
		{
			g = &font_glyphs[font][GLYPH_INDEX(*str)];
			v0.x = x;
			v0.y = y;
			v0.sow = ((float)g->u + 0.5f) * OOW;
			v2.tow = ((float)(g->v - g->h) + 1.55f) * OOW;
			v1.x = x + (float)(g->w / 2);
			v1.y = y;
			v1.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v3.tow = ((float)(g->v - g->h) + 1.55f) * OOW;
			v2.x = x;
			v2.y = y + (float)(g->h / 2);
			v2.sow = ((float)g->u + 0.5f) * OOW;
			v0.tow = ((float)g->v - 0.55f) * OOW;
			v3.x = x + (float)(g->w / 2);
			v3.y = y + (float)(g->h / 2);
			v3.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v1.tow = ((float)g->v - 0.55f) * OOW;
			draw_tri(&v0, &v1, &v3);
			draw_tri(&v0, &v3, &v2);
			x += (float)(g->w / 2);
		}
		else
		{
			x += 5.0f;
		}
		str++;
	}
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
}

/* 0x800ddc90: coloured, scaled string centred on x */
void draw_string_scaled_centered(int font, char *str, float x, float y, unsigned int color, float scale)
{
	MidVertex	v0, v1, v2, v3;
	glyph_t		*g;

	x -= scale * string_width(font, str) / 2.0f;
	guTexSource(find_surface(0x3ffe)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB);
	grConstantColorValue(color);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(8);
	v0.oow = OOW;
	v1.oow = OOW;
	v2.oow = OOW;
	v3.oow = OOW;
	while(*str)
	{
		if(IS_GLYPH(*str))
		{
			g = &font_glyphs[font][GLYPH_INDEX(*str)];
			v0.x = x;
			v0.y = y;
			v0.sow = ((float)g->u + 0.5f) * OOW;
			v2.tow = ((float)(g->v - g->h) + 1.5f) * OOW;
			v1.x = x + (float)g->w * scale;
			v1.y = y;
			v1.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v3.tow = ((float)(g->v - g->h) + 1.5f) * OOW;
			v2.x = x;
			v2.y = y + (float)g->h * scale;
			v2.sow = ((float)g->u + 0.5f) * OOW;
			v0.tow = ((float)g->v - 1.5f) * OOW;
			v3.x = x + (float)g->w * scale;
			v3.y = y + (float)g->h * scale;
			v3.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v1.tow = ((float)g->v - 1.5f) * OOW;
			draw_tri(&v0, &v1, &v3);
			draw_tri(&v0, &v3, &v2);
			x += scale * (float)g->w;
		}
		else
		{
			x += scale * 10.0f;
		}
		str++;
	}
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
}

/* 0x800ddf74: coloured string centred on x */
void draw_string_centered(int font, char *str, float x, float y, unsigned int color)
{
	x -= string_width(font, str) / 2.0f;
	draw_string_color(font, str, x, y, color);
}

/* 0x800ddff4: coloured string with iterated alpha, centred on x */
void draw_string_centered_alpha(int font, char *str, float x, float y, unsigned int color, float alpha)
{
	draw_string_alpha(font, str, x - string_width(font, str) / 2.0f, y, color, alpha);
}

/* 0x800de084: pixel width of a string in a font (glyph widths plus spacing, space width otherwise) */
float string_width(int font, char *str)
{
	float		width = 0.0f;
	unsigned char	c;

	while(*str)
	{
		c = *str;
		if(IS_GLYPH(c))
		{
			width += (float)font_glyphs[font][GLYPH_INDEX(c)].w + font_spacing[font];
		}
		else
		{
			width += font_space_width[font];
		}
		str++;
	}
	return(width);
}

/* 0x800de118: coloured string with iterated alpha */
void draw_string_alpha(int font, char *str, float x, float y, unsigned int color, float alpha)
{
	MidVertexWithTA	v0, v1, v2, v3;
	glyph_t		*g;

	guTexSource(find_surface(0x3ffe)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB);
	grConstantColorValue(color);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(10);
	v0.a = alpha;
	v1.a = alpha;
	v2.a = alpha;
	v3.a = alpha;
	v0.oow = OOW;
	v1.oow = OOW;
	v2.oow = OOW;
	v3.oow = OOW;
	while(*str)
	{
		if(IS_GLYPH(*str))
		{
			g = &font_glyphs[font][GLYPH_INDEX(*str)];
			v0.x = x;
			v0.y = y;
			v0.sow = ((float)g->u + 0.5f) * OOW;
			v2.tow = ((float)(g->v - g->h) + 0.5f) * OOW;
			v1.x = x + (float)g->w;
			v1.y = y;
			v1.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v3.tow = ((float)(g->v - g->h) + 0.5f) * OOW;
			v2.x = x;
			v2.y = y + (float)g->h;
			v2.sow = ((float)g->u + 0.5f) * OOW;
			v0.tow = ((float)g->v - 0.5f) * OOW;
			v3.x = x + (float)g->w;
			v3.y = y + (float)g->h;
			v3.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v1.tow = ((float)g->v - 0.5f) * OOW;
			draw_tri_alpha(&v0, &v1, &v3);
			draw_tri_alpha(&v0, &v3, &v2);
			x += (float)g->w + font_spacing[font];
		}
		else
		{
			x += font_space_width[font];
		}
		str++;
	}
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
}

/* 0x800de3f0: coloured, scaled string */
void draw_string_scaled(int font, char *str, float x, float y, unsigned int color, float scale)
{
	MidVertex	v0, v1, v2, v3;
	glyph_t		*g;
	char		*p;

	for(p = str; *p; p++)
	{
	}
	guTexSource(find_surface(0x3ffe)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB);
	grConstantColorValue(color);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(8);
	v0.oow = OOW;
	v1.oow = OOW;
	v2.oow = OOW;
	v3.oow = OOW;
	while(*str)
	{
		if(IS_GLYPH(*str))
		{
			g = &font_glyphs[font][GLYPH_INDEX(*str)];
			v0.x = x;
			v0.y = y;
			v0.sow = ((float)g->u + 0.5f) * OOW;
			v2.tow = ((float)(g->v - g->h) + 1.5f) * OOW;
			v1.x = x + (float)g->w * scale;
			v1.y = y;
			v1.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v3.tow = ((float)(g->v - g->h) + 1.5f) * OOW;
			v2.x = x;
			v2.y = y + (float)g->h * scale;
			v2.sow = ((float)g->u + 0.5f) * OOW;
			v0.tow = ((float)g->v - 1.5f) * OOW;
			v3.x = x + (float)g->w * scale;
			v3.y = y + (float)g->h * scale;
			v3.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v1.tow = ((float)g->v - 1.5f) * OOW;
			draw_tri(&v0, &v1, &v3);
			draw_tri(&v0, &v3, &v2);
			x += scale * (float)g->w;
		}
		else
		{
			x += scale * 10.0f;
		}
		str++;
	}
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
}

/* 0x800de6d8: coloured, scaled string ending at x (right-aligned) */
void draw_string_scaled_right(int font, char *str, float x, float y, unsigned int color, float scale)
{
	MidVertex	v0, v1, v2, v3;
	glyph_t		*g;
	float		width;
	char		*p;

	width = 0.0f;
	for(p = str; *p; p++)
	{
		if(IS_GLYPH(*p))
		{
			width += (float)font_glyphs[font][GLYPH_INDEX(*p)].w + font_spacing[font];
		}
		else
		{
			width += font_space_width[font];
		}
	}
	x -= scale * width;
	guTexSource(find_surface(0x3ffe)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB);
	grConstantColorValue(color);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(8);
	v0.oow = OOW;
	v1.oow = OOW;
	v2.oow = OOW;
	v3.oow = OOW;
	while(*str)
	{
		if(IS_GLYPH(*str))
		{
			g = &font_glyphs[font][GLYPH_INDEX(*str)];
			v0.x = x;
			v0.y = y;
			v0.sow = ((float)g->u + 0.5f) * OOW;
			v2.tow = ((float)(g->v - g->h) + 1.5f) * OOW;
			v1.x = x + (float)g->w * scale;
			v1.y = y;
			v1.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v3.tow = ((float)(g->v - g->h) + 1.5f) * OOW;
			v2.x = x;
			v2.y = y + (float)g->h * scale;
			v2.sow = ((float)g->u + 0.5f) * OOW;
			v0.tow = ((float)g->v - 1.5f) * OOW;
			v3.x = x + (float)g->w * scale;
			v3.y = y + (float)g->h * scale;
			v3.sow = ((float)(g->u + g->w) - 0.5f) * OOW;
			v1.tow = ((float)g->v - 1.5f) * OOW;
			draw_tri(&v0, &v1, &v3);
			draw_tri(&v0, &v3, &v2);
			x += scale * (float)g->w;
		}
		else
		{
			x += scale * 10.0f;
		}
		str++;
	}
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
}
