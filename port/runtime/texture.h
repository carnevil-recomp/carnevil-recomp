/* host-side texture table (port/runtime/texture.c) */
#ifndef PORT_TEXTURE_H
#define PORT_TEXTURE_H
#include <glide/glide.h>
typedef struct {
	int		used;
	Gu3dfHeader	header;
	unsigned int	bytes;
	unsigned char	*data;		/* mip chain, largest level first, Glide texel formats */
	char		name[16];
	GuTexTable	table;		/* palette / NCC table */
	unsigned int	gl_name;	/* renderer's objects, 0 = none */
	unsigned int	gl_keyed, gl_key;
	int		gl_dirty;
} port_texture_t;
port_texture_t *port_texture(GrMipMapId_t handle);
unsigned int port_texture_bytes(const Gu3dfHeader *h);
void port_texture_verbose(int v);
#endif
