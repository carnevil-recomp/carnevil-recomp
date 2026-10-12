/*
 * texmap.c - CarnEvil's texture maps: tables of (key, flags, name) entries that levels and
 * enemies load and release; each becomes a "surface" holding the GOOSE texture node.
 * Functions at 0x800d6814-0x800d6e60.
 */
#include <stdio.h>
#include <goose/texture.h>

#include "texmap.h"

surface_t	surfaces[MAX_SURFACES];
int		num_surfaces;
extern int	debug_mode;
extern int	D_801fad10;
extern int	D_801e0f10;
extern void	func_80162a10(int *, int *, int *, int *);

surface_t *find_surface(int key)
{
	int		i = 0;
	surface_t	*s = surfaces;

	for(; i < MAX_SURFACES; s++, i++)
	{
		if(surfaces[i].key == key)
		{
			return(s);
		}
	}
	return(0);
}

int add_surface(texmap_t *t)
{
	surface_t	*s;
	int		i;
	int		idx;

	idx = -1;
	s = 0;
	for(i = 0; i < MAX_SURFACES; i++)
	{
		if(surfaces[i].key == -1)
		{
			idx = i;
			goto found;
		}
	}
	idx = -1;
found:
	if(idx == -1)
	{
		for(i = 0; i < MAX_SURFACES; i++)
		{
			if(surfaces[i].refs <= 0)
			{
				idx = i;
				goto found2;
			}
		}
		idx = -1;
	}
found2:
	if(idx == -1)
	{
		printf("Could not add surface.\n");
		return(0);
	}
	s = &surfaces[idx];
	s->key = t->key;
	s->flags = t->flags;
	s->param = t->param;
	s->index = idx;
	num_surfaces++;
	s->refs = 1;
	t->index = idx;
	return(1);
}

int remove_surface_entry(surface_t *s)
{
	if(s->flags & 1)
	{
		if(!s->handle)
		{
			printf("Trying to remove_surface with decal 0!\n");
		}
		else
		{
			delete_texture(s->tn);
		}
	}
	s->key = -1;
	s->refs = 0;
	s->index = -1;
	num_surfaces--;
	return(1);
}

static void load_texmap_decal(texmap_t *t);
static void load_texmap_clamped(texmap_t *t);

void load_texmap(texmap_t *t, int n, char *label)
{
	surface_t	*s;
	int		i;
	int		j;
	int		key;
	int		m0, m1, m2, m3, m4;

	if(debug_mode & 0x200)
	{
		func_80162a10(&m0, &m1, &m2, &m3);
		printf("Loading texmaps for %s...", label);
	}
	for(i = 0; i < n; i++)
	{
		key = t->key;
		for(j = 0; j < MAX_SURFACES; j++)
		{
			if(surfaces[j].key == key)
			{
				s = &surfaces[j];
				goto have;
			}
		}
		s = 0;
have:
		if(s == 0)
		{
			if(!add_surface(t))
			{
				printf("Problem adding surface here!\n");
			}
			if(t->flags & 1)
			{
				if(t->flags & 4)
				{
					load_texmap_clamped(t);
				}
				else
				{
					load_texmap_decal(t);
				}
			}
		}
		else
		{
			s->refs++;
		}
		t++;
	}
	if(debug_mode & 0x200)
	{
		func_80162a10(&m0, &m1, &m4, &m3);
		printf("%dK\n", (m4 - m2) / 1024);
		printf("Total surfaces %d, %dK left\n", num_surfaces, m1 / 1024);
	}
}

void remove_surface(texmap_t *t, int n)
{
	surface_t	*s;
	int		i;
	int		j;
	int		key;

	for(i = 0; i < n; i++)
	{
		key = t->key;
		for(j = 0; j < MAX_SURFACES; j++)
		{
			if(surfaces[j].key == key)
			{
				s = &surfaces[j];
				goto have2;
			}
		}
		s = 0;
have2:
		if(!s)
		{
			printf("Error surface %s does not exist!\n", t->name);
		}
		else if(--s->refs <= 0)
		{
			if(s->flags & 1)
			{
				if(!s->handle)
				{
					printf("Trying to remove_surface with decal 0!\n");
				}
				else
				{
					delete_texture(s->tn);
				}
			}
			s->key = -1;
			s->refs = 0;
			s->index = -1;
			num_surfaces--;
		}
		t++;
	}
}

static void load_texmap_decal(texmap_t *t)
{
	surface_t	*s = &surfaces[t->index];

	s->tn = create_texture(t->name, 5, 0, 0, 0, 0, 1, 1);
	if(s->tn)
	{
		s->handle = s->tn->texture_handle;
		if(D_801fad10 == 1)
		{
			D_801e0f10 = s->tn->texture_handle;
		}
	}
	else
	{
		s->handle = 0;
		printf("Problem loading texmap %s\n", t->name);
	}
}

static void load_texmap_clamped(texmap_t *t)
{
	surface_t	*s = &surfaces[t->index];

	s->tn = create_texture(t->name, 5, 0, 0, 1, 1, 0, 0);
	if(s->tn)
	{
		s->handle = s->tn->texture_handle;
		if(D_801fad10 == 1)
		{
			D_801e0f10 = s->tn->texture_handle;
		}
	}
	else
	{
		s->handle = 0;
		printf("Problem loading texmap %s\n", t->name);
	}
}
