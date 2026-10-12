/*
 * texture.c - CarnEvil's texture files and the Voodoo texture memory, for the host port.
 *
 * The game has no .3df files: every texture is a .WMS file, a 40-byte Midway header
 * (u32 version 0x8005, u32 0, u32 1, u32 0, then a Glide Gu3dfHeader: width, height,
 * small_lod, large_lod, aspect_ratio, format) followed by the raw mipmap chain in Glide
 * texel format, largest level first. The game's guLoadTextureDirect() (0x80168558) reads the
 * file into a static buffer, checks the version, copies the header into the caller's
 * Gu3dfInfo and downloads the levels to the TMU. Here the levels stay in host memory and a
 * texture handle indexes this table; the renderer turns entries into GL textures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <glide/glide.h>
#include "hal.h"
#include "texture.h"

#define WMS_VERSION	0x8005
#define WMS_HEADER	0x28
#define TMU_SIZE	(4u << 20)		/* one 4 MB TMU on the Seattle Voodoo */
#define MAX_TEXTURES	2048

static port_texture_t	tex[MAX_TEXTURES];
static unsigned int	tmu_used;
static int		verbose;

/* bytes in one mip level: Glide LOD 0 = 256 texels on the long side ... 8 = 1 */
static unsigned int lod_bytes(const Gu3dfHeader *h, int lod)
{
	unsigned int	w = h->width >> (lod - h->large_lod), hh = h->height >> (lod - h->large_lod);
	if(!w) w = 1;
	if(!hh) hh = 1;
	return w * hh * (h->format >= GR_TEXFMT_16BIT ? 2 : 1);
}

unsigned int port_texture_bytes(const Gu3dfHeader *h)
{
	unsigned int	n = 0;
	int		lod;
	for(lod = h->large_lod; lod <= h->small_lod; lod++) n += lod_bytes(h, lod);
	return n;
}

/* parse a WMS image in memory; returns the texel data pointer or 0 */
static const unsigned char *wms_parse(const unsigned char *buf, unsigned int len, Gu3dfInfo *info, const char *what)
{
	const unsigned int	*w = (const unsigned int *)buf;
	if(len < WMS_HEADER || (w[0] & 0xffff7fff) != 5)
	{
		fprintf(stderr, "Version mismatch on file %s\nExpected %d and got %d\n", what, 5, len >= 4 ? (int)(w[0] & 0xffff7fff) : -1);
		return 0;
	}
	memcpy(&info->header, buf + 0x10, sizeof(Gu3dfHeader));
	info->mem_required = port_texture_bytes(&info->header);
	if(WMS_HEADER + info->mem_required > len)
	{
		fprintf(stderr, "[texture] %s: file too short for %ux%u lod %d..%d\n", what, info->header.width, info->header.height, info->header.large_lod, info->header.small_lod);
		return 0;
	}
	return buf + WMS_HEADER;
}

static unsigned char *read_file(const char *name, unsigned int *len)
{
	int	fd = open(name, O_RDONLY);
	unsigned char *buf;
	long	n;
	if(fd < 0) { fprintf(stderr, "Can not open file %s\n", name); return 0; }
	n = lseek(fd, 0, SEEK_END); lseek(fd, 0, SEEK_SET);
	buf = malloc(n > 0 ? n : 1);
	{ long got = read(fd, buf, n); if(verbose) fprintf(stderr, "[texture] read_file(%s): size %ld, got %ld, word0 %08x\n", name, n, got, *(unsigned int *)buf);
	  if(got != n) { fprintf(stderr, "Could not read texture file: %s\n", name); close(fd); free(buf); return 0; } }
	close(fd);
	*len = n;
	return buf;
}

static GrMipMapId_t download(const char *name, Gu3dfInfo *info, const unsigned char *texels)
{
	int	i;
	for(i = 0; i < MAX_TEXTURES && tex[i].used; i++) ;
	if(i == MAX_TEXTURES || tmu_used + info->mem_required > TMU_SIZE)
	{
		fprintf(stderr, "[texture] out of texture memory loading %s (%u used)\n", name, tmu_used);
		return GR_NULL_MIPMAP_HANDLE;
	}
	tex[i].used = 1;
	tex[i].header = info->header;
	tex[i].table = info->table;
	tex[i].bytes = info->mem_required;
	tex[i].data = malloc(info->mem_required);
	memcpy(tex[i].data, texels, info->mem_required);
	snprintf(tex[i].name, sizeof tex[i].name, "%s", name);
	tex[i].gl_dirty = 1;
	tmu_used += info->mem_required;
	if(verbose) fprintf(stderr, "[texture] %d <- %s %ux%u fmt %d lod %d..%d (%u bytes)\n", i, name, info->header.width, info->header.height, info->header.format, info->header.large_lod, info->header.small_lod, info->mem_required);
	return (GrMipMapId_t)i;
}

GrMipMapId_t guLoadTextureDirect(const char *name, Gu3dfInfo *info, int sclamp, int tclamp, int minfilt, int maxfilt)
{
	unsigned int	len;
	unsigned char	*buf = read_file(name, &len);
	const unsigned char *texels;
	GrMipMapId_t	h = GR_NULL_MIPMAP_HANDLE;
	(void)sclamp; (void)tclamp; (void)minfilt; (void)maxfilt;
	if(!buf) return h;
	if((texels = wms_parse(buf, len, info, name))) h = download(name, info, texels);
	free(buf);
	return h;
}

GrMipMapId_t guLoadTextureDirectFromMemory(char *buffer, Gu3dfInfo *info, int sclamp, int tclamp, int minfilt, int maxfilt)
{
	const unsigned char *texels = wms_parse((unsigned char *)buffer, ~0u, info, "(memory)");
	(void)sclamp; (void)tclamp; (void)minfilt; (void)maxfilt;
	return texels ? download("(memory)", info, texels) : GR_NULL_MIPMAP_HANDLE;
}

/* Glide's 3df helpers, used by texture_to_mem(): header first, then the data into the
   caller's buffer (info->data, mem_required bytes) */
int gu3dfGetInfo(const char *name, Gu3dfInfo *info)
{
	unsigned int	len;
	unsigned char	*buf = read_file(name, &len);
	int	ok;
	if(!buf) return 0;
	ok = wms_parse(buf, len, info, name) != 0;
	free(buf);
	return ok;
}

int gu3dfLoad(const char *name, Gu3dfInfo *info)
{
	unsigned int	len;
	unsigned char	*buf = read_file(name, &len);
	const unsigned char *texels;
	Gu3dfInfo	tmp;
	if(!buf) return 0;
	texels = wms_parse(buf, len, &tmp, name);
	if(texels && info->data) memcpy(info->data, texels, tmp.mem_required);
	free(buf);
	return texels != 0;
}

void grFreeTexture(GrMipMapId_t h)
{
	if(h < MAX_TEXTURES && tex[h].used)
	{
		tmu_used -= tex[h].bytes;
		free(tex[h].data);
		memset(&tex[h], 0, sizeof tex[h]);
		tex[h].gl_dirty = 1;
	}
}

void guTexMemReset(void)
{
	int	i;
	for(i = 0; i < MAX_TEXTURES; i++) if(tex[i].used) grFreeTexture(i);
	tmu_used = 0;
}

unsigned int get_mm_frag(int handle) { (void)handle; return 0; }
unsigned int get_tmu_fragmentation(void) { return 0; }
void show_tmu_map(void)
{
	int	i;
	fprintf(stderr, "TMU: %u of %u bytes used\r\n", tmu_used, TMU_SIZE);
	for(i = 0; i < MAX_TEXTURES; i++)
		if(tex[i].used) fprintf(stderr, " %4d %-14s %4ux%-4u fmt %2d lod %d..%d %7u\r\n", i, tex[i].name, tex[i].header.width, tex[i].header.height, tex[i].header.format, tex[i].header.large_lod, tex[i].header.small_lod, tex[i].bytes);
}

port_texture_t *port_texture(GrMipMapId_t h) { return (h < MAX_TEXTURES && tex[h].used) ? &tex[h] : 0; }
void port_texture_verbose(int v) { verbose = v; }
