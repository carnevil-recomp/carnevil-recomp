/*
 * lights.c - the level's lights (0x800d6ef0-0x800d8xxx: the lights file loader, the debug
 * light display and the vertex lighting). Compiled without optimisation like the original.
 * Only the vertex lighting is in C so far.
 */

typedef struct light
{
	float		dir[3];
	float		color[3];		/* +0x0c: 0..255 per channel */
	struct light	*next;			/* +0x18 */
} light_t;

int	light_flags;				/* 0x8017c1d8 (.data): bit 0 = everything fully lit */
float	ambient_override[3] = { 255.0f, 255.0f, 255.0f };	/* 0x8017c1e8 */
light_t	*light_list;				/* 0x8017c2a8 */

/* 0x800d7dfc: the colour of n lit normals: a base of 5, plus each light's colour by the
   normal's cosine to it (negative contributions dropped), clamped to 254 */
void light_vertices(int n, float *normals, float *colors)
{
	float	scale = 0.00390625f;
	light_t	*l;
	float	*nm, *c;
	float	dot, v;
	int	i;

	c = colors;
	if(light_flags & 1)
	{
		for(i = 0; i < n; i++)
		{
			c[0] = ambient_override[0];
			c[1] = ambient_override[1];
			c[2] = ambient_override[2];
			c += 3;
		}
		return;
	}
	for(i = 0; i < n; i++)
	{
		c[0] = 5.0f;
		c[1] = 5.0f;
		c[2] = 5.0f;
		c += 3;
	}
	l = light_list;
	while(l)
	{
		nm = normals;
		c = colors;
		for(i = 0; i < n; i++)
		{
			dot = nm[0] * l->dir[0] + nm[1] * l->dir[1] + nm[2] * l->dir[2];
			v = l->color[0] * dot * scale;
			if(v > 0.0f) c[0] = c[0] + v;
			v = l->color[1] * dot * scale;
			if(v > 0.0f) c[1] = c[1] + v;
			v = l->color[2] * dot * scale;
			if(v > 0.0f) c[2] = c[2] + v;
			nm += 3;
			c += 3;
		}
		l = l->next;
	}
	c = colors;
	for(i = 0; i < n; i++)
	{
		if(c[0] > 254.0f) c[0] = 254.0f;
		if(c[1] > 254.0f) c[1] = 254.0f;
		if(c[2] > 254.0f) c[2] = 254.0f;
		c += 3;
	}
}
