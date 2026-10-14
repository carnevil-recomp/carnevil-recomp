#ifndef GAME_TEXMAP_H
#define GAME_TEXMAP_H
typedef struct texmap
{
	int	key;
	int	flags;			/* bit 0: has a texture, bit 2: clamped */
	int	param;
	int	index;			/* surface index, filled in by add_surface() */
	char	name[16];
} texmap_t;
typedef struct surface
{
	int			key;		/* -1 = free */
	int			flags;
	int			handle;		/* texture handle, 0 if the load failed */
	int			param;
	int			index;
	int			refs;
	struct texture_node	*tn;
} surface_t;
#define MAX_SURFACES	330
extern surface_t	surfaces[MAX_SURFACES];
extern int		num_surfaces;
surface_t *find_surface(int key);
int add_surface(texmap_t *t);
void load_texmap(texmap_t *t, int n, char *label);
void remove_surface(texmap_t *t, int n);
#endif
