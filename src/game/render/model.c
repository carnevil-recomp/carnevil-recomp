/*
 * model.c - drawing an enemy's model (0x800deaa0-0x800e2d48). draw_model() places each
 * submesh (from the animation frame, or blending two frames, or from a parent submesh),
 * projects its vertices and normals, lights the normals and hands the faces to one of the
 * face drawers below, chosen by the group's draw-mode bits. The rest of the file (the
 * per-enemy helpers at 0x800e2958 onwards) is still assembly.
 */
#include <stdio.h>
#include <glide/glide.h>
#include "model.h"
#include "camera.h"
#include "enemy.h"

extern int	randrng(int);
#include "texmap.h"
#include "credits.h"			/* debug_mode */

extern void	rot_matrix_zyx(float *, float *), rot_matrix_yxz(float *, float *), mat_mul(float *, float *, float *), mat_copy(float *, float *);
extern void	mat_scale(float *, float *), mat_unscale(float *, float *);
extern void	mat_vec_mul(float *, float *, float *), vec_add(float *, float *, float *);
extern void	project_vertices(int, float *, float *, float *, float *);
extern void	transform_vectors(int, float *, float *, float *);
extern void	light_vertices(int, float *, float *);				/* lights.c */
extern void	build_light_list(float *pos);				/* 0x800dxxxx */
extern float	func_800fe8dc(float range);					/* a random float in +-range */
extern void	func_800c8938(float *pos, float radius, unsigned int colour);	/* a debug sphere */
extern void	func_8015f30c(int, int, int, int, int);				/* texture combine set-up */
extern void	draw_tri_rgb(void *, void *, void *);				/* 0x800d1d8c */
extern void	func_800d1a38(void *, void *, void *);				/* a flat-colour triangle */
extern void	func_800d299c(void *, void *, void *);				/* a textured triangle with alpha */
extern void	func_800d3834(void *, void *, void *);
extern void	func_800d3bf0(void *, void *, void *);

pvert_t	proj_verts[MAX_MODEL_VERTS];		/* 0x8022b1e8 */
float	proj_normals[MAX_MODEL_VERTS][3];	/* 0x8021f100 */
float	lit_colors[MAX_MODEL_VERTS][3];		/* 0x80212ea0 */

/* vertices as the triangle drawers take them */
typedef struct tvert { float x, y, q, uq, vq, r, g, b; } tvert_t;		/* textured, lit */
typedef struct avert { float x, y, q, uq, vq, a, r, g, b, pad; } avert_t;	/* textured, lit, alpha */
typedef struct fvert { float x, y, q, r, g, b; } fvert_t;			/* flat colour */

void	draw_faces_tex(face_t *, pvert_t *, float (*)[3], short, short, int);
void	draw_faces_tex_or_const(face_t *, pvert_t *, float (*)[3], short, short, int);
void	draw_faces_tex_or_const2(face_t *, pvert_t *, float (*)[3], short, short, int);
void	draw_faces_tex_or_const3(face_t *, pvert_t *, float (*)[3], short, short, int);
void	draw_faces_one_key(face_t *, pvert_t *, float (*)[3], short, short, int);
void	draw_faces_tex_plain(face_t *, pvert_t *, float (*)[3], short, short, int);
void	draw_faces_flat(face_t *, pvert_t *, float (*)[3], short, short);
void	draw_faces_alpha(face_t *, pvert_t *, float (*)[3], short, short, int, float);
void	draw_faces_alpha2(face_t *, pvert_t *, float (*)[3], short, short, int, float);

/* 0x800e00c0: n vertices copied with a random shiver of up to 0.02 on each axis */
void shiver_vertices(int n, float *src, float *dst)
{
	float	x, y, z;
	int	i;

	for(i = 0; i < n; i++)
	{
		x = *src++;
		y = *src++;
		z = *src++;
		x = x + func_800fe8dc(0.02f);
		*dst++ = x;
		y = y + func_800fe8dc(0.02f);
		*dst++ = y;
		z = z + func_800fe8dc(0.02f);
		*dst++ = z;
	}
}

/* 0x800e0268: a free-standing piece: its one submesh projected at the part's position,
   rotation and scale, lit, and drawn with its alpha */
void draw_part_model(part_t *p, submesh_t *sub, float *verts, int nverts, face_t *faces, int nfaces, float *normals, int nnormals)
{
	float	scaled[9], unscaled[9];
	int	nv, nn;

	build_light_list(p->pos);
	rot_matrix_zyx(scaled, p->rot);
	rot_matrix_zyx(unscaled, p->rot);
	mat_scale(scaled, p->scale);
	nv = sub->vert_end - sub->vert_start + 1;
	project_vertices(nv, verts + sub->vert_start * 3, (float *)&proj_verts[sub->vert_start], scaled, p->pos);
	nn = sub->norm_end - sub->norm_start + 1;
	transform_vectors(nn, (float *)&proj_normals[sub->norm_start], normals + sub->norm_start * 3, unscaled);
	nn = sub->norm_end - sub->norm_start + 1;
	light_vertices(nn, (float *)&proj_normals[sub->norm_start], (float *)&lit_colors[sub->norm_start]);
	guAlphaSource(GR_ALPHASOURCE_ITERATED_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	draw_faces_alpha(faces, proj_verts, lit_colors, sub->face_start, sub->face_end, 0, p->alpha);
}

/* 0x800e059c: knock a group loose with a random fling: it leaves the animation (0x2000 off,
   0x4000 on) keeping its offset of from relative to to, flies up at 1 to 1.5 with up to 0.25
   sideways each way and spins, opaque, for 25 steps of life, until it falls to floor; drawn
   alpha-blended (0x80). The zombie's body comes apart with it (state 0x403). The last argument
   is not used */
void func_800e059c(group_t *g, float *from, float *to, float floor, float *unused)
{
	if(!(g->flags & 0x4000))
	{
		g->flags = (g->flags & ~0x2000) | 0x4000;
		g->off[0] = from[0] - to[0];
		g->off[1] = from[1] - to[1];
		g->off[2] = from[2] - to[2];
		g->rot[0] = 0.0f;
		g->rot[1] = 0.0f;
		g->rot[2] = 0.0f;
		g->spin[1] = 0.276f;
		g->spin[0] = 0.79f;
		g->spin[2] = 0.0f;
		g->vel[0] = (float)(randrng(1000) - 500) / 2000.0f;
		g->vel[1] = (float)randrng(1000) / 2000.0f + 1.0f;
		g->vel[2] = (float)(randrng(1000) - 500) / 2000.0f;
		g->life = 0x19;
		g->life_step = 0;
		g->floor = floor;
		g->alpha = 255.0f;
		g->alpha_step = 0.0f;
		g->flags |= 0x80;
	}
}

/* 0x800e06d8: one submesh drawn at a matrix and position of the caller's choosing with a
   group's draw flags (the held weapons: the group is the hand): alpha-blended with the
   group's alpha for flag 0x80, textured and alpha-tested for 0x40, plain otherwise */
void draw_submesh_at(group_t *g, submesh_t *sub, float *verts, int nverts, face_t *faces, int nfaces, float *normals, int nnormals, float *mat, float *pos, float *scale)
{
	float	unscaled[9];
	int	nv, nn;

	nv = sub->vert_end - sub->vert_start + 1;
	project_vertices(nv, verts + sub->vert_start * 3, (float *)&proj_verts[sub->vert_start], mat, pos);
	mat_copy(mat, unscaled);
	mat_unscale(unscaled, scale);
	nn = sub->norm_end - sub->norm_start + 1;
	transform_vectors(nn, (float *)&proj_normals[sub->norm_start], normals + sub->norm_start * 3, unscaled);
	nn = sub->norm_end - sub->norm_start + 1;
	light_vertices(nn, (float *)&proj_normals[sub->norm_start], (float *)&lit_colors[sub->norm_start]);
	if(g->flags & 0x80)
	{
		guAlphaSource(GR_ALPHASOURCE_ITERATED_ALPHA);
		grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
		draw_faces_alpha(faces, proj_verts, lit_colors, sub->face_start, sub->face_end, g->flags & 0xffff0000, g->alpha);
	}
	else
	{
		if(g->flags & 0x40)
		{
			guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
			grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
		}
		else
		{
			grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
		}
		draw_faces_tex_or_const2(faces, proj_verts, lit_colors, sub->face_start, sub->face_end, g->flags & 0xffff0000);
	}
}

extern void	detach_effect(struct enemy *, group_t *), move_effects(struct enemy *, group_t *, group_t *);	/* effects.c */

/* 0x80114560: hide a group and drop the effects riding it */
void hide_group(struct enemy *e, group_t *g)
{
	if(g->flags)
	{
		detach_effect(e, g);
		g->flags = 0;
	}
}

/* 0x8011459c: group to takes over group from's place (a shot-off part swapped for its stump):
   its flags (keeping its own alpha-test bit 0x40), matrix, position and pivot, and the
   effects riding from; from is hidden */
void replace_group(struct enemy *e, group_t *from, group_t *to)
{
	if(from->flags)
	{
		if(to->flags & 0x40)
		{
			to->flags = from->flags | 0x40;
		}
		else
		{
			to->flags = from->flags;
		}
		from->flags = 0;
		mat_copy(from->mat, to->mat);
		to->pos[0] = from->pos[0];
		to->pos[1] = from->pos[1];
		to->pos[2] = from->pos[2];
		to->pivot[0] = from->pivot[0];
		to->pivot[1] = from->pivot[1];
		to->pivot[2] = from->pivot[2];
		move_effects(e, from, to);
	}
}

/* 0x80114cdc: knock a group loose: it leaves the animation (0x2000 off, 0x4000 on) and flies
   from where it is, its offset kept relative to from, upward at 0.99 and spinning, opaque,
   for 25 steps of life, until it falls to floor; drawn alpha-blended (0x80). The fourth
   argument is not used */
void detach_group(group_t *g, float *from, float floor, float *unused)
{
	if(!(g->flags & 0x4000))
	{
		g->flags = (g->flags & ~0x2000) | 0x4000;
		mat_copy(g->mat, g->parent_mat);
		g->parent_pos[0] = g->pos[0];
		g->parent_pos[1] = g->pos[1];
		g->parent_pos[2] = g->pos[2];
		g->off[0] = g->pos[0] - from[0];
		g->off[1] = g->pos[1] - from[1];
		g->off[2] = g->pos[2] - from[2];
		g->life = 0x19;
		g->life_step = 0;
		g->rot[0] = 0.0f;
		g->rot[1] = 0.0f;
		g->rot[2] = 0.0f;
		g->vel[0] = 0.0f;
		g->vel[1] = 0.99f;
		g->vel[2] = 0.0f;
		g->spin[0] = 0.176f;
		g->spin[1] = 0.09f;
		g->spin[2] = 0.0f;
		g->floor = floor;
		g->alpha = 255.0f;
		g->alpha_step = 0.0f;
		g->flags |= 0x80;
	}
}

/* 0x80114b8c: a group shot loose (flag 0x4000) flies on its own: while it is above its
   floor it moves by its velocity, spins by its spin and falls (the y velocity loses 0.22 a
   frame); its life counts down by its step and its alpha changes by its step; at the end of
   either it is no longer loose */
void animate_loose_groups(enemy_t *e, int n)
{
	group_t	*g;
	int	i;

	for(i = 0; i < n; i++)
	{
		g = &e->groups[i];
		if(!(g->flags & 0x4000))
		{
			continue;
		}
		if(g->pos[1] > g->floor)
		{
			g->rot[0] = g->rot[0] + g->spin[0];
			g->rot[1] = g->rot[1] + g->spin[1];
			g->rot[2] = g->rot[2] + g->spin[2];
			g->parent_pos[0] = g->parent_pos[0] + g->vel[0];
			g->parent_pos[1] = g->parent_pos[1] + g->vel[1];
			g->parent_pos[2] = g->parent_pos[2] + g->vel[2];
			g->vel[0] = g->vel[0] + 0.0f;
			g->vel[1] = g->vel[1] + -0.22f;
			g->vel[2] = g->vel[2] + 0.0f;
		}
		g->life = g->life + g->life_step;
		if(g->life == 0)
		{
			g->flags &= ~0x4000;
			g->flags &= ~0x8000;
		}
		g->alpha = g->alpha + g->alpha_step;
		if(g->alpha <= 0.0f)
		{
			g->flags &= ~0x4000;
			g->flags &= ~0x8000;
		}
	}
}

/* 0x8011464c: place the groups at the current animation frame without drawing: a root group
   (GROUP_ROOT) gets the enemy's rotation and scale times its frame's angles and sits at the
   enemy's position plus its frame offset, its pivot carried along; any other group is reset to
   the origin with an unrotated matrix. The Haunted House's hand, bat, ghoul and Mari call it from
   their inits so their first hit test sees the pose */
void func_8011464c(enemy_t *e, submesh_t *sub, int nsub)
{
	float	m[9];
	float	r[9];
	float	p[3];
	group_t	*g;
	int	i;

	rot_matrix_zyx(m, &e->rx);
	mat_scale(m, e->scale);
	for(i = 0; i < nsub; i++)
	{
		g = &e->groups[i];
		if(g->flags & GROUP_ROOT)
		{
			rot_matrix_zyx(r, sub[i].anim[e->frame].rot);
			p[0] = sub[i].anim[e->frame].pos[0];
			p[1] = sub[i].anim[e->frame].pos[1];
			p[2] = sub[i].anim[e->frame].pos[2];
			mat_mul(m, r, g->mat);
			mat_vec_mul(p, m, p);
			vec_add(p, &e->x, g->pos);
			mat_vec_mul(sub[i].pivot, g->mat, g->pivot);
			vec_add(g->pivot, p, g->pivot);
		}
		else
		{
			g->pos[0] = 0.0f;
			g->pos[1] = 0.0f;
			g->pos[2] = 0.0f;
			g->pivot[0] = 0.0f;
			g->pivot[1] = 0.0f;
			g->pivot[2] = 0.0f;
			rot_matrix_yxz(g->mat, g->pos);
		}
	}
}

/* 0x80114814: the same for one group n (a part put back at its frame pose) */
void func_80114814(enemy_t *e, submesh_t *sub, int n)
{
	float	m[9];
	float	r[9];
	float	p[3];
	group_t	*g;

	rot_matrix_zyx(m, &e->rx);
	mat_scale(m, e->scale);
	g = &e->groups[n];
	if(g->flags & GROUP_ROOT)
	{
		rot_matrix_zyx(r, sub[n].anim[e->frame].rot);
		p[0] = sub[n].anim[e->frame].pos[0];
		p[1] = sub[n].anim[e->frame].pos[1];
		p[2] = sub[n].anim[e->frame].pos[2];
		mat_mul(m, r, g->mat);
		mat_vec_mul(p, m, p);
		vec_add(p, &e->x, g->pos);
		mat_vec_mul(sub[n].pivot, g->mat, g->pivot);
		vec_add(g->pivot, p, g->pivot);
	}
	else
	{
		g->pos[0] = 0.0f;
		g->pos[1] = 0.0f;
		g->pos[2] = 0.0f;
		g->pivot[0] = 0.0f;
		g->pivot[1] = 0.0f;
		g->pivot[2] = 0.0f;
		rot_matrix_yxz(g->mat, g->pos);
	}
}

/* 0x801149b4: every group starts at the enemy's position */
void init_groups(enemy_t *e, submesh_t *sub, int nsub)
{
	int	i;

	for(i = 0; i < nsub; i++)
	{
		e->groups[i].pos[0] = e->x;
		e->groups[i].pos[1] = e->y;
		e->groups[i].pos[2] = e->z;
		e->groups[i].pivot[0] = e->x;
		e->groups[i].pivot[1] = e->y;
		e->groups[i].pivot[2] = e->z;
	}
}

/* an angle difference into -pi..pi */
static float wrap_angle(float d)
{
	while(d > 3.1415927f)
	{
		d = d - 6.2831855f;
	}
	while(d < -3.141592f)
	{
		d = d + 6.2831855f;
	}
	return(d);
}

/* the rotation and position of submesh anim at the current frame, or between frame_from and
   frame_to when the enemy is blending (angles wrapped the short way round) */
static void frame_pose(enemy_t *e, frame_t *anim, float t, float *rot, float *pos)
{
	frame_t	*a, *b;

	if(e->flags & 2)
	{
		a = &anim[e->frame_from];
		b = &anim[e->frame_to];
		rot[0] = wrap_angle(b->rot[0] - a->rot[0]) * t + a->rot[0];
		rot[1] = wrap_angle(b->rot[1] - a->rot[1]) * t + a->rot[1];
		rot[2] = wrap_angle(b->rot[2] - a->rot[2]) * t + a->rot[2];
		pos[0] = (b->pos[0] - a->pos[0]) * t + a->pos[0];
		pos[1] = (b->pos[1] - a->pos[1]) * t + a->pos[1];
		pos[2] = (b->pos[2] - a->pos[2]) * t + a->pos[2];
	}
	else
	{
		a = &anim[e->frame];
		rot[0] = a->rot[0];
		rot[1] = a->rot[1];
		rot[2] = a->rot[2];
		pos[0] = a->pos[0];
		pos[1] = a->pos[1];
		pos[2] = a->pos[2];
	}
}

/* 0x800deaa0 (some_todo_with_animations) */
void draw_model(enemy_t *e, submesh_t *sub, int nsub, float *verts, int nverts, face_t *faces, int nfaces, float *normals, int nnormals)
{
	float	scaled[9], unscaled[9], anim_m[9], norm_m[9], own_m[9];
	float	rot[3], pos[3], epos[3], d[3];
	float	t;
	int	i, nv, nn, key;
	submesh_t *s;
	group_t	*g;

	if(nverts > MAX_MODEL_VERTS)
	{
		printf("ERROR! More vertices (%d) than I can handle(%d).\n", nverts, MAX_MODEL_VERTS);
		return;
	}
	if(nnormals > MAX_MODEL_VERTS)
	{
		printf("ERROR! More normals (%d) than I can handle(%d).\n", nnormals, MAX_MODEL_VERTS);
		return;
	}
	build_light_list(e->hit_group->pos);
	e->flags |= 1;
	rot_matrix_zyx(scaled, &e->rx);
	rot_matrix_zyx(unscaled, &e->rx);
	mat_scale(scaled, e->scale);
	t = 0.0f;
	if(e->flags & 2)
	{
		/* a blend in progress: the root motion between its frames moves the enemy */
		t = (float)e->blend_pos / (float)e->blend_len;
		e->x = e->x + e->vel[0];
		e->y = e->y + e->vel[1];
		e->z = e->z + e->vel[2];
	}
	epos[0] = e->x;
	epos[1] = e->y;
	epos[2] = e->z;
	for(i = 0; i < nsub; i++)
	{
		g = &e->groups[i];
		s = &sub[i];
		if(!(g->flags & GROUP_VISIBLE))
		{
			continue;
		}
		nv = s->vert_end - s->vert_start + 1;
		nn = s->norm_end - s->norm_start + 1;
		if(g->flags & GROUP_ROOT)
		{
			frame_pose(e, s->anim, t, rot, pos);
			rot_matrix_zyx(anim_m, rot);
			mat_mul(scaled, anim_m, g->mat);
			mat_vec_mul(pos, scaled, g->pos);
			vec_add(g->pos, epos, g->pos);
			if(g->flags & GROUP_SHIVER)
			{
				shiver_vertices(nv, verts + s->vert_start * 3, (float *)&proj_verts[s->vert_start]);
				project_vertices(nv, (float *)&proj_verts[s->vert_start], (float *)&proj_verts[s->vert_start], g->mat, g->pos);
			}
			else
			{
				project_vertices(nv, verts + s->vert_start * 3, (float *)&proj_verts[s->vert_start], g->mat, g->pos);
			}
			mat_vec_mul(s->pivot, g->mat, g->pivot);
			vec_add(g->pivot, g->pos, g->pivot);
			mat_mul(unscaled, anim_m, norm_m);
			transform_vectors(nn, proj_normals[s->norm_start], normals + s->norm_start * 3, norm_m);
		}
		else if(g->flags & GROUP_ATTACHED)
		{
			rot_matrix_zyx(anim_m, g->rot);
			mat_mul(anim_m, g->parent_mat, g->mat);
			d[0] = g->parent_pos[0] - g->off[0];
			d[1] = g->parent_pos[1] - g->off[1];
			d[2] = g->parent_pos[2] - g->off[2];
			mat_vec_mul(g->off, anim_m, pos);
			vec_add(d, pos, g->pos);
			project_vertices(nv, verts + s->vert_start * 3, (float *)&proj_verts[s->vert_start], g->mat, g->pos);
			mat_vec_mul(s->pivot, g->mat, g->pivot);
			vec_add(g->pivot, g->pos, g->pivot);
			mat_copy(g->mat, norm_m);
			mat_unscale(norm_m, e->scale);
			transform_vectors(nn, proj_normals[s->norm_start], normals + s->norm_start * 3, norm_m);
		}
		else if(g->flags & GROUP_TURNED)
		{
			rot_matrix_zyx(anim_m, g->rot);
			mat_mul(scaled, anim_m, g->mat);
			g->pos[0] = epos[0];
			g->pos[1] = epos[1];
			g->pos[2] = epos[2];
			project_vertices(nv, verts + s->vert_start * 3, (float *)&proj_verts[s->vert_start], g->mat, g->pos);
			mat_vec_mul(s->pivot, g->mat, g->pivot);
			vec_add(g->pivot, g->pos, g->pivot);
			mat_mul(unscaled, anim_m, norm_m);
			transform_vectors(nn, proj_normals[s->norm_start], normals + s->norm_start * 3, norm_m);
		}
		else if(g->flags & GROUP_ROOT_TURN)
		{
			frame_pose(e, s->anim, t, rot, pos);
			rot_matrix_zyx(anim_m, rot);
			rot_matrix_zyx(own_m, g->rot);
			mat_mul(anim_m, own_m, anim_m);
			mat_mul(scaled, anim_m, g->mat);
			mat_vec_mul(pos, scaled, g->pos);
			vec_add(g->pos, epos, g->pos);
			project_vertices(nv, verts + s->vert_start * 3, (float *)&proj_verts[s->vert_start], g->mat, g->pos);
			mat_vec_mul(s->pivot, g->mat, g->pivot);
			vec_add(g->pivot, g->pos, g->pivot);
			mat_mul(unscaled, anim_m, norm_m);
			transform_vectors(nn, proj_normals[s->norm_start], normals + s->norm_start * 3, norm_m);
		}
		else
		{
			mat_copy(scaled, g->mat);
			g->pos[0] = epos[0];
			g->pos[1] = epos[1];
			g->pos[2] = epos[2];
			project_vertices(nv, verts + s->vert_start * 3, (float *)&proj_verts[s->vert_start], g->mat, g->pos);
			mat_vec_mul(s->pivot, g->mat, g->pivot);
			vec_add(g->pivot, g->pos, g->pivot);
			transform_vectors(nn, proj_normals[s->norm_start], normals + s->norm_start * 3, unscaled);
		}
		light_vertices(nn, proj_normals[s->norm_start], lit_colors[s->norm_start]);
		key = g->flags & 0xffff0000;
		if(g->flags & 0x7f4)
		{
			if(g->flags & 0x80)
			{
				guAlphaSource(1);
				grAlphaBlendFunction(1, 5, 4, 0);
				if(g->flags & 8)
				{
					draw_faces_alpha2(faces, proj_verts, lit_colors, s->face_start, s->face_end, key, g->alpha);
					goto drawn;
				}
				/* without 8 the part is drawn with its alpha like a 0x100 part (0x800dfc28,
				   the call the 0x100 branch falls into); the transcription had left it undrawn */
				draw_faces_alpha(faces, proj_verts, lit_colors, s->face_start, s->face_end, key, g->alpha);
				goto drawn;
			}
			else if(g->flags & 0x40)
			{
				guAlphaSource(2);
				grAlphaBlendFunction(1, 5, 4, 0);
				draw_faces_tex_or_const2(faces, proj_verts, lit_colors, s->face_start, s->face_end, key);
				goto drawn;
			}
			else if(g->flags & 0x400)
			{
				grAlphaTestReferenceValue(5);
				guAlphaSource(0);
				grAlphaBlendFunction(1, 5, 4, 0);
				grConstantColorValue(((int)e->alpha << 24) | 0xffffff);
				draw_faces_tex_plain(faces, proj_verts, lit_colors, s->face_start, s->face_end, key);
				goto drawn;
			}
			else if(g->flags & 0x100)
			{
				guAlphaSource(3);
				grAlphaBlendFunction(1, 5, 4, 0);
				draw_faces_alpha(faces, proj_verts, lit_colors, s->face_start, s->face_end, key, g->alpha);
				goto drawn;
			}
			if(g->flags & 4)
			{
				draw_faces_one_key(faces, proj_verts, lit_colors, s->face_start, s->face_end, 0x1000);
			}
			else if(g->flags & 0x20)
			{
				draw_faces_one_key(faces, proj_verts, lit_colors, s->face_start, s->face_end, 0x1001);
			}
			else if(g->flags & 0x10)
			{
				draw_faces_tex(faces, proj_verts, lit_colors, s->face_start, s->face_end, key);
			}
			else if(g->flags & 0x200)
			{
				draw_faces_tex_or_const(faces, proj_verts, lit_colors, s->face_start, s->face_end, key);
			}
		}
		else
		{
			guAlphaSource(2);
			if(e->flags & 8)
			{
				if(g->flags & 8)
				{
					draw_faces_tex_or_const3(faces, proj_verts, lit_colors, s->face_start, s->face_end, key);
				}
				else
				{
					draw_faces_tex_or_const2(faces, proj_verts, lit_colors, s->face_start, s->face_end, key);
				}
			}
			else
			{
				draw_faces_flat(faces, proj_verts, lit_colors, s->face_start, s->face_end);
			}
		}
	drawn:
		if(debug_mode & 0x100)
		{
			func_800c8938(g->pivot, s->radius * e->scale[0], 0x60b060);
		}
	}
#ifdef PORT_HOST
	{ extern int port_trace; extern unsigned long hal_frame_count; static int shown;
	  extern float view_matrix[9], camera_neg_pos[3], camera_x, camera_y, camera_z, cull_x, cull_y, proj_x, proj_y;
	  if(port_trace && (shown++ % 40) == 0) { float p[3], w[3];
	    vec_add(e->groups[0].pos, camera_neg_pos, p); mat_vec_mul(p, view_matrix, w);
	    fprintf(stderr, "[model] vblank %lu enemy %d at (%.1f %.1f %.1f) frame %d cam (%.1f %.1f %.1f) neg (%.1f %.1f %.1f) view-space group0 (%.1f %.1f %.1f) cull %.2f %.2f proj %.1f %.1f vert0 q %.4f\n", hal_frame_count, e->num, e->x, e->y, e->z, e->frame, camera_x, camera_y, camera_z, camera_neg_pos[0], camera_neg_pos[1], camera_neg_pos[2], w[0], w[1], w[2], cull_x, cull_y, proj_x, proj_y, proj_verts[sub[0].vert_start].q);
	    { int gi, vis = 0, tot = 0; float minx = 9999, maxx = -9999, miny = 9999, maxy = -9999;
	      for(gi = 0; gi < nsub; gi++) { int vi; if(!(e->groups[gi].flags & GROUP_VISIBLE)) continue;
	        for(vi = sub[gi].vert_start; vi <= sub[gi].vert_end; vi++) { tot++; if(proj_verts[vi].q != -1.0f) { vis++; if(proj_verts[vi].x < minx) minx = proj_verts[vi].x; if(proj_verts[vi].x > maxx) maxx = proj_verts[vi].x; if(proj_verts[vi].y < miny) miny = proj_verts[vi].y; if(proj_verts[vi].y > maxy) maxy = proj_verts[vi].y; } } }
	      { extern float project_point(float *, float *); float s5[3], se[3]; project_point(e->groups[5].pos, s5); project_point(&e->x, se);
	      fprintf(stderr, "[model] visible %d of %d vertices, screen x %.0f..%.0f y %.0f..%.0f, scale %.2f %.2f %.2f, groups0 flags %x group5 pos (%.1f %.1f %.1f) -> screen (%.0f %.0f) q %.4f; feet -> screen (%.0f %.0f); cam angles (%.3f %.3f %.3f)\n", vis, tot, minx, maxx, miny, maxy, e->scale[0], e->scale[1], e->scale[2], e->groups[0].flags, e->groups[5].pos[0], e->groups[5].pos[1], e->groups[5].pos[2], s5[0], s5[1], s5[2], se[0], se[1], camera_rx, camera_ry, camera_rz); } } } }

#endif
	if(e->flags & 2)
	{
		e->blend_pos++;
		if(e->blend_pos >= e->blend_len)
		{
			e->flags &= ~2;
			e->frame = e->frame_to;
		}
	}
}

/* the face drawers: faces first..last of a submesh, skipping any with a clipped vertex.
   A face's texture is its key or'ed with the group's high half; a change of key looks the
   surface up (falling back to the face's own key) */
#define FACE_VERTS(f, pv, a, b, c) \
	a = &pv[f->v[0]]; b = &pv[f->v[1]]; c = &pv[f->v[2]]; \
	if(a->q == -1.0f || b->q == -1.0f || c->q == -1.0f) continue;
#define TEX_VERT(o, p, f, k, col) \
	o.x = p->x; o.y = p->y; o.q = p->q; o.uq = f->uv[k][0] * p->q; o.vq = f->uv[k][1] * p->q; \
	o.r = col[f->c[k]][0]; o.g = col[f->c[k]][1]; o.b = col[f->c[k]][2];

/* 0x800e1304: textured, no blending, the surface set up for texture modulation */
void draw_faces_tex(face_t *faces, pvert_t *pv, float (*col)[3], short first, short last, int hi)
{
	face_t	*f;
	pvert_t	*a, *b, *c;
	surface_t *s;
	tvert_t	va, vb, vc;
	int	key, last_key;

	last_key = -1;
	for(f = &faces[first]; first <= last; first++, f++)
	{
		key = f->tex | hi;
		if(key != last_key)
		{
			last_key = key;
			s = find_surface(key);
			if(!s)
			{
				s = find_surface(f->tex);
				if(!s)
				{
					printf("ERROR! Could not find surface %08x\n", key);
					printf("Shutting down drawing of this object.\n");
					return;
				}
			}
			guTexSource(s->handle);
			guAlphaSource(2);
			grAlphaBlendFunction(1, 5, 4, 0);
			func_8015f30c(3, 1, 0, 1, 1);
		}
		FACE_VERTS(f, pv, a, b, c);
		TEX_VERT(va, a, f, 0, col);
		TEX_VERT(vb, b, f, 1, col);
		TEX_VERT(vc, c, f, 2, col);
		draw_tri_rgb(&va, &vb, &vc);
	}
}

/* 0x800e1a1c: textured with the texture combine only */
void draw_faces_tex_plain(face_t *faces, pvert_t *pv, float (*col)[3], short first, short last, int hi)
{
	face_t	*f;
	pvert_t	*a, *b, *c;
	surface_t *s;
	tvert_t	va, vb, vc;
	int	key, last_key;

	last_key = -1;
	for(f = &faces[first]; first <= last; first++, f++)
	{
		key = f->tex | hi;
		if(key != last_key)
		{
			last_key = key;
			s = find_surface(key);
			if(!s)
			{
				s = find_surface(f->tex);
				if(!s)
				{
					printf("ERROR! Could not find surface %08x\n", key);
					printf("Shutting down drawing of this object.\n");
					return;
				}
			}
			guTexSource(s->handle);
			guColorCombineFunction(6);
		}
		FACE_VERTS(f, pv, a, b, c);
		TEX_VERT(va, a, f, 0, col);
		TEX_VERT(vb, b, f, 1, col);
		TEX_VERT(vc, c, f, 2, col);
		draw_tri_rgb(&va, &vb, &vc);
	}
}

/* 0x800e167c, 0x800e08c8, 0x800e0c68: textured when the surface has a texture, else its
   constant colour (three copies; the third uses another triangle routine) */
#define TEX_OR_CONST_BODY(tri) \
	last_key = -1; \
	for(f = &faces[first]; first <= last; first++, f++) \
	{ \
		key = f->tex | hi; \
		if(key != last_key) \
		{ \
			last_key = key; \
			s = find_surface(key); \
			if(!s) \
			{ \
				s = find_surface(f->tex); \
				if(!s) \
				{ \
					printf("ERROR! Could not find surface %08x\n", key); \
					printf("Shutting down drawing of this object.\n"); \
					return; \
				} \
			} \
			if(s->flags & 1) \
			{ \
				guTexSource(s->handle); \
				guColorCombineFunction(6); \
				guAlphaSource(2); \
				grAlphaBlendFunction(1, 5, 4, 0); \
			} \
			else \
			{ \
				guColorCombineFunction(1); \
				grConstantColorValue(s->param); \
			} \
		} \
		FACE_VERTS(f, pv, a, b, c); \
		TEX_VERT(va, a, f, 0, col); \
		TEX_VERT(vb, b, f, 1, col); \
		TEX_VERT(vc, c, f, 2, col); \
		tri(&va, &vb, &vc); \
	}

void draw_faces_tex_or_const(face_t *faces, pvert_t *pv, float (*col)[3], short first, short last, int hi)
{
	face_t	*f;
	pvert_t	*a, *b, *c;
	surface_t *s;
	tvert_t	va, vb, vc;
	int	key, last_key;

	TEX_OR_CONST_BODY(draw_tri_rgb)
}

void draw_faces_tex_or_const2(face_t *faces, pvert_t *pv, float (*col)[3], short first, short last, int hi)
{
	face_t	*f;
	pvert_t	*a, *b, *c;
	surface_t *s;
	tvert_t	va, vb, vc;
	int	key, last_key;

	TEX_OR_CONST_BODY(draw_tri_rgb)
}

void draw_faces_tex_or_const3(face_t *faces, pvert_t *pv, float (*col)[3], short first, short last, int hi)
{
	face_t	*f;
	pvert_t	*a, *b, *c;
	surface_t *s;
	tvert_t	va, vb, vc;
	int	key, last_key;

	TEX_OR_CONST_BODY(func_800d3834)
}

/* 0x800e1008: one surface (key) for the whole range */
void draw_faces_one_key(face_t *faces, pvert_t *pv, float (*col)[3], short first, short last, int key)
{
	face_t	*f;
	pvert_t	*a, *b, *c;
	surface_t *s;
	tvert_t	va, vb, vc;

	s = find_surface(key);
	guTexSource(s->handle);
	guColorCombineFunction(6);
	guAlphaSource(2);
	grAlphaBlendFunction(1, 5, 4, 0);
	for(f = &faces[first]; first <= last; first++, f++)
	{
		FACE_VERTS(f, pv, a, b, c);
		TEX_VERT(va, a, f, 0, col);
		TEX_VERT(vb, b, f, 1, col);
		TEX_VERT(vc, c, f, 2, col);
		draw_tri_rgb(&va, &vb, &vc);
	}
}

/* 0x800e1d64: the surface's constant colour times the lighting, no texture */
void draw_faces_flat(face_t *faces, pvert_t *pv, float (*col)[3], short first, short last)
{
	face_t	*f;
	pvert_t	*a, *b, *c;
	surface_t *s;
	fvert_t	va, vb, vc;
	int	last_key;
	float	r, g, bl;

	last_key = -1;
	r = 0.0f;
	g = 0.0f;
	bl = 0.0f;
	for(f = &faces[first]; first <= last; first++, f++)
	{
		if(f->tex != last_key)
		{
			last_key = f->tex;
			s = find_surface(f->tex);
			guColorCombineFunction(2);
			r = (float)((unsigned int)s->param >> 16 & 0xff) / 255.0f;
			g = (float)((unsigned int)s->param >> 8 & 0xff) / 255.0f;
			bl = (float)((unsigned int)s->param & 0xff) / 255.0f;
		}
		FACE_VERTS(f, pv, a, b, c);
		va.x = a->x; va.y = a->y; va.q = a->q;
		vb.x = b->x; vb.y = b->y; vb.q = b->q;
		vc.x = c->x; vc.y = c->y; vc.q = c->q;
		va.r = col[f->c[0]][0] * r; va.g = col[f->c[0]][1] * g; va.b = col[f->c[0]][2] * bl;
		vb.r = col[f->c[1]][0] * r; vb.g = col[f->c[1]][1] * g; vb.b = col[f->c[1]][2] * bl;
		vc.r = col[f->c[2]][0] * r; vc.g = col[f->c[2]][1] * g; vc.b = col[f->c[2]][2] * bl;
		func_800d1a38(&va, &vb, &vc);
	}
}

/* 0x800e2220, 0x800e25bc: textured (or constant colour) with a constant alpha on every vertex */
#define ALPHA_VERT(o, p, f, k, col) \
	o.a = alpha; o.x = p->x; o.y = p->y; o.q = p->q; o.uq = f->uv[k][0] * p->q; o.vq = f->uv[k][1] * p->q; \
	o.r = col[f->c[k]][0]; o.g = col[f->c[k]][1]; o.b = col[f->c[k]][2];
#define ALPHA_BODY(tri) \
	last_key = -1; \
	for(f = &faces[first]; first <= last; first++, f++) \
	{ \
		key = f->tex | hi; \
		if(key != last_key) \
		{ \
			last_key = key; \
			s = find_surface(key); \
			if(!s) \
			{ \
				s = find_surface(f->tex); \
				if(!s) \
				{ \
					printf("ERROR! Could not find surface %08x\n", key); \
					printf("Shutting down drawing of this object.\n"); \
					return; \
				} \
			} \
			if(s->flags & 1) \
			{ \
				guTexSource(s->handle); \
				guColorCombineFunction(6); \
			} \
			else \
			{ \
				guColorCombineFunction(1); \
				grConstantColorValue(s->param); \
			} \
		} \
		FACE_VERTS(f, pv, a, b, c); \
		ALPHA_VERT(va, a, f, 0, col); \
		ALPHA_VERT(vb, b, f, 1, col); \
		ALPHA_VERT(vc, c, f, 2, col); \
		tri(&va, &vb, &vc); \
	}

void draw_faces_alpha(face_t *faces, pvert_t *pv, float (*col)[3], short first, short last, int hi, float alpha)
{
	face_t	*f;
	pvert_t	*a, *b, *c;
	surface_t *s;
	avert_t	va, vb, vc;
	int	key, last_key;

	ALPHA_BODY(func_800d299c)
}

void draw_faces_alpha2(face_t *faces, pvert_t *pv, float (*col)[3], short first, short last, int hi, float alpha)
{
	face_t	*f;
	pvert_t	*a, *b, *c;
	surface_t *s;
	avert_t	va, vb, vc;
	int	key, last_key;

	ALPHA_BODY(func_800d3bf0)
}
