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
#include <sys/stat.h>
#include <glide/glide.h>
#include "hal.h"
#include "texture.h"
#include "mods.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(n)		host_alloc(n)	/* outside the game's arena (mods.c) */
#define STBI_REALLOC(p, n)	host_realloc(p, n)
#define STBI_FREE(p)		host_free(p)
#define STBI_ONLY_PNG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../third_party/stb/stb_image.h"	/* public domain / MIT, see its end */
#pragma GCC diagnostic pop

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
	/* the mods folder: NAME.png in place of NAME.WMS, at any size with the same proportions (the
	   game's texture coordinates are fractions of the picture, so a bigger one is just sharper) */
	{
		const char	*png = strcmp(name, "(memory)") ? mods_texture(name) : NULL;
		int		w, h, n;
		unsigned char	*px = png ? stbi_load(png, &w, &h, &n, 4) : NULL;
		if(png && !px) fprintf(stderr, "[mods] %s: not a PNG this port can read (%s)\n", png, stbi_failure_reason());
		else if(px && (long)w * info->header.height != (long)h * info->header.width)
		{
			fprintf(stderr, "[mods] %s is %dx%d; %s is %ux%u: the proportions must match, so it is not used\n", png, w, h, name, info->header.width, info->header.height);
			stbi_image_free(px);
		}
		else if(px) { tex[i].mod_rgba = px; tex[i].mod_w = w; tex[i].mod_h = h; }
	}
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
		if(tex[h].mod_rgba) stbi_image_free(tex[h].mod_rgba);
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

/* --export-textures DIR: every .WMS of the game as DIR/NAME.png (its largest level, row 0 at the
   top: the orientation the mods folder reads back), the starting point for replacements */
#include <dirent.h>
#include <strings.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_MALLOC(n)		host_alloc(n)
#define STBIW_REALLOC(p, n)	host_realloc(p, n)
#define STBIW_FREE(p)		host_free(p)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../third_party/stb/stb_image_write.h"	/* public domain / MIT, see its end */
#pragma GCC diagnostic pop
extern void port_texel_rgba(int fmt, const unsigned char *p, const GuTexTable *tbl, unsigned char *o);
extern const char *hal_partition_root;
/* an absolute path: the HAL treats relative paths as the game's own, sending writes to the state
   directory and reads through the mods folder, so the exporter names its files absolutely */
static void host_path(const char *p, char *out, size_t n)
{
#ifdef _WIN32
	if(!_fullpath(out, p, n)) snprintf(out, n, "%s", p);
#else
	size_t	l;
	if(p[0] == '/' || !getcwd(out, n)) { snprintf(out, n, "%s", p); return; }
	l = strlen(out);
	snprintf(out + l, n - l, "/%s", p);
#endif
}
int port_export_textures(const char *to)
{
	char		root[1024], dir[1024];
	DIR		*d;
	struct dirent	*e;
	int		n = 0, bad = 0;
	host_path(hal_partition_root, root, sizeof root);
	host_path(to, dir, sizeof dir);
	if(!(d = opendir(root))) { fprintf(stderr, "[export] no game files at %s\n", root); return 1; }
#ifdef _WIN32
	mkdir(dir);
#else
	mkdir(dir, 0755);
#endif
	while((e = readdir(d)))
	{
		size_t		l = strlen(e->d_name);
		char		path[1024], out[1024], base[256];
		unsigned char	*buf, *rgba;
		unsigned int	len, i, w, h, bpt;
		const unsigned char *texels;
		Gu3dfInfo	info;
		FILE		*f;
		if(l < 5 || strcasecmp(e->d_name + l - 4, ".wms")) continue;
		snprintf(path, sizeof path, "%s/%s", root, e->d_name);
		if(!(f = fopen(path, "rb"))) { bad++; continue; }
		fseek(f, 0, SEEK_END); len = (unsigned int)ftell(f); fseek(f, 0, SEEK_SET);
		buf = malloc(len);
		if(fread(buf, 1, len, f) != len) { fclose(f); free(buf); bad++; continue; }
		fclose(f);
		if(!(texels = wms_parse(buf, len, &info, e->d_name))) { free(buf); bad++; continue; }
		w = info.header.width; h = info.header.height;
		bpt = info.header.format >= GR_TEXFMT_16BIT ? 2 : 1;
		rgba = malloc(w * h * 4);
		for(i = 0; i < w * h; i++) port_texel_rgba(info.header.format, texels + i * bpt, &info.table, rgba + i * 4);
		snprintf(base, sizeof base, "%.*s", (int)(l - 4), e->d_name);
		snprintf(out, sizeof out, "%s/%s.png", dir, base);
		if(stbi_write_png(out, (int)w, (int)h, 4, rgba, (int)w * 4)) n++; else bad++;
		free(rgba); free(buf);
	}
	closedir(d);
	if(bad) fprintf(stderr, "[export] %d textures written to %s; %d could not be read or written\n", n, dir, bad);
	else fprintf(stderr, "[export] %d textures written to %s\n", n, dir);
	return 0;
}
