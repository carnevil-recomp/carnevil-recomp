/* the mods folder: replacement files and textures (mods.c) */
#ifndef PORT_MODS_H
#define PORT_MODS_H
#include <stddef.h>
extern const char	*port_mods_root;	/* --mods DIR */
extern int		port_mods;		/* --no-mods: 0 */
void		mods_init(void);
void		*host_alloc(size_t n), host_free(void *p), *host_realloc(void *p, size_t n);	/* outside the game's arena */
const char	*mods_lookup(const char *name);		/* the replacement for a game file, or NULL */
const char	*mods_texture(const char *wms);		/* NAME.png for NAME.WMS, or NULL */
#endif
