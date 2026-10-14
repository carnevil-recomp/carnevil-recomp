/*
 * lights.c - the level's lights (0x800d6ef0-0x800d8c60, compiled without optimisation like
 * the original). See lights.h. The draw pass calls update_lights() with the movie frame; each
 * model's draw calls build_light_list() with its hit group's position and light_vertices()
 * per submesh. Point lights fall off with the square of the distance over ten times their
 * table range; directional lights are stored as a direction vector scaled by their strength.
 */
#include <stdio.h>
#include <stdlib.h>
#include "lights.h"
#include "waves.h"
#include "camera.h"			/* camera_x/y/z */

extern void	rot_matrix_yxz(float *, float *);
extern void	mat_vec_mul(float *, float *, float *);
extern void	vec_max_normalize(float *);			/* objects.c */
extern float	fsqrt(float);

level_light_t	light_root = { 0, 1, 0, { 0, 0, 0 }, { 255.0f, 255.0f, 255.0f } };	/* 0x8017c1d0 */
level_light_t	light_root2 = { 0, 1, 0, { 0, 0, 0 }, { 255.0f, 255.0f, 255.0f } };	/* 0x8017c218: unused */
level_light_t	light_root3 = { 0, 1, 0, { 0, 0, 0 }, { 255.0f, 255.0f, 255.0f } };	/* 0x8017c260: unused */
light_t		*light_list = 0;		/* 0x8017c2a8 */
int		next_id = 0x400;		/* 0x8017c2ac */
light_info_t	*level_light_info_global;	/* 0x8021f084 */
int		num_light_groups;		/* 0x8021f0d4 */
int		light_count;			/* 0x801e5cb8 */
level_light_t	*camera_light;			/* 0x801e5cc8 */
int		light_wave;			/* 0x801e6d78: the wave id or'ed into the group ids */
int		light_group;			/* 0x801e0a00 */
int		group_flags[MAX_LIGHTS];	/* 0x801e15b0: a group's table flags once it is on */
float		D_802372c4;

/* 0x800d6ef0: pick the wave's light table; the attract and high-score screens light groups 0
   and 1 at once, the levels wait for update_lights */
void load_level_lights(int wave)
{
	int	i;

	free_light_records();
	for(i = 0; i < MAX_LIGHTS; i++)
	{
		group_flags[i] = 0;
	}
	light_wave = wave;
	switch(wave)
	{
	case WAVE_HHOUSE:
		level_light_info_global = hhouse_light_info;
		num_light_groups = 0x44;
		break;
	case WAVE_FREAKSHOW:
		level_light_info_global = freakshow_light_info;
		num_light_groups = 0x30;
		break;
	case WAVE_RICKITY:
		level_light_info_global = rickitytown_light_info;
		num_light_groups = 0x24;
		break;
	case WAVE_BIGTOP:
		level_light_info_global = bigtop_light_info;
		num_light_groups = 3;
		break;
	case WAVE_HHOUSE_SOUNDS:
	case WAVE_HISCORE_ENTRY:
		level_light_info_global = hiscore_light_info;
		num_light_groups = 6;
		add_light_group(0);
		add_light_group(1);
		break;
	case WAVE_ATTRACT:
	case WAVE_LEVEL_SELECT:
		level_light_info_global = attract_light_info;
		num_light_groups = 2;
		break;
	default:
		printf("Could not find wave 0x%08x in load_level_light\n", wave);
		break;
	}
	if(num_light_groups >= MAX_LIGHTS + 1)
	{
		printf("Error, more lights in this wave than MAX_LIGHTS!\n");
	}
}

/* 0x800d7174, 0x800d719c */
void free_lights(void)
{
	free_light_list();
	free_light_records();
}

void remove_light_group(int n)
{
	int	id;

	id = light_wave | n;
	remove_light(id);
}

/* 0x800d71e0: switch on light group n of the table: a camera light, or a record with the
   position scaled up, the fade step and a directional light's direction from its angles */
void add_light_group(int n)
{
	int	id;
	light_info_t *info;
	level_light_t *l;
	float	angles[3];
	float	m[9];
	float	dir[3];

	if(n < 0)
	{
		n = 0;
	}
	if(num_light_groups - 1 < n)
	{
		n = num_light_groups - 1;
	}
	light_group = n;
	id = light_wave | light_group;
	if(check_if_light_exists(id))
	{
		printf("trying to add a light which already exists!!!\n");
		return;
	}
	info = &level_light_info_global[light_group];
	group_flags[n] = info->flags;
	if(info->flags & 4)
	{
		add_light_to_camera_(info->flags, info->color, info->falloff, info->life);
		return;
	}
	l = malloc(sizeof(level_light_t));
	if(!l)
	{
		printf("Could not malloc light\n");
		return;
	}
	l->id = id;
	l->type = info->type;
	l->flags = info->flags;
	l->pos[0] = info->v[0] * 10.0f;
	l->pos[1] = info->v[1] * 10.0f;
	l->pos[2] = info->v[2] * 10.0f;
	l->color[0] = info->color[0];
	l->color[1] = info->color[1];
	l->color[2] = info->color[2];
	if(l->flags & 0x40)
	{
		l->dcolor[0] = info->color[0] - level_light_info_global[light_group - 1].color[0];
		l->dcolor[1] = info->color[1] - level_light_info_global[light_group - 1].color[1];
		l->dcolor[2] = info->color[2] - level_light_info_global[light_group - 1].color[2];
		l->dcolor[0] = l->dcolor[0] / (float)(info->end - info->start);
		l->dcolor[2] = l->dcolor[2] / (float)(info->end - info->start);
		l->dcolor[1] = l->dcolor[1] / (float)(info->end - info->start);
		l->color[0] = level_light_info_global[light_group - 1].color[0];
		l->color[1] = level_light_info_global[light_group - 1].color[1];
		l->color[2] = level_light_info_global[light_group - 1].color[2];
	}
	if(l->flags & 0x80)
	{
		l->dcolor[0] = info->color[0] - level_light_info_global[light_group + 1].color[0];
		l->dcolor[1] = info->color[1] - level_light_info_global[light_group + 1].color[1];
		l->dcolor[2] = info->color[2] - level_light_info_global[light_group + 1].color[2];
		l->dcolor[0] = l->dcolor[0] / (float)(info->end - info->start);
		l->dcolor[2] = l->dcolor[2] / (float)(info->end - info->start);
		l->dcolor[1] = l->dcolor[1] / (float)(info->end - info->start);
	}
	switch(l->type)
	{
	case 2:
		angles[0] = info->v[1] * 0.017453292f;
		angles[1] = -info->v[0] * 0.017453292f;
		angles[2] = 0;
		rot_matrix_yxz(m, angles);
		dir[0] = 0;
		dir[1] = 0;
		dir[2] = -info->intensity * 2.56f;
		mat_vec_mul(dir, m, dir);
		l->pos[0] = dir[0];
		l->pos[1] = dir[1];
		l->pos[2] = dir[2];
		break;
	case 1:
		break;
	case 4:
		if(info->falloff < 1.0f)
		{
			printf("Warning! Invalid fallof value %f for light %d\n", info->falloff, n);
			l->falloff = 1.0f;
		}
		else
		{
			l->falloff = info->falloff;
		}
		break;
	}
	append_light_record(l);
}

/* 0x800d78fc: at boot, the haunted house's table */
void init_lights(void)
{
	D_802372c4 = 0.00512f;
	load_level_lights(WAVE_HHOUSE);
}

/* 0x800d7930: a point light made by code (an effect): its id */
int add_dynamic_light(int flags, float *pos, float *color, float falloff, int life)
{
	int	id;
	level_light_t *l;

	id = light_wave | next_light_id();
	l = malloc(sizeof(level_light_t));
	if(!l)
	{
		return(0);
	}
	l->id = id;
	l->type = 4;
	l->flags = flags;
	l->pos[0] = pos[0];
	l->pos[1] = pos[1];
	l->pos[2] = pos[2];
	l->color[0] = color[0];
	l->color[1] = color[1];
	l->color[2] = color[2];
	l->falloff = falloff;
	l->life = life;
	append_light_record(l);
	return(id);
}

/* 0x800d7a54: unreferenced: c minus the square of (b - a), towards zero */
float func_800d7a54(float a, float b, float c)
{
	float	d;
	float	r;

	d = b - a;
	if(d == 0.0f)
	{
		return(0.0f);
	}
	if(d < 0.0f)
	{
		r = -c + d * d;
		if(r > 0.0f)
		{
			r = 0;
		}
	}
	else
	{
		r = c - d * d;
		if(r < 0.0f)
		{
			r = 0;
		}
	}
	return(r);
}

/* 0x800d7b44: once per frame: the camera light follows the camera, the table's groups come
   on and go off at their frames, fades step, timed lights count down */
void update_lights(int frame)
{
	int	i;
	light_info_t *info;
	level_light_t *l;

	if(camera_light)
	{
		camera_light->pos[0] = camera_x;
		camera_light->pos[1] = camera_y;
		camera_light->pos[2] = camera_z;
	}
	for(i = 0; i < num_light_groups; i++)
	{
		info = &level_light_info_global[i];
		if(info->start == frame && group_flags[i] == 0)
		{
			add_light_group(i);
		}
		if(info->end == frame)
		{
			remove_light_group(i);
		}
	}
	for(l = light_root.next; l; l = l->next)
	{
		if(l->flags & 0xc0)
		{
			l->color[0] = l->color[0] + l->dcolor[0];
			l->color[1] = l->color[1] + l->dcolor[1];
			l->color[2] = l->color[2] + l->dcolor[2];
		}
		if(l->flags & 0x10)
		{
			l->life = l->life - 1;
			if(l->life <= 0)
			{
				remove_light(l->id);
			}
		}
	}
}

/* 0x800d7d60, 0x800d7dc4: light everything by one colour / stop doing so */
void set_ambient_override(float r, float g, float b)
{
	light_root.color[0] = r;
	light_root.color[1] = g;
	light_root.color[2] = b;
	light_root.flags |= 1;
	free_light_list();
}

void clear_ambient_override(void)
{
	light_root.color[0] = 0;
	light_root.color[1] = 0;
	light_root.color[2] = 0;
	light_root.flags &= ~1;
}

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
	if(light_root.flags & 1)
	{
		for(i = 0; i < n; i++)
		{
			c[0] = light_root.color[0];
			c[1] = light_root.color[1];
			c[2] = light_root.color[2];
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

/* 0x800d80d4: a debug check on n colours */
void check_light_colors(int n, float *colors)
{
	int	i;
	float	*c;

	c = colors;
	for(i = 0; i < n; i++)
	{
		if(c[0] < 20.0f && c[1] > 200.0f && c[2] < 20.0f)
		{
			printf("Goofy color in list!\n");
		}
		c += 3;
	}
}

/* 0x800d8198: the directional lights for a model at pos: every point light in range as a
   direction scaled by its attenuation, every directional light as it is */
void build_light_list(float *pos)
{
	level_light_t *l, *rec;
	light_t	*node;
	float	d[3];
	float	dist, range, atten;

	free_light_list();
	for(l = light_root.next; l; l = l->next)
	{
		if(!(l->flags & 1))
		{
			continue;
		}
		if(l->type == 4)
		{
			rec = l;
			d[0] = rec->pos[0] - pos[0];
			d[1] = rec->pos[1] - pos[1];
			d[2] = rec->pos[2] - pos[2];
			dist = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
			dist = fsqrt(dist);
			range = rec->falloff * 10.0f;
			if(dist < range && dist > 0.0f)
			{
				node = malloc(sizeof(light_t));
				if(!node)
				{
					printf("Could not malloc light\n");
					return;
				}
				vec_max_normalize(d);
				atten = 1.0f - dist * dist / (range * range);
				if(atten > 1.0f)
				{
					atten = 1.0f;
				}
				node->color[0] = rec->color[0];
				node->color[1] = rec->color[1];
				node->color[2] = rec->color[2];
				node->dir[0] = d[0] * -256.0f * atten;
				node->dir[1] = d[1] * 256.0f * atten;
				node->dir[2] = d[2] * 256.0f * atten;
				node->next = 0;
				append_light(node);
			}
		}
		else if(l->type == 2)
		{
			rec = l;
			node = malloc(sizeof(light_t));
			if(!node)
			{
				return;
			}
			node->color[0] = rec->color[0];
			node->color[1] = rec->color[1];
			node->color[2] = rec->color[2];
			node->dir[0] = rec->pos[0];
			node->dir[1] = rec->pos[1];
			node->dir[2] = rec->pos[2];
			node->next = 0;
			append_light(node);
		}
	}
}

/* 0x800d84f0: the record list */
int append_light_record(level_light_t *l)
{
	level_light_t *p, *n;

	p = &light_root;
	n = p->next;
	while(n)
	{
		p = n;
		n = p->next;
	}
	p->next = l;
	l->next = 0;
	return(1);
}

/* 0x800d8574 */
int move_light(int id, float *pos)
{
	int	found;
	level_light_t *p, *n;

	found = 0;
	p = &light_root;
	n = p->next;
	while(n && !found)
	{
		if(n->id == id)
		{
			found = 1;
		}
		else
		{
			p = n;
			n = p->next;
		}
	}
	if(found)
	{
		n->pos[0] = pos[0];
		n->pos[1] = pos[1];
		n->pos[2] = pos[2];
	}
	return(0);
}

/* 0x800d8660 */
int check_if_light_exists(int id)
{
	int	found;
	level_light_t *p, *n;

	found = 0;
	p = &light_root;
	n = p->next;
	while(n && !found)
	{
		if(n->id == id)
		{
			found = 1;
		}
		else
		{
			p = n;
			n = p->next;
		}
	}
	return(found);
}

/* 0x800d870c */
int remove_light(int id)
{
	int	found;
	level_light_t *p, *n;

	found = 0;
	p = &light_root;
	n = p->next;
	while(n && !found)
	{
		if(n->id == id)
		{
			p->next = n->next;
			if(n == camera_light)
			{
				camera_light = 0;
			}
			free(n);
			n = p->next;
			found = 1;
		}
		else
		{
			p = n;
			n = p->next;
		}
	}
	if(found)
	{
		return(1);
	}
	return(0);
}

/* 0x800d8824 */
int free_light_records(void)
{
	level_light_t *l, *n;

	l = light_root.next;
	while(l)
	{
		n = l;
		l = n->next;
		if(n == camera_light)
		{
			camera_light = 0;
		}
		free(n);
	}
	light_root.next = 0;
	return(1);
}

/* 0x800d88bc, 0x800d8988: the model's directional light list */
int append_light(light_t *l)
{
	light_t	*p, *n;

	p = light_list;
	if(!light_list)
	{
		light_list = l;
		light_list->next = 0;
	}
	else
	{
		n = p->next;
		while(n)
		{
			p = n;
			n = p->next;
		}
		p->next = l;
		l->next = 0;
	}
	light_count++;
	return(1);
}

int free_light_list(void)
{
	light_t	*l, *n;

	l = light_list;
	while(l)
	{
		n = l;
		l = n->next;
		free(n);
	}
	light_list = 0;
	light_count = 0;
	return(1);
}

/* 0x800d8a0c */
int next_light_id(void)
{
	int	id;

	id = next_id;
	next_id++;
	if(next_id > 0xffff)
	{
		next_id = 0x400;
	}
	return(id);
}

/* 0x800d8a78: the one light that follows the camera */
int add_light_to_camera_(int flags, float *color, float falloff, int life)
{
	int	id;
	level_light_t *l;

	if(camera_light)
	{
		printf("Already have a light attached to camera! Failing...\n");
		return(0);
	}
	id = light_wave | next_light_id();
	l = malloc(sizeof(level_light_t));
	if(!l)
	{
		return(0);
	}
	l->id = id;
	l->type = 4;
	l->flags = flags;
	l->pos[0] = camera_x;
	l->pos[1] = camera_y;
	l->pos[2] = camera_z;
	l->color[0] = color[0];
	l->color[1] = color[1];
	l->color[2] = color[2];
	l->falloff = falloff;
	l->life = life;
	append_light_record(l);
	camera_light = l;
	return(id);
}

/* 0x800d8bd4 */
void print_light_records(void)
{
	level_light_t *l;

	l = light_root.next;
	printf("ROOT->");
	while(l)
	{
		printf("0x%08lx->", (long)l->id);
		l = l->next;
	}
	printf("\n");
}
