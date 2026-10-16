/*
 * render_gl.c - 3dfx Glide 2.x as the game drives it, on OpenGL 2.1 fixed function.
 *
 * The Voodoo takes screen-space vertices (x, y in pixels, oow = 1/w, sow/tow = s/w, t/w in
 * texel units) and a small state machine set by the gr/gu calls. Each draw call renders
 * straight into the GL back buffer; dma_start() (the Galileo DMA kick that the real board
 * answers with grBufferSwap from its interrupt handler) presents the frame. Without a window
 * (video_null.c) the state is tracked and the draws are counted but nothing is rendered.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#elif defined(_WIN32)
/* Windows' opengl32 exports OpenGL 1.1 only: the newer calls are loaded once the context exists
   (port_gl_load, called by video_sdl.c with SDL_GL_GetProcAddress) */
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#define GL_FN(type, name)	static type p_##name;
#define GL_FNS \
	GL_FN(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) GL_FN(PFNGLFOGCOORDFPROC, glFogCoordf) \
	GL_FN(PFNGLWINDOWPOS2FPROC, glWindowPos2f) GL_FN(PFNGLGENFRAMEBUFFERSEXTPROC, glGenFramebuffersEXT) \
	GL_FN(PFNGLBINDFRAMEBUFFEREXTPROC, glBindFramebufferEXT) GL_FN(PFNGLGENRENDERBUFFERSEXTPROC, glGenRenderbuffersEXT) \
	GL_FN(PFNGLBINDRENDERBUFFEREXTPROC, glBindRenderbufferEXT) GL_FN(PFNGLRENDERBUFFERSTORAGEEXTPROC, glRenderbufferStorageEXT) \
	GL_FN(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC, glFramebufferRenderbufferEXT) \
	GL_FN(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, glCheckFramebufferStatusEXT)
GL_FNS
#undef GL_FN
int port_gl_load(void *(*getproc)(const char *))
{
	int	missing = 0;
#define GL_FN(type, name)	if(!(p_##name = (type)getproc(#name))) { fprintf(stderr, "[render] OpenGL lacks %s\n", #name); missing++; }
	GL_FNS
#undef GL_FN
	return missing;
}
#define glBlendFuncSeparate		p_glBlendFuncSeparate
#define glFogCoordf			p_glFogCoordf
#define glWindowPos2f			p_glWindowPos2f
#define glGenFramebuffersEXT		p_glGenFramebuffersEXT
#define glBindFramebufferEXT		p_glBindFramebufferEXT
#define glGenRenderbuffersEXT		p_glGenRenderbuffersEXT
#define glBindRenderbufferEXT		p_glBindRenderbufferEXT
#define glRenderbufferStorageEXT	p_glRenderbufferStorageEXT
#define glFramebufferRenderbufferEXT	p_glFramebufferRenderbufferEXT
#define glCheckFramebufferStatusEXT	p_glCheckFramebufferStatusEXT
#else
#define GL_GLEXT_PROTOTYPES	1	/* glWindowPos, glFogCoord, the EXT framebuffer calls (GL 1.4 / EXT_framebuffer_object) */
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#include <glide/glide.h>
#include "hal.h"
#include "video.h"
#include "texture.h"

extern float	hres, vres;
unsigned long	port_triangles;
static unsigned long dump_every;
static int	have_display;
static int	origin_lower_left;
static float	alpha_ref = 0.5f;
static int	win_w, win_h;
extern int	port_hires;		/* cabinet mode below; --hires */

/* ---- Glide state ---- */
static struct {
	GrMipMapId_t	tex;
	int		tex_combine, color_combine, alpha_source;
	int		cc_invert;		/* grColorCombine's invert flag: output = 1 - result */
	unsigned int	const_color, chroma_value;
	int		chroma_mode;
	int		filter_min, filter_mag, clamp_s, clamp_t;
	int		blend_rgb_src, blend_rgb_dst, blend_a_src, blend_a_dst;
	int		depth_func, depth_mask, depth_mode;
	int		alpha_test;
	int		cull;
	int		dither;			/* grDitherMode: GR_DITHER_DISABLE / 2x2 / 4x4 */
} st;

static void glide_defaults(void)
{
	memset(&st, 0, sizeof st);
	st.tex = GR_NULL_MIPMAP_HANDLE;
	st.depth_func = GR_CMP_LESS; st.depth_mask = 1; st.depth_mode = GR_DEPTHBUFFER_WBUFFER;
	st.blend_rgb_src = GR_BLEND_ONE; st.blend_rgb_dst = GR_BLEND_ZERO;
	st.blend_a_src = GR_BLEND_ONE; st.blend_a_dst = GR_BLEND_ZERO;
	st.alpha_test = GR_CMP_ALWAYS; st.color_combine = GR_COLORCOMBINE_DECAL_TEXTURE;
	st.const_color = 0xffffffff; st.filter_min = st.filter_mag = GR_TEXTUREFILTER_BILINEAR;
	st.dither = GR_DITHER_4x4;		/* Glide's window open (0x80161814) sets 4x4 */
}

/* ---- texture upload: Glide texel formats -> RGBA8 ---- */
static void texel_rgba(int fmt, const unsigned char *p, const GuTexTable *tbl, unsigned char *o)
{
	unsigned int v;
	/* the Voodoo widens a channel by repeating its top bits (MAME's voodoo), not by rounding */
#define X5(c)	(((c) << 3) | ((c) >> 2))
#define X6(c)	(((c) << 2) | ((c) >> 4))
#define X3(c)	(((c) << 5) | ((c) << 2) | ((c) >> 1))
	switch(fmt)
	{
	case GR_TEXFMT_RGB_565: v = p[0] | (p[1] << 8);
		o[0] = X5(v >> 11); o[1] = X6((v >> 5) & 63); o[2] = X5(v & 31); o[3] = 255; break;
	case GR_TEXFMT_ARGB_1555: v = p[0] | (p[1] << 8);
		o[0] = X5((v >> 10) & 31); o[1] = X5((v >> 5) & 31); o[2] = X5(v & 31); o[3] = (v & 0x8000) ? 255 : 0; break;
	case GR_TEXFMT_ARGB_4444: v = p[0] | (p[1] << 8);
		o[0] = ((v >> 8) & 15) * 17; o[1] = ((v >> 4) & 15) * 17; o[2] = (v & 15) * 17; o[3] = (v >> 12) * 17; break;
	case GR_TEXFMT_ALPHA_INTENSITY_88: o[0] = o[1] = o[2] = p[0]; o[3] = p[1]; break;
	case GR_TEXFMT_ARGB_8332: v = p[0] | (p[1] << 8);
		o[0] = X3((v >> 5) & 7); o[1] = X3((v >> 2) & 7); o[2] = (v & 3) * 85; o[3] = v >> 8; break;
	case GR_TEXFMT_RGB_332: v = p[0];
		o[0] = X3(v >> 5); o[1] = X3((v >> 2) & 7); o[2] = (v & 3) * 85; o[3] = 255; break;
	case GR_TEXFMT_ALPHA_8: o[0] = o[1] = o[2] = 255; o[3] = p[0]; break;
	case GR_TEXFMT_INTENSITY_8: o[0] = o[1] = o[2] = p[0]; o[3] = 255; break;
	case GR_TEXFMT_ALPHA_INTENSITY_44: o[0] = o[1] = o[2] = (p[0] & 15) * 17; o[3] = (p[0] >> 4) * 17; break;
	case GR_TEXFMT_P_8: v = tbl ? tbl->palette.data[p[0]] : 0x808080;
		o[0] = (v >> 16) & 255; o[1] = (v >> 8) & 255; o[2] = v & 255; o[3] = 255; break;
	case GR_TEXFMT_AP_88: v = tbl ? tbl->palette.data[p[0]] : 0x808080;
		o[0] = (v >> 16) & 255; o[1] = (v >> 8) & 255; o[2] = v & 255; o[3] = p[1]; break;
	default: o[0] = 255; o[1] = 0; o[2] = 255; o[3] = 255; break;	/* YIQ/NCC: no CarnEvil texture uses them (every .WMS is RGB332, ARGB8332, RGB565, ARGB4444 or AI88) */
	}
}

static unsigned int upload(port_texture_t *t, int keyed, unsigned int key)
{
	const Gu3dfHeader *h = &t->header;
	const unsigned char *src = t->data;
	int	bpt = h->format >= GR_TEXFMT_16BIT ? 2 : 1;
	unsigned int name, lod, level = 0;
	unsigned char kr = (key >> 16) & 255, kg = (key >> 8) & 255, kb = key & 255;
	glGenTextures(1, &name);
	glBindTexture(GL_TEXTURE_2D, name);
	for(lod = h->large_lod; lod <= h->small_lod; lod++, level++)
	{
		unsigned int w = h->width >> (lod - h->large_lod), hh = h->height >> (lod - h->large_lod), i;
		unsigned char *rgba;
		if(!w) w = 1;
		if(!hh) hh = 1;
		rgba = malloc(w * hh * 4);
		for(i = 0; i < w * hh; i++)
		{
			/* t = 0 is the first stored row (the game opens the screen GR_ORIGIN_LOWER_LEFT, see below) */
			unsigned int o = i * 4;
			texel_rgba(h->format, src + i * bpt, &t->table, rgba + o);
			if(keyed && rgba[o] == kr && rgba[o+1] == kg && rgba[o+2] == kb) rgba[o+3] = 0;
		}
		glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, w, hh, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
		free(rgba);
		src += w * hh * bpt;
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, level - 1);
	return name;
}

static void bind_texture(void)
{
	port_texture_t *t = port_texture(st.tex);
	int	keyed = st.chroma_mode == GR_CHROMAKEY_ENABLE;
	int	filt, lod_count;
	if(!t) { glDisable(GL_TEXTURE_2D); return; }
	if(t->gl_dirty) { if(t->gl_name) glDeleteTextures(1, &t->gl_name); if(t->gl_keyed) glDeleteTextures(1, &t->gl_keyed); t->gl_name = t->gl_keyed = 0; t->gl_dirty = 0; }
	if(keyed)
	{
		if(t->gl_keyed && t->gl_key != st.chroma_value) { glDeleteTextures(1, &t->gl_keyed); t->gl_keyed = 0; }
		if(!t->gl_keyed) { t->gl_keyed = upload(t, 1, st.chroma_value); t->gl_key = st.chroma_value; }
		glBindTexture(GL_TEXTURE_2D, t->gl_keyed);
	}
	else
	{
		if(!t->gl_name) t->gl_name = upload(t, 0, 0);
		glBindTexture(GL_TEXTURE_2D, t->gl_name);
	}
	glEnable(GL_TEXTURE_2D);
	lod_count = t->header.small_lod - t->header.large_lod + 1;
	filt = st.filter_min == GR_TEXTUREFILTER_BILINEAR;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, st.filter_mag == GR_TEXTUREFILTER_BILINEAR ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, lod_count > 1 ? (filt ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST) : (filt ? GL_LINEAR : GL_NEAREST));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, st.clamp_s == GR_TEXTURECLAMP_CLAMP ? GL_CLAMP_TO_EDGE : GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, st.clamp_t == GR_TEXTURECLAMP_CLAMP ? GL_CLAMP_TO_EDGE : GL_REPEAT);
}

static GLenum blend_factor(int f)
{
	switch(f)
	{
	case GR_BLEND_ZERO: return GL_ZERO; case GR_BLEND_SRC_ALPHA: return GL_SRC_ALPHA;
	case GR_BLEND_SRC_COLOR: return GL_SRC_COLOR; case GR_BLEND_DST_ALPHA: return GL_DST_ALPHA;
	case GR_BLEND_ONE: return GL_ONE; case GR_BLEND_ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
	case GR_BLEND_ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR; case GR_BLEND_ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
	case GR_BLEND_ALPHA_SATURATE: return GL_SRC_ALPHA_SATURATE;
	}
	return GL_ONE;
}

static GLenum cmp_func(int f)
{
	static const GLenum t[8] = { GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS };
	return t[f & 7];
}

/* push the Glide state into GL before a draw */
/* the per-draw colour the vertices inherit unless the combine iterates it */
static unsigned char cur_rgb[3], cur_alpha;
static int	cur_tex_rgb;		/* the bound texture feeds the colour, not only the alpha */
static int	have_uv = 1;		/* the vertex layout carries s/w, t/w (a flat-lit layout does not) */

static int iterated_rgb(void)
{
	switch(st.color_combine)
	{
	case GR_COLORCOMBINE_ITRGB: case GR_COLORCOMBINE_ITRGB_DELTA0: case GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB:
	case GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB_DELTA0: case GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB_ADD_ALPHA:
	case GR_COLORCOMBINE_TEXTURE_TIMES_ALPHA_ADD_ITRGB: case GR_COLORCOMBINE_TEXTURE_ADD_ITRGB: case GR_COLORCOMBINE_TEXTURE_SUB_ITRGB:
		return 1;
	}
	return 0;
}
static int iterated_alpha(void)
{
	return st.alpha_source == GR_ALPHASOURCE_ITERATED_ALPHA || st.alpha_source == GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA;
}

/* Fog as the Voodoo's table mode does it: the 64-entry table is indexed by the same 16-bit
   floating w the depth test uses (fog_word: 1/w in 16.32 fixed point, leading-zero exponent,
   inverted mantissa), the top 6 bits pick the entry and the next 8 interpolate towards the
   following one (Glide loads the differences as the fog deltas); the pixel's colour is then
   blended towards the fog colour by (blend + 1) / 256, before alpha blending (MAME's voodoo).
   The port evaluates the table at the vertices and lets GL interpolate the factor, which is
   exact at the vertices and close across the game's small triangles. CarnEvil v1.0.3 never
   turns fog on: apply_fog (fog.c) sets GR_FOG_WITH_TABLE only when fog_enabled is set, and
   nothing calls enable_fog; preload_wave clears it. */
static int		fog_mode;
static unsigned int	fog_color;
static unsigned char	fog_table[64];
void grFogMode(int mode) { fog_mode = mode; }
void grFogColorValue(unsigned int c) { fog_color = c; }
void grFogTable(const unsigned char *t) { memcpy(fog_table, t, sizeof fog_table); }
static unsigned fog_word(float oow)
{
	double		iw = (double)oow * 4294967296.0;
	unsigned	t, w;
	int		e;

	if(iw >= 4294967296.0) return 0;
	t = (unsigned)iw;
	if(!(t & 0xffff0000u)) return 0xffff;
	e = __builtin_clz(t);
	w = (e << 12) | ((~t >> (19 - e)) & 0xfff);
	return w < 0xffff ? w + 1 : w;
}
static float fog_factor(float oow)
{
	unsigned	w = fog_word(oow), i = w >> 10;
	int		next = i < 63 ? fog_table[i + 1] : fog_table[i];
	int		blend = fog_table[i] + (((next - fog_table[i]) * (int)((w >> 2) & 0xff)) >> 8);

	return (blend + 1) / 256.0f;
}
static int fog_on(void) { return fog_mode == GR_FOG_WITH_TABLE; }

static void apply_state(void)
{
	/* The Voodoo's colour and alpha paths are separate: a combine whose colour ignores the
	   texture (CCRGB, ITRGB, ZERO, ONE) still takes the texture's alpha when the alpha source
	   asks for it. The smoke columns (draw_smoke: CCRGB in the column's colour, texture alpha
	   times iterated alpha) get their whole shape from that alpha; without it the acid fumes
	   were solid yellow-green quads. */
	int	tex_rgb = st.color_combine != GR_COLORCOMBINE_CCRGB
			&& st.color_combine != GR_COLORCOMBINE_ZERO && st.color_combine != GR_COLORCOMBINE_ONE
			&& st.color_combine != GR_COLORCOMBINE_ITRGB && st.color_combine != GR_COLORCOMBINE_ITRGB_DELTA0;
	int	tex_alpha = st.alpha_source == GR_ALPHASOURCE_TEXTURE_ALPHA || st.alpha_source == GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_CONSTANT_ALPHA
			|| st.alpha_source == GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA;
	int	textured = st.tex != GR_NULL_MIPMAP_HANDLE && have_uv && (tex_rgb || tex_alpha);
	cur_tex_rgb = tex_rgb;
	unsigned char r = (st.const_color >> 16) & 255, g = (st.const_color >> 8) & 255, b = st.const_color & 255, a = st.const_color >> 24;
	static const float white[4] = { 1, 1, 1, 1 };
	if(textured) bind_texture(); else glDisable(GL_TEXTURE_2D);
	switch(st.color_combine)
	{
	case GR_COLORCOMBINE_ZERO: r = g = b = 0; break;
	case GR_COLORCOMBINE_DECAL_TEXTURE: case GR_COLORCOMBINE_ONE: case GR_COLORCOMBINE_DECAL_TEXTURE_ADD_CCALPHA: r = g = b = 255; break;
	case GR_COLORCOMBINE_TEXTURE_TIMES_ALPHA: r = g = b = 255; break;	/* alpha applied through the alpha channel below */
	default: break;							/* CCRGB, TEXTURE_TIMES_CCRGB: constant colour; the iterated modes set the colour per vertex */
	}
	if(st.cc_invert && !(textured && tex_rgb)) { r = 255 - r; g = 255 - g; b = 255 - b; }
	glColor4ub(r, g, b, a);
	cur_rgb[0] = r; cur_rgb[1] = g; cur_rgb[2] = b; cur_alpha = a;
	if(textured)
	{
		/* RGB = texture * colour (or + / - colour); alpha per Glide's alpha source */
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE); glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PRIMARY_COLOR); glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
		if(!tex_rgb)
		{
			/* the colour is the constant or iterated colour alone (cc_invert applied above) */
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PRIMARY_COLOR);
		}
		else if(st.cc_invert)
		{
			/* 1 - tex * col = (1 - col) * tex + 1 * (1 - tex): INTERPOLATE(arg0 = 1 - col, arg1 = white, arg2 = tex) */
			glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, white);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PRIMARY_COLOR); glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_ONE_MINUS_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_CONSTANT); glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SRC2_RGB, GL_TEXTURE); glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
		}
		else if(st.color_combine == GR_COLORCOMBINE_TEXTURE_ADD_ITRGB) glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD);
		else if(st.color_combine == GR_COLORCOMBINE_TEXTURE_SUB_ITRGB) glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_SUBTRACT);
		else glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		switch(st.alpha_source)
		{
		case GR_ALPHASOURCE_TEXTURE_ALPHA: glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE); glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE); break;
		case GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_CONSTANT_ALPHA: case GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA:
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE); glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE); glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PRIMARY_COLOR); break;
		default: /* CC_ALPHA, ITERATED_ALPHA */ glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE); glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PRIMARY_COLOR); break;
		}
	}
	if(st.blend_rgb_src == GR_BLEND_ONE && st.blend_rgb_dst == GR_BLEND_ZERO) glDisable(GL_BLEND);
	else { glEnable(GL_BLEND); glBlendFuncSeparate(blend_factor(st.blend_rgb_src), blend_factor(st.blend_rgb_dst), blend_factor(st.blend_a_src), blend_factor(st.blend_a_dst)); }
	if(st.depth_mode == GR_DEPTHBUFFER_DISABLE) glDisable(GL_DEPTH_TEST);
	else { glEnable(GL_DEPTH_TEST); glDepthFunc(cmp_func(st.depth_func)); }
	glDepthMask(st.depth_mask ? GL_TRUE : GL_FALSE);
	if(textured && st.chroma_mode == GR_CHROMAKEY_ENABLE) { glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.0f); }
	else if(st.alpha_test != GR_CMP_ALWAYS) { glEnable(GL_ALPHA_TEST); glAlphaFunc(cmp_func(st.alpha_test), alpha_ref); }
	else glDisable(GL_ALPHA_TEST);
	if(fog_on())
	{
		GLfloat	c[4] = { ((fog_color >> 16) & 255) / 255.0f, ((fog_color >> 8) & 255) / 255.0f, (fog_color & 255) / 255.0f, 1.0f };
		glEnable(GL_FOG); glFogi(GL_FOG_MODE, GL_LINEAR); glFogf(GL_FOG_START, 0.0f); glFogf(GL_FOG_END, 1.0f);
		glFogi(GL_FOG_COORD_SRC, GL_FOG_COORD); glFogfv(GL_FOG_COLOR, c);
	}
	else glDisable(GL_FOG);
	if(!port_hires)
	{
		/* mark the pixels this draw writes with the dither mode it is drawn under */
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, st.dither == GR_DITHER_DISABLE ? 0 : 1, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	}
	glDisable(GL_CULL_FACE);
}

static int	vp_x, vp_y;		/* the letterboxed frame's place in the window and its scale */
static float	vp_s = 1.0f;

/* ---- Cabinet mode (the default; --hires turns it off) ----
   The Seattle board's Voodoo draws the game at 400x256 into a 16-bit RGB565 frame buffer and
   scans that out through the gamma RAM. The port draws each frame into an offscreen 400x256
   target, marks in the stencil buffer the pixels written while the game's dither mode is 4x4,
   then on the CPU turns the frame into RGB565 as the Voodoo writes it (the 4x4 ordered dither
   where marked, truncation elsewhere: MAME's voodoo tables), widens it back by bit repetition,
   runs it through the 33-entry gamma RAM with the hardware's interpolation, and shows it scaled
   with sharp pixels. On the cabinet dithering is on only until the first movie frame: Glide's
   window open sets 4x4, the movie's LFB write sets GR_DITHER_DISABLE and nothing sets it back.
   The one simplification: a blended pixel is reduced to 16 bits once per frame, not at each
   write, so overlapping translucent effects can differ by a step. With --hires the port draws
   straight to the window at its resolution in 24-bit colour, and the gamma ramp is applied
   with two blend passes. */
int		port_hires = 0;
static GLuint	cab_fbo, cab_color, cab_depth;
static int	cab_ready;
static void cabinet_target(void)
{
	if(!cab_ready)
	{
		glGenFramebuffersEXT(1, &cab_fbo);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, cab_fbo);
		glGenRenderbuffersEXT(1, &cab_color);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, cab_color);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, (int)hres, (int)vres);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, cab_color);
		glGenRenderbuffersEXT(1, &cab_depth);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, cab_depth);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT, (int)hres, (int)vres);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, cab_depth);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, cab_depth);
		if(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
		{
			fprintf(stderr, "[render] no offscreen target: falling back to --hires\n");
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			port_hires = 1;
			return;
		}
		cab_ready = 1;
	}
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, cab_fbo);
}

static void frame_setup(void)
{
	video_size(&win_w, &win_h);
	if(win_w <= 0) return;
	if(!port_hires) cabinet_target();
	if(!port_hires)
	{
		vp_x = vp_y = 0; vp_s = 1.0f;
		glViewport(0, 0, (int)hres, (int)vres);
	}
	else
	/* letterbox the 400x256 (or whatever grSstOpen chose) frame into the window */
	{
		float sx = win_w / hres, sy = win_h / vres, s = sx < sy ? sx : sy;
		int vw = (int)(hres * s), vh = (int)(vres * s);
		vp_x = (win_w - vw) / 2; vp_y = (win_h - vh) / 2; vp_s = s;
		glViewport(vp_x, vp_y, vw, vh);
	}
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	/* GOOSE opens the Voodoo with GR_ORIGIN_LOWER_LEFT: y grows upward from the bottom edge */
	if(origin_lower_left) glOrtho(0, hres, 0, vres, -1, 1); else glOrtho(0, hres, vres, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}

static void vertex_rgba(const MidVertex *v, port_texture_t *t, const float *rgb, const float *alpha)
{
	float oow = v->oow > 0 ? v->oow : 1e-6f;
	if(rgb || alpha)
	{
		unsigned char r = cur_rgb[0], g = cur_rgb[1], b = cur_rgb[2], a = cur_alpha;
		if(rgb && iterated_rgb())
		{
			r = rgb[0] < 0 ? 0 : rgb[0] > 255 ? 255 : (unsigned char)rgb[0];
			g = rgb[1] < 0 ? 0 : rgb[1] > 255 ? 255 : (unsigned char)rgb[1];
			b = rgb[2] < 0 ? 0 : rgb[2] > 255 ? 255 : (unsigned char)rgb[2];
			if(st.cc_invert && !(t && cur_tex_rgb)) { r = 255 - r; g = 255 - g; b = 255 - b; }
		}
		if(alpha && iterated_alpha()) a = *alpha < 0 ? 0 : *alpha > 255 ? 255 : (unsigned char)*alpha;
		glColor4ub(r, g, b, a);
	}
	if(t)
	{
		/* Glide texture coordinates run 0..256 on the texture's long axis */
		float smax = 256.0f, tmax = 256.0f;
		int ar = t->header.aspect_ratio;
		if(ar < 3) tmax = 256.0f / (float)(1 << (3 - ar)); else if(ar > 3) smax = 256.0f / (float)(1 << (ar - 3));
		glTexCoord4f(v->sow / smax, v->tow / tmax, 0.0f, oow);
	}
	/* w-buffer: nearer (larger oow) wins under GR_CMP_LESS. frame_setup's glOrtho(.., -1, 1)
	   negates z on the way to clip space, so 2 * oow - 1 lands in the window as 1 - oow (near
	   0, far 1), the same mapping the movie's depth plane is written with */
	if(fog_on()) glFogCoordf(fog_factor(v->oow));
	glVertex3f(v->x, v->y, 2.0f * (oow > 1.0f ? 1.0f : oow) - 1.0f);
}
static void vertex(const MidVertex *v, port_texture_t *t) { vertex_rgba(v, t, 0, 0); }

/* a triangle from the game's own vertex layouts: x, y, 1/w, then s/w, t/w when has_uv, with
   the iterated colour at float index rgb_at (-1: none) and the alpha at alpha_at (-1: none) */
void port_draw_tri(const float *a, const float *b, const float *c, int has_uv, int rgb_at, int alpha_at)
{
	port_texture_t *t;
	port_triangles++;
	if(!have_display) return;
	have_uv = has_uv;
	apply_state();
	have_uv = 1;
	t = (has_uv && st.tex != GR_NULL_MIPMAP_HANDLE && glIsEnabled(GL_TEXTURE_2D)) ? port_texture(st.tex) : 0;
	if(dump_every && getenv("CARNEVIL_TRACE") && !(hal_frame_count % dump_every))
		fprintf(stderr, "[tri] f%lu tex %d cc %d as %d col %08x blend %d/%d/%d/%d depth m%d f%d mask %d atest %d  q %.4f %.4f %.4f at (%.0f,%.0f) (%.0f,%.0f) (%.0f,%.0f)\n", hal_frame_count, (int)st.tex, st.color_combine, st.alpha_source, st.const_color, st.blend_rgb_src, st.blend_rgb_dst, st.blend_a_src, st.blend_a_dst,
			st.depth_mode, st.depth_func, st.depth_mask, st.alpha_test, a[2], b[2], c[2], a[0], a[1], b[0], b[1], c[0], c[1]);
	glBegin(GL_TRIANGLES);
	vertex_rgba((const MidVertex *)a, t, rgb_at >= 0 ? a + rgb_at : 0, alpha_at >= 0 ? a + alpha_at : 0);
	vertex_rgba((const MidVertex *)b, t, rgb_at >= 0 ? b + rgb_at : 0, alpha_at >= 0 ? b + alpha_at : 0);
	vertex_rgba((const MidVertex *)c, t, rgb_at >= 0 ? c + rgb_at : 0, alpha_at >= 0 ? c + alpha_at : 0);
	glEnd();
}

/* ---- Glide entry points ---- */
void grDrawTriangleDma(MidVertex *a, MidVertex *b, MidVertex *c, int cull)
{
	port_texture_t *t;
	(void)cull;
	port_triangles++;
	if(!have_display) return;
	apply_state();
	t = (st.tex != GR_NULL_MIPMAP_HANDLE) ? port_texture(st.tex) : 0;
	if(dump_every && getenv("CARNEVIL_TRACE") && !(hal_frame_count % dump_every))
		fprintf(stderr, "[tri] f%lu tex %d(%s) cc %d as %d col %08x blend %d/%d/%d/%d depth m%d f%d mask %d atest %d ref %.2f chroma %d  (%.1f,%.1f,%.3f s%.1f t%.1f) (%.1f,%.1f) (%.1f,%.1f)\n",
			hal_frame_count, (int)st.tex, t ? t->name : "-", st.color_combine, st.alpha_source, st.const_color, st.blend_rgb_src, st.blend_rgb_dst, st.blend_a_src, st.blend_a_dst,
			st.depth_mode, st.depth_func, st.depth_mask, st.alpha_test, alpha_ref, st.chroma_mode, a->x, a->y, a->oow, a->sow, a->tow, b->x, b->y, c->x, c->y);
	glBegin(GL_TRIANGLES);
	vertex(a, t); vertex(b, t); vertex(c, t);
	glEnd();
}
void grDrawTriangle(const GrVertex *a, const GrVertex *b, const GrVertex *c) { grDrawTriangleDma((MidVertex *)a, (MidVertex *)b, (MidVertex *)c, 0); }
void grDrawQuadDma(MidVertex *a, MidVertex *b, MidVertex *c, MidVertex *d, int cull)
{
	grDrawTriangleDma(a, b, c, cull); grDrawTriangleDma(a, c, d, cull);
}

void grBufferClear(GrColor_t color, GrAlpha_t alpha, FxU16 depth)
{
	(void)alpha; (void)depth;
	if(!have_display) return;
	frame_setup();
	glClearColor(((color >> 16) & 255) / 255.0f, ((color >> 8) & 255) / 255.0f, (color & 255) / 255.0f, 1.0f);
	glDepthMask(GL_TRUE);
	glClearStencil(st.dither == GR_DITHER_DISABLE ? 0 : 1);	/* the fast fill dithers like a triangle */
	glStencilMask(~0u);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}
void grDitherMode(int mode) { st.dither = mode; }
/* the LFB writes (the movie, the autograph board) run under GR_DITHER_DISABLE: the original's
   blocks call grDitherMode(GR_DITHER_DISABLE) before writing, which the port's paths replace */
static void lfb_write_state(void)
{
	st.dither = GR_DITHER_DISABLE;
	if(!port_hires) { glEnable(GL_STENCIL_TEST); glStencilFunc(GL_ALWAYS, 0, 1); glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE); }
}

static float brightness = 1.0f;
static const char *dump_path;
static long	cab_dithered;			/* pixels of the last frame drawn under the 4x4 dither ([dump] trace) */
static void dump_frame(void)
{
	unsigned char *px; FILE *f; int y; char name[1024];
	if(!dump_path || !have_display) return;
	if(dump_every)
	{
		/* dump the first swap at or after every multiple of dump_every */
		static unsigned long last_bucket;
		if(hal_frame_count / dump_every == last_bucket) return;
		last_bucket = hal_frame_count / dump_every;
	}
	if(getenv("CARNEVIL_TRACE")) fprintf(stderr, "[dump] frame %lu brightness %.2f triangles %lu tex %d dithered %ld\n", hal_frame_count, brightness, port_triangles, (int)st.tex, port_hires ? -1L : cab_dithered);
	px = malloc(win_w * win_h * 3);
	glReadPixels(0, 0, win_w, win_h, GL_RGB, GL_UNSIGNED_BYTE, px);
	if(dump_every) snprintf(name, sizeof name, "%s.%06lu.ppm", dump_path, hal_frame_count); else snprintf(name, sizeof name, "%s", dump_path);
	if((f = fopen(name, "wb")))
	{
		fprintf(f, "P6\n%d %d\n255\n", win_w, win_h);
		for(y = win_h - 1; y >= 0; y--) fwrite(px + y * win_w * 3, 1, win_w * 3, f);
		fclose(f);
	}
	free(px);
}

/* The Voodoo's gamma RAM (the video CLUT): GOOSE's fader (gfader.c, the Seattle build) writes
   33 entries, entry i being the output for input 8 * i, and the hardware interpolates between
   neighbours for the 8-bit inputs in between, the same on R, G and B. Every ramp the game loads
   is a straight line (fade: 0 .. intensity; specific_gamma_load(from, to): from .. to, used by the
   movie scripts and the gun's flash), so the output is from + (to - from) * input / 256: the port
   applies it to the finished frame as a multiply and an add. Until 2026-10-14 the port listened
   for entry 255, which the 33-entry table never writes, so no fade or flash showed. */
#define GAMMA_TABLE_SIZE	33
static int	gamma_ram[GAMMA_TABLE_SIZE];
static int	gamma_loaded;
void port_gamma_entry(int i, int g)
{
	if(i < 0 || i >= GAMMA_TABLE_SIZE) return;
	gamma_ram[i] = g < 0 ? 0 : g > 255 ? 255 : g;
	gamma_loaded = 1;
	brightness = gamma_ram[GAMMA_TABLE_SIZE - 1] / 255.0f;	/* for the [dump] trace */
}
static void fullscreen_quad(void)
{
	glBegin(GL_QUADS); glVertex3f(0, 0, 0); glVertex3f(hres, 0, 0); glVertex3f(hres, vres, 0); glVertex3f(0, vres, 0); glEnd();
}
static void apply_brightness(void)
{
	float	add, mul;

	if(!gamma_loaded) return;
	add = gamma_ram[0] / 255.0f;
	mul = (gamma_ram[GAMMA_TABLE_SIZE - 1] - gamma_ram[0]) / 255.0f * (255.0f / 256.0f);
	if(add == 0.0f && gamma_ram[GAMMA_TABLE_SIZE - 1] == 255) return;	/* the normal ramp: off by under one step */
	if(mul < 0.0f) mul = 0.0f;
	glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST); glDisable(GL_ALPHA_TEST); glDisable(GL_FOG);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ZERO, GL_SRC_COLOR);			/* frame * mul */
	glColor4f(mul, mul, mul, 1.0f);
	fullscreen_quad();
	if(add > 0.0f)
	{
		glBlendFunc(GL_ONE, GL_ONE);			/* + from */
		glColor4f(add, add, add, 1.0f);
		fullscreen_quad();
	}
}

/* ---- the movie frame: 400x256 LFB words written with grLfbWriteMode(13) = RGB555 + depth;
   row 0 is the bottom scan line. The high half of each word is the Voodoo's 16-bit w-buffer
   value (a 4-bit exponent and a 12-bit mantissa of the distance, see depth_word_to_float in
   scene.c); the hardware writes it into the depth buffer with the pixel, which is what hides
   an enemy behind the scenery. The port writes the same distances as GL depth, in the mapping
   vertex_rgba uses for the triangles' 1/w ---- */
static GLuint	movie_tex;
static float	depth_of_word[65536];
static int	depth_table_built;
/* The Voodoo never decodes the depth words: under GR_CMP_LEQUAL it compares the stored word
   with the word it computes from the pixel's iterated 1/w, held as 16.32 fixed point (MAME's
   voodoo emulation): 0 when 1/w >= 1, 0xffff when 1/w < 2^-16, else with e = the leading zeros
   of the low 32 bits t, word = ((e << 12) | (~t >> (19 - e) & 0xfff)) + 1. Within an exponent
   that is linear in 1/w, not in the distance. A pixel therefore passes against word W exactly
   when its 1/w is at least the smallest 1/w that encodes to W, 2^-(e+1) * (1 + (4095 - m) / 4096)
   for W - 1 = e << 12 | m, and the table stores 1 minus that bound, so GL's LEQUAL on
   1 - 1/w (vertex_rgba) gives the hardware's answer. The game's own depth_word_to_float
   (scene.c), used for the gun's shot test, reads the word as the distance 2^e * (1 + m / 4096);
   that is a linear approximation of the same curve, and the port took it for the depth buffer
   until 2026-10-14, which put the scenery up to 12% farther than the hardware saw it: enemies
   near scenery, and feet in the floor, showed through (about 7% of the vertices the cabinet
   hides, measured over a Rickety Town session). */
static void build_depth_table(void)
{
	int	i;
	depth_of_word[0] = 0.0f;			/* 1/w >= 1: only an equally near pixel passes */
	for(i = 1; i < 0xffff; i++)
	{
		int	e = (i - 1) >> 12, m = (i - 1) & 0xfff;
		double	oow_min = ldexp(1.0 + (double)(4095 - m) / 4096.0, -(e + 1));
		depth_of_word[i] = (float)(1.0 - (oow_min > 1.0 ? 1.0 : oow_min));
	}
	depth_of_word[0xffff] = 1.0f;			/* the farthest word: every pixel passes */
	depth_table_built = 1;
}
void port_movie_blit(const unsigned int *buf, int scroll)
{
	static unsigned short	rgb[400 * 256];
	static float		depth[400 * 256];
	int	i;
	float	y0, y1;
	if(!have_display) return;
	frame_setup();
	if(!depth_table_built) build_depth_table();
	for(i = 0; i < 400 * 256; i++) { rgb[i] = (unsigned short)buf[i]; depth[i] = depth_of_word[buf[i] >> 16]; }
	if(!movie_tex) glGenTextures(1, &movie_tex);
	glBindTexture(GL_TEXTURE_2D, movie_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 400, 256, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, rgb);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glClearColor(0, 0, 0, 1); glDepthMask(GL_TRUE); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_TEXTURE_2D); glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST); glDisable(GL_FOG);	/* LFB writes bypass the pixel pipeline */
	lfb_write_state();
	glColor4f(1, 1, 1, 1);
	/* row 0 of the frame is the bottom line; a positive scroll inserts blank lines below it */
	y0 = (float)scroll;		/* bottom edge */
	y1 = y0 + (float)256;		/* top edge */
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex3f(0, y0, 0);
	glTexCoord2f(1, 0); glVertex3f((float)400, y0, 0);
	glTexCoord2f(1, 1); glVertex3f((float)400, y1, 0);
	glTexCoord2f(0, 1); glVertex3f(0, y1, 0);
	glEnd();
	/* the movie's depth plane, scaled like the picture (row 0 at the bottom) */
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glEnable(GL_DEPTH_TEST); glDepthFunc(GL_ALWAYS); glDepthMask(GL_TRUE);
	if(origin_lower_left) { glPixelZoom(vp_s, vp_s); glWindowPos2f((float)vp_x, (float)vp_y + y0 * vp_s); }
	else { glPixelZoom(vp_s, -vp_s); glWindowPos2f((float)vp_x, (float)vp_y + (vres - y0) * vp_s); }
	glDrawPixels(400, 256, GL_DEPTH_COMPONENT, GL_FLOAT, depth);
	glPixelZoom(1.0f, 1.0f);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
}

/* A frame-buffer write in GR_LFBWRITEMODE_555_DEPTH with the pixel pipeline bypassed: w x h
   words (colour in the low half, depth in the high half, row 0 at the bottom with the lower-left
   origin) stored at x, y. The high-score signing board writes its ink this way
   (draw_autograph_board, hiscore.c). Pixels are written as they are, unfiltered. */
/* An LFB write in GR_LFBWRITEMODE_565 with GR_ORIGIN_UPPER_LEFT and the pipeline bypassed
   (the end credits, endcredits.c): w x h RGB565 pixels, top row first, over the whole frame
   from its top-left corner. Colour only: the 565 write mode carries no depth. */
void port_lfb_write565(const unsigned short *rows, int w, int h)
{
	static GLuint	tex;

	if(!have_display || w <= 0 || h <= 0) return;
	frame_setup();
	if(!tex) glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, rows);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glEnable(GL_TEXTURE_2D); glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST); glDisable(GL_FOG);	/* LFB writes bypass the pixel pipeline */
	if(!port_hires) { glEnable(GL_STENCIL_TEST); glStencilFunc(GL_ALWAYS, 0, 1); glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE); }	/* not dithered */
	glColor4f(1, 1, 1, 1);
	glBegin(GL_QUADS);
	if(origin_lower_left)
	{
		glTexCoord2f(0, 1); glVertex3f(0, vres - h, 0);
		glTexCoord2f(1, 1); glVertex3f((float)w, vres - h, 0);
		glTexCoord2f(1, 0); glVertex3f((float)w, vres, 0);
		glTexCoord2f(0, 0); glVertex3f(0, vres, 0);
	}
	else
	{
		glTexCoord2f(0, 0); glVertex3f(0, 0, 0);
		glTexCoord2f(1, 0); glVertex3f((float)w, 0, 0);
		glTexCoord2f(1, 1); glVertex3f((float)w, (float)h, 0);
		glTexCoord2f(0, 1); glVertex3f(0, (float)h, 0);
	}
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void port_lfb_write(const unsigned int *src, int x, int y, int w, int h)
{
	static GLuint		lfb_tex;
	unsigned short		*rgb;
	float			*depth;
	int			i;

	if(!have_display || w <= 0 || h <= 0) return;
	frame_setup();
	if(!depth_table_built) build_depth_table();
	rgb = malloc(w * h * sizeof *rgb);
	depth = malloc(w * h * sizeof *depth);
	if(!rgb || !depth) { free(rgb); free(depth); return; }
	for(i = 0; i < w * h; i++) { rgb[i] = (unsigned short)src[i]; depth[i] = depth_of_word[src[i] >> 16]; }
	if(!lfb_tex) glGenTextures(1, &lfb_tex);
	glBindTexture(GL_TEXTURE_2D, lfb_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, rgb);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glEnable(GL_TEXTURE_2D); glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST); glDisable(GL_FOG);	/* LFB writes bypass the pixel pipeline */
	lfb_write_state();
	glColor4f(1, 1, 1, 1);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex3f((float)x, (float)y, 0);
	glTexCoord2f(1, 0); glVertex3f((float)(x + w), (float)y, 0);
	glTexCoord2f(1, 1); glVertex3f((float)(x + w), (float)(y + h), 0);
	glTexCoord2f(0, 1); glVertex3f((float)x, (float)(y + h), 0);
	glEnd();
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glEnable(GL_DEPTH_TEST); glDepthFunc(GL_ALWAYS); glDepthMask(GL_TRUE);
	if(origin_lower_left) { glPixelZoom(vp_s, vp_s); glWindowPos2f((float)vp_x + x * vp_s, (float)vp_y + y * vp_s); }
	else { glPixelZoom(vp_s, -vp_s); glWindowPos2f((float)vp_x + x * vp_s, (float)vp_y + (vres - y) * vp_s); }
	glDrawPixels(w, h, GL_DEPTH_COMPONENT, GL_FLOAT, depth);
	glPixelZoom(1.0f, 1.0f);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	free(rgb); free(depth);
}

/* Cabinet mode's scan-out: the offscreen 400x256 frame -> RGB565 as the Voodoo writes it ->
   back to 8 bits per channel -> the gamma RAM -> the window, letterboxed with sharp pixels */
static const unsigned char dither4x4[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
static unsigned char gamma_out(int v)
{
	int	i = v >> 3, f = v & 7;
	int	a = gamma_loaded ? gamma_ram[i] : (i << 3);
	int	b = gamma_loaded ? gamma_ram[i + 1] : ((i + 1) << 3 > 255 ? 255 : (i + 1) << 3);
	if(!gamma_loaded) return (unsigned char)v;
	return (unsigned char)(a + (((b - a) * f) >> 3));
}
static void cabinet_present(void)
{
	static unsigned char	rgba[400 * 256 * 4], sten[400 * 256], out[400 * 256 * 3];
	static GLuint		tex;
	unsigned char		lut[256];
	int			w = (int)hres, h = (int)vres, x, y, i;

	if(w > 400 || h > 256) return;
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, cab_fbo);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	glReadPixels(0, 0, w, h, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, sten);
	for(i = 0; i < 256; i++) lut[i] = gamma_out(i);
	cab_dithered = 0;
	for(y = 0; y < h; y++)
		for(x = 0; x < w; x++)
		{
			const unsigned char	*c = rgba + 4 * (y * w + x);
			unsigned char		*o = out + 3 * (y * w + x);
			int			r5, g6, b5;
			if(sten[y * w + x] & 1)
			{
				/* MAME's voodoo dither tables (4x4 ordered) */
				int d = dither4x4[((y & 3) << 2) | (x & 3)];
				cab_dithered++;
				r5 = ((c[0] << 1) - (c[0] >> 4) + (c[0] >> 7) + d) >> 4;
				g6 = ((c[1] << 2) - (c[1] >> 4) + (c[1] >> 6) + d) >> 4;
				b5 = ((c[2] << 1) - (c[2] >> 4) + (c[2] >> 7) + d) >> 4;
			}
			else { r5 = c[0] >> 3; g6 = c[1] >> 2; b5 = c[2] >> 3; }
			o[0] = lut[(r5 << 3) | (r5 >> 2)];
			o[1] = lut[(g6 << 2) | (g6 >> 4)];
			o[2] = lut[(b5 << 3) | (b5 >> 2)];
		}
	/* show it: the window, letterboxed, nearest-neighbour */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	video_size(&win_w, &win_h);
	{
		float sx = win_w / hres, sy = win_h / vres, sc = sx < sy ? sx : sy;
		int vw = (int)(hres * sc), vh = (int)(vres * sc);
		glViewport(0, 0, win_w, win_h);
		glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
		glViewport((win_w - vw) / 2, (win_h - vh) / 2, vw, vh);
	}
	if(!tex) glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, out);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST); glDisable(GL_FOG); glDisable(GL_STENCIL_TEST);
	glEnable(GL_TEXTURE_2D); glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f(1, 1, 1, 1);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex2f(0, 0); glTexCoord2f(1, 0); glVertex2f(1, 0);
	glTexCoord2f(1, 1); glVertex2f(1, 1); glTexCoord2f(0, 1); glVertex2f(0, 1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 4); glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}
/* A gamepad-aimed player's crosshair (a light gun has none; port-only, drawn over the finished
   picture in the window so it stays sharp in cabinet mode): player 1 red, player 2 blue */
static void draw_crosshairs(void)
{
	static const float	col[2][3] = { { 1.0f, 0.25f, 0.25f }, { 0.3f, 0.6f, 1.0f } };
	float			x, y, sx, sy, sc;
	int			i, vw, vh;

	video_size(&win_w, &win_h);
	sx = win_w / hres; sy = win_h / vres; sc = sx < sy ? sx : sy;
	vw = (int)(hres * sc); vh = (int)(vres * sc);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glViewport((win_w - vw) / 2, (win_h - vh) / 2, vw, vh);
	glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, hres, 0, vres, -1, 1);
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST); glDisable(GL_ALPHA_TEST); glDisable(GL_FOG); glDisable(GL_STENCIL_TEST);
	glDisable(GL_BLEND);
	glLineWidth(sc > 3.0f ? sc * 0.7f : 2.0f);	/* about a game pixel thick at any window size */
	for(i = 0; i < 2; i++)
	{
		if(!hal_crosshair(i, &x, &y)) continue;
		glColor3f(col[i][0], col[i][1], col[i][2]);
		glBegin(GL_LINES);
		glVertex2f(x - 9, y); glVertex2f(x - 3, y); glVertex2f(x + 3, y); glVertex2f(x + 9, y);
		glVertex2f(x, y - 9); glVertex2f(x, y - 3); glVertex2f(x, y + 3); glVertex2f(x, y + 9);
		glEnd();
		glBegin(GL_LINE_LOOP);
		{
			int k;
			for(k = 0; k < 16; k++) glVertex2f(x + 6.0f * cosf(k * 0.3927f), y + 6.0f * sinf(k * 0.3927f));
		}
		glEnd();
	}
	glLineWidth(1.0f);
}
void grBufferSwap(int interval)
{
	(void)interval;
	if(!have_display) return;
	if(port_hires) apply_brightness();
	else cabinet_present();
	draw_crosshairs();
	dump_frame();
	video_present();
	if(video_closed()) exit(0);
}
void dma_start(void) { grBufferSwap(1); }			/* the DMA kick; the board swaps when it completes */

void guTexSource(GrMipMapId_t h) { st.tex = h; }
void grTexCombineFunction(int tmu, int fn) { (void)tmu; st.tex_combine = fn; }
void guColorCombineFunction(int fn) { st.color_combine = fn; st.cc_invert = 0; }
void guAlphaSource(int s) { st.alpha_source = s; }
void grConstantColorValue(unsigned int c) { st.const_color = c; }
void grChromakeyValue(unsigned int c) { st.chroma_value = c & 0xffffff; }
void grChromaKeyValue(unsigned int c) { grChromakeyValue(c); }
void grChromakeyMode(int m) { st.chroma_mode = m; }
void grTexFilterMode(int tmu, int min, int mag) { (void)tmu; st.filter_min = min; st.filter_mag = mag; }
void grTexClampMode(int tmu, int s, int t) { (void)tmu; st.clamp_s = s; st.clamp_t = t; }
void grAlphaBlendFunction(int rs, int rd, int as, int ad) { st.blend_rgb_src = rs; st.blend_rgb_dst = rd; st.blend_a_src = as; st.blend_a_dst = ad; }
void grDepthBufferFunction(int f) { st.depth_func = f; }
void grDepthBufferMode(int m) { st.depth_mode = m; }
void grDepthMask(int m) { st.depth_mask = m; }
void grDepthBiasLevel(FxI16 l) { (void)l; }
void grAlphaTestFunction(int f) { st.alpha_test = f; }
void grAlphaTestReferenceValue(FxU8 v) { alpha_ref = v / 255.0f; }
void grCullMode(int m) { st.cull = m; }
void grLoadGammaTable(unsigned int n, unsigned int *r, unsigned int *g, unsigned int *b) { (void)n; (void)r; (void)g; (void)b; }
/* grColorCombine(function, factor, local, other, invert): the tuple Glide's gu helpers are
   built from, mapped back to the mode the state keeps (plus the invert flag) */
void grColorCombine(int function, int factor, int local, int other, int invert)
{
	int mode = GR_COLORCOMBINE_DECAL_TEXTURE;
	switch(function)
	{
	case GR_COMBINE_FUNCTION_ZERO: mode = invert ? GR_COLORCOMBINE_ONE : GR_COLORCOMBINE_ZERO; invert = 0; break;
	case GR_COMBINE_FUNCTION_LOCAL: mode = local == GR_COMBINE_LOCAL_ITERATED ? GR_COLORCOMBINE_ITRGB : GR_COLORCOMBINE_CCRGB; break;
	case GR_COMBINE_FUNCTION_SCALE_OTHER:
		if(other == GR_COMBINE_OTHER_TEXTURE)
		{
			if(factor == GR_COMBINE_FACTOR_ONE) mode = GR_COLORCOMBINE_DECAL_TEXTURE;
			else if(factor == GR_COMBINE_FACTOR_LOCAL) mode = local == GR_COMBINE_LOCAL_ITERATED ? GR_COLORCOMBINE_TEXTURE_TIMES_ITRGB : GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB;
			else if(factor == GR_COMBINE_FACTOR_LOCAL_ALPHA) mode = GR_COLORCOMBINE_TEXTURE_TIMES_ALPHA;
		}
		else mode = local == GR_COMBINE_LOCAL_ITERATED ? GR_COLORCOMBINE_ITRGB : GR_COLORCOMBINE_CCRGB;
		break;
	case GR_COMBINE_FUNCTION_SCALE_OTHER_ADD_LOCAL:
		mode = other == GR_COMBINE_OTHER_TEXTURE ? GR_COLORCOMBINE_TEXTURE_ADD_ITRGB : GR_COLORCOMBINE_ITRGB; break;
	case GR_COMBINE_FUNCTION_SCALE_OTHER_MINUS_LOCAL:
		mode = other == GR_COMBINE_OTHER_TEXTURE ? GR_COLORCOMBINE_TEXTURE_SUB_ITRGB : GR_COLORCOMBINE_ITRGB; break;
	default:
		{ static int warned; if(!warned++) fprintf(stderr, "[gl] grColorCombine(%d, %d, %d, %d, %d) not mapped\n", function, factor, local, other, invert); }
		break;
	}
	st.color_combine = mode;
	st.cc_invert = invert != 0;
}

/* ---- init: the game asks for 400x256 (grSstVidMode timing), graphics_init() sets hres/vres ---- */
void grSstVidMode(int which, void *timing) { (void)which; (void)timing; }
void grGlideInit(void) { glide_defaults(); }
FxBool grSstQueryHardware(GrHwConfiguration *hw) { (void)hw; return 1; }
void grSstSelect(int n) { (void)n; }
int  grSstOpen(int res, int refresh, int fmt, int origin, int smooth, int nbuf)
{
	(void)res; (void)refresh; (void)fmt; (void)smooth; (void)nbuf;
	origin_lower_left = origin == GR_ORIGIN_LOWER_LEFT;
	if(!hal_headless) have_display = video_open(800, 512, "CarnEvil");
	if(!have_display) fprintf(stderr, "[render] no display: running headless\n");
	return 1;
}
int  func_80157004(void) { return hal_refresh_hz(); }	/* Glide: current vertical refresh */
/* Glide grSstIsBusy (0x80162570): bit 9 of the SST-1 status register. The port draws each
   triangle as the game issues it and keeps no command FIFO, so the chip is never still busy
   when the game asks (the light gun's flash waits for an idle chip, shoot.c) */
int  grSstIsBusy(void) { return 0; }
void render_set_dump(const char *path, unsigned long every) { dump_path = path; dump_every = every; }


/* a window position (backing pixels, origin bottom-left) in game-screen units; 0 when outside the picture */
int video_to_game(float wx, float wy, float *gx, float *gy)
{
	float sx = win_w / hres, sy = win_h / vres, s = sx < sy ? sx : sy;
	int vw = (int)(hres * s), vh = (int)(vres * s), vx = (win_w - vw) / 2, vy = (win_h - vh) / 2;
	*gx = (wx - vx) / s; *gy = (wy - vy) / s;
	if(!origin_lower_left) *gy = vres - *gy;
	return *gx >= 0 && *gx <= hres && *gy >= 0 && *gy <= vres;
}
