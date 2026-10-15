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
#else
#include <GL/gl.h>
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
}

/* ---- texture upload: Glide texel formats -> RGBA8 ---- */
static void texel_rgba(int fmt, const unsigned char *p, const GuTexTable *tbl, unsigned char *o)
{
	unsigned int v;
	switch(fmt)
	{
	case GR_TEXFMT_RGB_565: v = p[0] | (p[1] << 8);
		o[0] = (v >> 11) * 255 / 31; o[1] = ((v >> 5) & 63) * 255 / 63; o[2] = (v & 31) * 255 / 31; o[3] = 255; break;
	case GR_TEXFMT_ARGB_1555: v = p[0] | (p[1] << 8);
		o[0] = ((v >> 10) & 31) * 255 / 31; o[1] = ((v >> 5) & 31) * 255 / 31; o[2] = (v & 31) * 255 / 31; o[3] = (v & 0x8000) ? 255 : 0; break;
	case GR_TEXFMT_ARGB_4444: v = p[0] | (p[1] << 8);
		o[0] = ((v >> 8) & 15) * 17; o[1] = ((v >> 4) & 15) * 17; o[2] = (v & 15) * 17; o[3] = (v >> 12) * 17; break;
	case GR_TEXFMT_ALPHA_INTENSITY_88: o[0] = o[1] = o[2] = p[0]; o[3] = p[1]; break;
	case GR_TEXFMT_ARGB_8332: v = p[0] | (p[1] << 8);
		o[0] = ((v >> 5) & 7) * 255 / 7; o[1] = ((v >> 2) & 7) * 255 / 7; o[2] = (v & 3) * 85; o[3] = v >> 8; break;
	case GR_TEXFMT_RGB_332: v = p[0];
		o[0] = (v >> 5) * 255 / 7; o[1] = ((v >> 2) & 7) * 255 / 7; o[2] = (v & 3) * 85; o[3] = 255; break;
	case GR_TEXFMT_ALPHA_8: o[0] = o[1] = o[2] = 255; o[3] = p[0]; break;
	case GR_TEXFMT_INTENSITY_8: o[0] = o[1] = o[2] = p[0]; o[3] = 255; break;
	case GR_TEXFMT_ALPHA_INTENSITY_44: o[0] = o[1] = o[2] = (p[0] & 15) * 17; o[3] = (p[0] >> 4) * 17; break;
	case GR_TEXFMT_P_8: v = tbl ? tbl->palette.data[p[0]] : 0x808080;
		o[0] = (v >> 16) & 255; o[1] = (v >> 8) & 255; o[2] = v & 255; o[3] = 255; break;
	case GR_TEXFMT_AP_88: v = tbl ? tbl->palette.data[p[0]] : 0x808080;
		o[0] = (v >> 16) & 255; o[1] = (v >> 8) & 255; o[2] = v & 255; o[3] = p[1]; break;
	default: o[0] = 255; o[1] = 0; o[2] = 255; o[3] = 255; break;	/* YIQ/NCC: not decoded yet */
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
	glDisable(GL_CULL_FACE);
}

static int	vp_x, vp_y;		/* the letterboxed frame's place in the window and its scale */
static float	vp_s = 1.0f;
static void frame_setup(void)
{
	video_size(&win_w, &win_h);
	if(win_w <= 0) return;
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
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static float brightness = 1.0f;
static const char *dump_path;
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
	if(getenv("CARNEVIL_TRACE")) fprintf(stderr, "[dump] frame %lu brightness %.2f triangles %lu tex %d\n", hal_frame_count, brightness, port_triangles, (int)st.tex);
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

/* gamma RAM: the fader loads a ramp whose top entry is the screen brightness */
void port_gamma_entry(int i, int g) { if(i == 255) brightness = g / 255.0f; }
static void apply_brightness(void)
{
	if(brightness >= 1.0f) return;
	glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST); glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.0f, 0.0f, 0.0f, 1.0f - brightness);
	glBegin(GL_QUADS); glVertex3f(0, 0, 0); glVertex3f(hres, 0, 0); glVertex3f(hres, vres, 0); glVertex3f(0, vres, 0); glEnd();
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
	glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST);
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
	glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST);
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

void grBufferSwap(int interval) { (void)interval; if(have_display) { apply_brightness(); dump_frame(); video_present(); if(video_closed()) exit(0); } }
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

/* fog: the level fog table is not applied yet (the draw pass sets it every frame) */
void grFogMode(int mode) { (void)mode; }
void grFogColorValue(unsigned int c) { (void)c; }
void grFogTable(const unsigned char *t) { (void)t; }

/* a window position (backing pixels, origin bottom-left) in game-screen units; 0 when outside the picture */
int video_to_game(float wx, float wy, float *gx, float *gy)
{
	float sx = win_w / hres, sy = win_h / vres, s = sx < sy ? sx : sy;
	int vw = (int)(hres * s), vh = (int)(vres * s), vx = (win_w - vw) / 2, vy = (win_h - vh) / 2;
	*gx = (wx - vx) / s; *gy = (wy - vy) / s;
	if(!origin_lower_left) *gy = vres - *gy;
	return *gx >= 0 && *gx <= hres && *gy >= 0 && *gy <= vres;
}
