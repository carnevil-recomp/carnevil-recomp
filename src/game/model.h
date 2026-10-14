/*
 * model.h - the enemies' models (src/game/model.c, 0x800deaa0-0x800e2d48). A model is three
 * arrays loaded by load_model (loaders.c): vertices (xyz), normals (xyz) and faces (a texture
 * key, three vertex and three normal indices, three uv pairs). The model is cut into
 * submeshes (one per limb); each has a range of each array, its own animation (one frame_t
 * per animation frame, loaded by load_anim) and a pivot. An enemy keeps a group_t per
 * submesh with the limb's current matrix and position.
 */
#ifndef MODEL_H
#define MODEL_H

typedef struct frame
{
	float	rot[3];
	float	pos[3];
} frame_t;					/* 24 bytes per submesh per frame */

typedef struct submesh
{
	short	face_start, face_end;		/* inclusive ranges */
	frame_t	*anim;				/* +0x04: filled in by load_anim */
	short	vert_start, vert_end;
	short	norm_start, norm_end;
	float	radius;
	float	pivot[3];
} submesh_t;					/* sizeof = 0x20 */

typedef struct face
{
	int	tex;				/* texture key (texmap_t.key), or'ed with the group's high bits */
	short	v[3];
	short	c[3];				/* normal (and so lit colour) indices */
	float	uv[3][2];
} face_t;					/* sizeof = 0x28 */

typedef struct pvert
{
	float	x, y, q;			/* screen position and 1/depth; q = -1 when clipped */
} pvert_t;

/* an enemy's per-submesh state (enemy_t.groups, one per submesh) */
#define GROUP_VISIBLE	0x8000
#define GROUP_ROOT	0x2000			/* placed by the animation and the enemy's rotation */
#define GROUP_ATTACHED	0x4000			/* placed from parent_pos/parent_mat and its own angles */
#define GROUP_TURNED	0x1000			/* the enemy's rotation times its own angles, at the enemy */
#define GROUP_ROOT_TURN	0x0001			/* the animation, then its own angles */
#define GROUP_SHIVER	0x0002			/* vertices jittered each frame */
typedef struct group
{
	int	flags;				/* placement bits above, draw mode bits 0x7f4, texture key high half; 0x4000 shot loose */
	unsigned short	life, life_step;	/* +0x04: a loose group's frames left and the step it counts by */
	float	pos[3];				/* +0x08: world position */
	float	mat[9];				/* +0x14: world rotation (scaled) */
	float	rot[3];				/* +0x38: own angles */
	float	off[3];				/* +0x44 */
	float	pivot[3];			/* +0x50: the submesh pivot in the world */
	float	vel[3];				/* +0x5c: a loose group's velocity */
	float	spin[3];			/* +0x68: its angular velocity */
	float	floor;				/* +0x74: it stops falling below this height */
	float	alpha;				/* +0x78 */
	float	alpha_step;			/* +0x7c: a loose group's alpha change per frame */
	float	parent_pos[3];			/* +0x80: a loose group flies from here */
	float	parent_mat[9];			/* +0x8c */
} group_t;					/* sizeof = 0xb0 */

/* a free-standing model piece (the chunks of flesh, 0x48 bytes): draw_part_model places a
   model at its position, rotation and scale with its alpha */
typedef struct part
{
	short	type;				/* +0x00: 1 = a chunk */
	short	state;				/* +0x02: 0 flying, 2 fading */
	float	pos[3];				/* +0x04 */
	float	rot[3];				/* +0x10 */
	float	scale[3];			/* +0x1c */
	float	vel[3];				/* +0x28 */
	float	alpha;				/* +0x34 */
	float	alpha_step;			/* +0x38 */
	int	unk3c;
	float	floor;				/* +0x40: it comes to rest here */
	struct part *next;			/* +0x44 */
} part_t;

#define MAX_MODEL_VERTS	0x1000
extern pvert_t	proj_verts[MAX_MODEL_VERTS];	/* 0x8022b1e8 */
extern float	proj_normals[MAX_MODEL_VERTS][3];	/* 0x8021f100 */
extern float	lit_colors[MAX_MODEL_VERTS][3];	/* 0x80212ea0 */

struct enemy;
void	draw_model(struct enemy *e, submesh_t *sub, int nsub, float *verts, int nverts, face_t *faces, int nfaces, float *normals, int nnormals);
void	init_groups(struct enemy *e, submesh_t *sub, int nsub);	/* 0x801149b4 */
void	draw_submesh_at(struct group *g, submesh_t *sub, float *verts, int nverts, face_t *faces, int nfaces, float *normals, int nnormals, float *mat, float *pos, float *scale);	/* 0x800e06d8 */
void	draw_part_model(part_t *p, submesh_t *sub, float *verts, int nverts, face_t *faces, int nfaces, float *normals, int nnormals);	/* 0x800e0268 */

int	load_anim(submesh_t *sub, int nsub, int nframes, char *name, int crc);
int	load_model(void **verts, int nverts, void **faces, int nfaces, void **normals, int nnormals, char *name, int crc);
int	load_path(void **out, int count, char *name, int crc);

#endif
