/*
 * draw2d.c - CarnEvil's 2D helpers: screen-space quads built from MidVertex and handed to the
 * game's triangle setup (draw_tri, 0x800d1750, which writes Voodoo commands into the DMA
 * list). draw_quad is 0x800d90c8.
 */
#include <glide/glide.h>

extern void	guAlphaSource(int);
extern void	draw_tri(MidVertex *, MidVertex *, MidVertex *);

/* centred at (x, y), w by h pixels, texture coordinates 0..u, 0..v (top row at v); w = 1.1 */
void draw_quad(float x, float y, float w, float h, float u, float v)
{
	MidVertex	a, b, c, d;
	float		oow;

	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	w *= 0.5f;
	h *= 0.5f;
	oow = 0.90909088f;
	v *= oow;
	u *= oow;
	a.oow = oow;
	b.oow = oow;
	c.oow = oow;
	d.oow = oow;
	a.x = x - w;
	a.y = y - h;
	a.sow = 0.0f;
	a.tow = v;
	b.x = x + w;
	b.y = y - h;
	b.sow = u;
	b.tow = v;
	c.x = x - w;
	c.y = y + h;
	c.sow = 0.0f;
	c.tow = 0.0f;
	d.x = x + w;
	d.y = y + h;
	d.sow = u;
	d.tow = 0.0f;
	draw_tri(&a, &b, &d);
	draw_tri(&a, &d, &c);
}
