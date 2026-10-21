/* The mods folder: files that replace the game's own, without touching the extracted originals.

   mods/                 (--mods DIR; --no-mods turns it off)
     FILE.EXT            a loose file replaces the game file of that name (any case)
     PACK/...            a pack: every file in it (and one level of subfolders) replaces the game
                         file of the same name; a NAME.png replaces the texture NAME.WMS
     order.txt           optional: pack names, one per line, first wins; the rest alphabetically

   Loose files win over packs. The game's own written files (the state directory: CMOS, high
   scores) are never overridden. Lookups go through a name index built once at start-up. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include "mods.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

/* Mod data lives outside the game's arena (mem.c replaces malloc with the low 32-bit arena the
   game sees and measures its free memory by): a big replacement picture must not change how
   much memory the game thinks it has. Whole pages from the system, with the size in front. */
void *host_alloc(size_t n)
{
	size_t	*p;
	n += 16;
#ifdef _WIN32
	p = VirtualAlloc(NULL, n, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
	p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if(p == MAP_FAILED) p = NULL;
#endif
	if(!p) return NULL;
	*p = n;
	return (char *)p + 16;
}
void host_free(void *q)
{
	size_t *p;
	if(!q) return;
	p = (size_t *)((char *)q - 16);
#ifdef _WIN32
	VirtualFree(p, 0, MEM_RELEASE);
#else
	munmap(p, *p);
#endif
}
void *host_realloc(void *q, size_t n)
{
	void	*r = host_alloc(n);
	size_t	old;
	if(!r || !q) return r;
	old = *(size_t *)((char *)q - 16) - 16;
	memcpy(r, q, old < n ? old : n);
	host_free(q);
	return r;
}
static char *host_strdup(const char *s)
{
	char *d = host_alloc(strlen(s) + 1);
	if(d) strcpy(d, s);
	return d;
}

const char	*port_mods_root = "mods";
int		port_mods = 1;			/* --no-mods clears it */

typedef struct { char *key, *path; int used; } entry_t;
static entry_t	*idx;
static int	nidx, cap, nfiles, npacks, built;

static void lower(char *s) { for(; *s; s++) *s = (char)tolower((unsigned char)*s); }
static int is_dir(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }

static void add(const char *name, const char *path)
{
	char	key[256];
	int	i;
	snprintf(key, sizeof key, "%s", name);
	lower(key);
	for(i = 0; i < nidx; i++)
		if(!strcmp(idx[i].key, key)) return;			/* an earlier root wins */
	if(nidx == cap) { cap = cap ? cap * 2 : 256; idx = host_realloc(idx, cap * sizeof *idx); }
	idx[nidx].key = host_strdup(key); idx[nidx].path = host_strdup(path); idx[nidx].used = 0;
	nidx++; nfiles++;
}

/* the files of one directory, and (depth > 0) of its subfolders */
static void scan(const char *dir, int depth)
{
	DIR		*d = opendir(dir);
	struct dirent	*e;
	char		p[1024];
	if(!d) return;
	while((e = readdir(d)))
	{
		if(e->d_name[0] == '.') continue;
		snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
		if(is_dir(p)) { if(depth > 0) scan(p, depth - 1); continue; }
		if(!strcasecmp(e->d_name, "order.txt") || !strcasecmp(e->d_name, "readme.txt")) continue;
		add(e->d_name, p);
	}
	closedir(d);
}

static int cmp(const void *a, const void *b) { return strcasecmp(*(char *const *)a, *(char *const *)b); }

void mods_init(void)
{
	DIR		*d;
	struct dirent	*e;
	char		*packs[256], p[1024], line[256];
	int		n = 0, i;
	FILE		*f;
	if(built) return;
	built = 1;
	if(!port_mods || !is_dir(port_mods_root)) return;
	scan(port_mods_root, 0);					/* loose files first */
	snprintf(p, sizeof p, "%s/order.txt", port_mods_root);
	if((f = fopen(p, "r")))						/* packs named in order.txt */
	{
		while(fgets(line, sizeof line, f))
		{
			line[strcspn(line, "\r\n")] = 0;
			if(!line[0] || line[0] == '#') continue;
			snprintf(p, sizeof p, "%s/%s", port_mods_root, line);
			if(is_dir(p)) { scan(p, 1); npacks++; }
			else fprintf(stderr, "[mods] order.txt names %s, which is not a folder in %s\n", line, port_mods_root);
		}
		fclose(f);
	}
	if((d = opendir(port_mods_root)))				/* then the rest, alphabetically */
	{
		while((e = readdir(d)) && n < 256)
		{
			snprintf(p, sizeof p, "%s/%s", port_mods_root, e->d_name);
			if(e->d_name[0] != '.' && is_dir(p)) packs[n++] = host_strdup(e->d_name);
		}
		closedir(d);
	}
	qsort(packs, n, sizeof *packs, cmp);
	for(i = 0; i < n; i++)
	{
		snprintf(p, sizeof p, "%s/%s", port_mods_root, packs[i]);
		scan(p, 1);			/* add() keeps the first, so packs already read change nothing */
		host_free(packs[i]);
	}
	npacks = n;
	if(nfiles) fprintf(stderr, "[mods] %d replacement files in %s (%d pack%s)\n", nfiles, port_mods_root, npacks, npacks == 1 ? "" : "s");
}

const char *mods_lookup(const char *name)
{
	char		key[256];
	const char	*base = strrchr(name, '/');
	int		i;
	if(!built) mods_init();
	if(!nidx) return NULL;
	base = base ? base + 1 : name;
	if(strrchr(base, '\\')) base = strrchr(base, '\\') + 1;
	snprintf(key, sizeof key, "%s", base);
	lower(key);
	for(i = 0; i < nidx; i++)
		if(!strcmp(idx[i].key, key))
		{
			if(!idx[i].used) { fprintf(stderr, "[mods] %s <- %s\n", base, idx[i].path); idx[i].used = 1; }
			return idx[i].path;
		}
	return NULL;
}

/* a texture's replacement picture: NAME.png for NAME.WMS */
const char *mods_texture(const char *wms)
{
	char		png[256], *dot;
	const char	*base = strrchr(wms, '/');
	base = base ? base + 1 : wms;
	snprintf(png, sizeof png, "%s", base);
	if((dot = strrchr(png, '.'))) *dot = 0;
	strncat(png, ".png", sizeof png - strlen(png) - 1);
	return mods_lookup(png);
}
