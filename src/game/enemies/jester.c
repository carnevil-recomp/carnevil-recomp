/*
 * jester.c - the jester: one original file (0x801072e0-0x80109688, its tables at
 * 0x801929c0-0x80192ff0, jester_data.c). He is game object 0x2014: he dances on the loading
 * screen (preload_wave adds him in state 0x100), introduces each level (the cue states
 * 0x101-0x104 while the level loads) and appears in the levels as an enemy (spawn.c), where he
 * walks a path (jest.pth), runs at the player to hit him and can be shot: 50 points a hit,
 * a flinch every tenth, and at 100 damage he shatters into his 2530 faces, each a shard
 * that tumbles away while the pieces fade (get_jester_shards, draw_jester_shards). Two
 * coloured lights circle him on the loading screen and change colour at each shot.
 */
#include <stdlib.h>
#include <glide/glide.h>
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "texmap.h"
#include "credits.h"			/* player_stats_t, debug_mode */
#include "lights.h"
#include "levelsnd.h"
#include "waves.h"
#include "camera.h"

extern player_stats_t *get_target_player(void);			/* objects.c */
extern player_stats_t *attack_target(enemy_t *);
extern void	set_enemy_scale(enemy_t *, float), set_enemy_health(enemy_t *, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern int	find_submesh_hit(enemy_t *, submesh_t *, int, player_stats_t *);
extern float	shot_damage_of(player_stats_t *);			/* player.c */
extern void	hurt_player(player_stats_t *, int, int, float), award_points(int, int);
extern int	randrng(int);
extern float	func_800fe8dc(float);					/* a random float in +-range */
extern float	get_screen_pos(float *, float *);			/* vecmat.c */
extern void	rot_x(float *, float *), rot_y(float *, float *), rot_matrix_zyx(float *, float *);
extern void	mat_mul(float *, float *, float *), mat_vec_mul(float *, float *, float *), mat_scale(float *, float *);
extern void	vec_add(float *, float *, float *), translate_vectors(int, float *, float *, float *);
extern void	transform_vectors(int, float *, float *, float *);	/* model.c's file */
extern void	light_vertices(int, float *, float *), build_light_list(float *);	/* lights.c */
extern void	func_800d299c(void *, void *, void *);			/* a coloured, textured, alpha triangle */
extern void	func_800d326c(void *, void *, void *);			/* a textured triangle with the depth gradients */
extern void	draw_life_bar(float, float, float, float, float, unsigned int);	/* draw2d.c */
extern void	path_follow_turned(enemy_t *, float *, int, float);	/* enemy.c */
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern int	step_toward_goal(enemy_t *, float);
extern void	angles_to(float *, float *, float *);
extern float	distance_between(float *, float *);
extern void	func_800c99e4(enemy_t *, int);				/* debug: draw the groups' frames */
extern void	func_800c8938(float *, float, unsigned int);		/* debug sphere */
extern void	func_800d14a4(float, float);				/* debug map marker */
extern void	graphical_printf(char *, char *, ...), draw_string_at_color(int, char *, int, unsigned int);
extern int	load_model(void **, int, void **, int, void **, int, char *, int);
extern void	remove_model(void **, int, void **, int, void **, int, char *);
extern void	remove_animation(submesh_t *, int, int, char *), remove_path(void **, int, char *);
extern int	printf(const char *, ...), get_free_mem_left(void);
extern int	mark_die();						/* spawn.c: (id, killer) in the original */
extern int	D_801e7508;						/* shoot.c: a shot was fired this frame */
extern char	text_buffer[];

extern void	add_world_sprite(float *, int, int);			/* sprites.c */

/* jester_data.c */
extern texmap_t	jester_texmap[30];
extern submesh_t jester_submesh[14];				/* 0x80192d80 */
extern float	jester_light_colour[3][3];			/* 0x80192f40: two circling lights (the third colour is never used) */
extern float	jester_light_angles[3];				/* 0x80192f64 */
extern int	jester_light_flips;				/* 0x80192f70: which light recolours at the next shot */
extern int	jester_dance_cues[15];				/* 0x80192f74: the jesbig2 cues played while he dances ... */
extern int	jester_dance_cue;				/* 0x80192fb0: ... in turn */
extern int	jester_hit_cues[14];				/* 0x80192fb4: the cues for every tenth hit ... */
extern int	jester_hit_cue;					/* 0x80192fec */

float	*jester_verts, *jester_normals;			/* 0x801e5b98, 0x801e5858 */
face_t	*jester_faces;					/* 0x8021f0dc */
float	*jester_path;					/* 0x802373e0: jest.pth, 201 points */
enemy_t	*the_jester;					/* 0x801e0ec4 */
float	jester_max_health;				/* 0x801e43a0 */
int	jester_sound_timer;				/* 0x801fac7c: frames since a dance cue */
int	jester_hits;					/* 0x80237418: shots since the last flinch */
int	jester_shard_timer;				/* 0x80237408: the shards' alpha, 300 down by 2 a frame */
struct shard *jester_shards;				/* 0x801e7544 */
int	D_801de94c;					/* read here and by the big top (0x801394f4), set nowhere found: the jester cannot be hurt while it is set */

/* a shard's vertex for the coloured triangle drawer: screen position, texture, alpha, colour */
typedef struct shard_vertex
{
	float	x, y, oow, sow, tow, a, r, g, b;
	float	unused;
} shard_vertex_t;

/* a shard: one face of the model flying on its own (0x8c bytes) */
typedef struct shard
{
	int	key;			/* 0x00: the face's texture key */
	float	pos[3];			/* 0x04: its centre */
	float	vel[3];			/* 0x10: grows 2% a frame, the y part also falls */
	float	rot[3];			/* 0x1c: spins 0.2 and 0.1 a frame about x and y */
	float	corner[3][3];		/* 0x28: the three corners about the centre */
	float	colour[3][3];		/* 0x4c: the lit colours at the corners */
	float	uv[3][2];		/* 0x70 */
	float	alpha;			/* 0x88: 255, unused */
} shard_t;

/* the jester's states */
#define JESTER_LOADING		0x100		/* dancing on the loading screen */
#define JESTER_CUE_HHOUSE	0x101		/* the level introductions */
#define JESTER_CUE_FREAKSHOW	0x102
#define JESTER_CUE_RICKITY	0x103
#define JESTER_CUE_BIGTOP	0x104
#define JESTER_IDLE		0x98
#define JESTER_WALK		0x8080		/* along his path */
#define JESTER_ATTACK1		0x8081		/* runs at the player: three attacks ... */
#define JESTER_RETURN		0x8082		/* ... and the walk back to where he left the path */
#define JESTER_ATTACK2		0x8083
#define JESTER_ATTACK3		0x8084
#define JESTER_FLINCH		0x8085
#define JESTER_SHATTERED	0xf

static int	jester_hit(game_object_t *, player_stats_t *);
static int	jester_die(game_object_t *, player_stats_t *);
static void	remove_jester_object(game_object_t *);
static void	jester_dance_sound(void), jester_hit_sound(void);
static void	draw_jester_nameplate(float, float);
void		set_jester_state(enemy_t *, int);

/* 0x801072e0: draw and step the shards. Each is a coloured, textured triangle with the given
   alpha; afterwards it moves by its velocity (growing 2% a frame, the y part falling a
   thousandth) and spins */
void draw_jester_shards(shard_t *shards, int n, int alpha)
{
	shard_vertex_t	a, b, c;
	float		pa[3], pb[3], pc[3], m[9];
	float		fa;
	int		i;

	if(alpha >= 0x100)
	{
		fa = 255.0f;
	}
	else
	{
		fa = (float)alpha;
	}
	guAlphaSource(1);
	grAlphaBlendFunction(1, 5, 4, 0);
	guColorCombineFunction(6);
	for(i = 0; i < n; i++)
	{
		guTexSource(find_surface(shards[i].key)->handle);
		shards[i].rot[0] = shards[i].rot[0] + 0.2f;
		shards[i].rot[1] = shards[i].rot[1] + 0.1f;
		rot_matrix_zyx(m, shards[i].rot);
		mat_vec_mul(shards[i].corner[0], m, pa);
		mat_vec_mul(shards[i].corner[1], m, pb);
		mat_vec_mul(shards[i].corner[2], m, pc);
		pa[0] = pa[0] + shards[i].pos[0];
		pa[1] = pa[1] + shards[i].pos[1];
		pa[2] = pa[2] + shards[i].pos[2];
		pb[0] = pb[0] + shards[i].pos[0];
		pb[1] = pb[1] + shards[i].pos[1];
		pb[2] = pb[2] + shards[i].pos[2];
		pc[0] = pc[0] + shards[i].pos[0];
		pc[1] = pc[1] + shards[i].pos[1];
		pc[2] = pc[2] + shards[i].pos[2];
		if(get_screen_pos(pa, pa) != -1.0f && get_screen_pos(pb, pb) != -1.0f && get_screen_pos(pc, pc) != -1.0f)
		{
			a.a = fa;
			b.a = fa;
			c.a = fa;
			a.x = pa[0];
			a.y = pa[1];
			a.oow = pa[2];
			a.sow = shards[i].uv[0][0] * pa[2];
			a.tow = shards[i].uv[0][1] * pa[2];
			a.r = shards[i].colour[0][0];
			a.g = shards[i].colour[0][1];
			a.b = shards[i].colour[0][2];
			b.x = pb[0];
			b.y = pb[1];
			b.oow = pb[2];
			b.sow = shards[i].uv[1][0] * pb[2];
			b.tow = shards[i].uv[1][1] * pb[2];
			b.r = shards[i].colour[1][0];
			b.g = shards[i].colour[1][1];
			b.b = shards[i].colour[1][2];
			c.x = pc[0];
			c.y = pc[1];
			c.oow = pc[2];
			c.sow = shards[i].uv[2][0] * pc[2];
			c.tow = shards[i].uv[2][1] * pc[2];
			c.r = shards[i].colour[2][0];
			c.g = shards[i].colour[2][1];
			c.b = shards[i].colour[2][2];
			func_800d299c(&a, &b, &c);
		}
		shards[i].vel[0] = shards[i].vel[0] * 1.02f;
		shards[i].pos[0] = shards[i].pos[0] + shards[i].vel[0];
		shards[i].vel[1] = shards[i].vel[1] + -0.001f;
		shards[i].vel[2] = shards[i].vel[2] * 1.02f;
		shards[i].pos[1] = shards[i].pos[1] + shards[i].vel[1];
		shards[i].vel[1] = shards[i].vel[1] * 1.02f;
		shards[i].pos[2] = shards[i].pos[2] + shards[i].vel[2];
	}
}

/* 0x8010768c: shatter: every face of the model as it stands this frame (the groups placed
   by the animation, lit) becomes a shard with its corners about its centre, its lit colours,
   its texture and a random velocity of up to a hundredth each way */
shard_t *get_jester_shards(enemy_t *e, submesh_t *sub, int nsub, float *verts, int nverts, face_t *faces, int nfaces, float *normals, int nnormals)
{
	float		scaled[9], unscaled[9], anim_m[9], norm_m[9];
	float		pos[3], centre[3];
	shard_t		*shards;
	group_t		*g;
	submesh_t	*sm;
	int		i, j, k, nv, nn;

	printf("Heap %d\n", get_free_mem_left());
	printf("mallocing %d bytes!\n", nfaces * sizeof(shard_t));
	shards = malloc(nfaces * sizeof(shard_t));
	if(shards == 0)
	{
		return 0;
	}
	k = 0;
	build_light_list(e->hit_group->pos);
	rot_matrix_zyx(scaled, &e->rx);
	rot_matrix_zyx(unscaled, &e->rx);
	mat_scale(scaled, e->scale);
	for(i = 0, sm = sub; i < nsub; i++, sm++)
	{
		g = &e->groups[i];
		if(g->flags & GROUP_VISIBLE)
		{
			rot_matrix_zyx(anim_m, sm->anim[e->frame].rot);
			pos[0] = sm->anim[e->frame].pos[0];
			pos[1] = sm->anim[e->frame].pos[1];
			pos[2] = sm->anim[e->frame].pos[2];
			mat_mul(scaled, anim_m, g->mat);
			mat_vec_mul(pos, scaled, pos);
			vec_add(pos, &e->x, pos);
			nv = sm->vert_end - sm->vert_start + 1;
			transform_vectors(nv, (float *)&proj_verts[sm->vert_start], verts + sm->vert_start * 3, g->mat);
			nv = sm->vert_end - sm->vert_start + 1;
			translate_vectors(nv, (float *)&proj_verts[sm->vert_start], (float *)&proj_verts[sm->vert_start], pos);
			mat_mul(unscaled, anim_m, norm_m);
			nn = sm->norm_end - sm->norm_start + 1;
			transform_vectors(nn, (float *)&proj_normals[sm->norm_start], normals + sm->norm_start * 3, norm_m);
			nn = sm->norm_end - sm->norm_start + 1;
			light_vertices(nn, (float *)&proj_normals[sm->norm_start], (float *)&lit_colors[sm->norm_start]);
		}
		for(j = sm->face_start; j <= sm->face_end; j++, k++)
		{
			shards[k].key = faces[j].tex;
			shards[k].alpha = 255.0f;
			centre[0] = (proj_verts[faces[j].v[0]].x + proj_verts[faces[j].v[1]].x + proj_verts[faces[j].v[2]].x) / 3.0f;
			centre[1] = (proj_verts[faces[j].v[0]].y + proj_verts[faces[j].v[1]].y + proj_verts[faces[j].v[2]].y) / 3.0f;
			centre[2] = (proj_verts[faces[j].v[0]].q + proj_verts[faces[j].v[1]].q + proj_verts[faces[j].v[2]].q) / 3.0f;
			shards[k].corner[0][0] = proj_verts[faces[j].v[0]].x - centre[0];
			shards[k].corner[0][1] = proj_verts[faces[j].v[0]].y - centre[1];
			shards[k].corner[0][2] = proj_verts[faces[j].v[0]].q - centre[2];
			shards[k].corner[1][0] = proj_verts[faces[j].v[1]].x - centre[0];
			shards[k].corner[1][1] = proj_verts[faces[j].v[1]].y - centre[1];
			shards[k].corner[1][2] = proj_verts[faces[j].v[1]].q - centre[2];
			shards[k].corner[2][0] = proj_verts[faces[j].v[2]].x - centre[0];
			shards[k].corner[2][1] = proj_verts[faces[j].v[2]].y - centre[1];
			shards[k].corner[2][2] = proj_verts[faces[j].v[2]].q - centre[2];
			shards[k].colour[0][0] = lit_colors[faces[j].c[0]][0];
			shards[k].colour[0][1] = lit_colors[faces[j].c[0]][1];
			shards[k].colour[0][2] = lit_colors[faces[j].c[0]][2];
			shards[k].colour[1][0] = lit_colors[faces[j].c[1]][0];
			shards[k].colour[1][1] = lit_colors[faces[j].c[1]][1];
			shards[k].colour[1][2] = lit_colors[faces[j].c[1]][2];
			shards[k].colour[2][0] = lit_colors[faces[j].c[2]][0];
			shards[k].colour[2][1] = lit_colors[faces[j].c[2]][1];
			shards[k].colour[2][2] = lit_colors[faces[j].c[2]][2];
			shards[k].pos[0] = centre[0];
			shards[k].pos[1] = centre[1];
			shards[k].pos[2] = centre[2];
			shards[k].rot[0] = 0.0f;
			shards[k].rot[1] = 0.0f;
			shards[k].rot[2] = 0.0f;
			shards[k].vel[0] = func_800fe8dc(0.01f);
			shards[k].vel[1] = func_800fe8dc(0.01f);
			shards[k].vel[2] = func_800fe8dc(0.01f);
			shards[k].uv[0][0] = faces[j].uv[0][0];
			shards[k].uv[0][1] = faces[j].uv[0][1];
			shards[k].uv[1][0] = faces[j].uv[1][0];
			shards[k].uv[1][1] = faces[j].uv[1][1];
			shards[k].uv[2][0] = faces[j].uv[2][0];
			shards[k].uv[2][1] = faces[j].uv[2][1];
		}
	}
	return shards;
}

/* 0x80107e30: draw the jester, or his shards once shattered (they fade over 150 frames and
   then he goes). As an enemy (bit 0x8000) his health bar and name plate are drawn; on the
   loading screen (0x100) two lights circle him, one about the x axis and one about y, and a
   shot recolours one of them at random */
void draw_jester(game_object_t *o)
{
	enemy_t	*e;
	float	p[3], m[9];
	int	light1, light2;

	e = (enemy_t *)o->data;
	if(debug_mode & 0x1000)
	{
		func_800c99e4(e, 0xffffff);
	}
	if(e->ai_state == JESTER_SHATTERED)
	{
		draw_jester_shards(jester_shards, 0x9e2, jester_shard_timer);
		jester_shard_timer -= 2;
		if(jester_shard_timer <= 0)
		{
			remove_game_object(o->id);
		}
		return;
	}
	if(e->ai_state & 0x8000)
	{
		draw_life_bar(e->health, jester_max_health, 125.0f, 275.0f, 248.0f, 0xf0e010);
		draw_jester_nameplate(200.0f, 232.0f);
	}
	if(e->ai_state & JESTER_LOADING)
	{
		if(D_801e7508)
		{
			D_801e7508 = 0;
			if(jester_light_flips & 1)
			{
				jester_light_colour[0][0] = (float)randrng(255);
				jester_light_colour[0][1] = (float)randrng(255);
				jester_light_colour[0][2] = (float)randrng(255);
			}
			else
			{
				jester_light_colour[1][0] = (float)randrng(255);
				jester_light_colour[1][1] = (float)randrng(255);
				jester_light_colour[1][2] = (float)randrng(255);
			}
			jester_light_flips++;
		}
		p[0] = 0.0f;
		p[1] = 0.0f;
		p[2] = 10.0f;
		rot_x(m, jester_light_angles);
		mat_vec_mul(p, m, p);
		p[0] = p[0] + e->x;
		p[1] = p[1] + e->y;
		p[2] = p[2] + e->z;
		light1 = add_dynamic_light(0x11, p, jester_light_colour[0], 4.0f, 15);
		jester_light_angles[0] = jester_light_angles[0] + 0.13f;
		p[0] = 0.0f;
		p[1] = 0.0f;
		p[2] = 10.0f;
		rot_y(m, jester_light_angles);
		mat_vec_mul(p, m, p);
		p[0] = p[0] + e->x;
		p[1] = p[1] + e->y;
		p[2] = p[2] + e->z;
		light2 = add_dynamic_light(0x11, p, jester_light_colour[1], 4.0f, 15);
		jester_light_angles[1] = jester_light_angles[1] + 0.2f;
	}
	grChromakeyMode(0);
	draw_model(e, jester_submesh, 0xe, jester_verts, 0x650, jester_faces, 0x9e2, jester_normals, 0x60c);
	if(e->ai_state & JESTER_LOADING)
	{
		remove_light(light1);
		remove_light(light2);
	}
	if(debug_mode & DEBUG_SPAWN_LOG)
	{
		graphical_printf(text_buffer, "%d", e->num);
		draw_string_at_color(0, text_buffer, (int)e->hit_group->pos, 0xffffff);
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->hit_group->pos, e->radius, 0x600000);
	}
	if(debug_mode & 0x20)
	{
		guAlphaSource(1);
		guColorCombineFunction(1);
		grConstantColorValue(0xffffff00);
		grAlphaBlendFunction(1, 5, 1, 5);
		func_800d14a4(e->x * 0.12f + 200.0f, e->z * 0.12f + 128.0f);
		grAlphaBlendFunction(4, 0, 4, 0);
	}
}

/* 0x801082e8: a shot against the jester as an enemy: his sphere, then the submesh */
static int jester_hit(game_object_t *o, player_stats_t *p)
{
	enemy_t	*e;
	int	group;

	e = (enemy_t *)o->data;
	if(e->ai_state != JESTER_SHATTERED && (e->ai_state & 0x8000))
	{
		if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
		{
			if((unsigned int)p->unk10 != 0xffff0000)
			{
				p->unk10 = 0xffffff00;
			}
			group = find_submesh_hit(e, jester_submesh, 0xe, p);
			if(group != -1)
			{
				p->hit = 1;
				p->hit_group = group;
				p->die = (void *)jester_die;
				p->hit_id = o->id;
				p->hit_point[0] = p->aim[0];
				p->hit_point[1] = p->aim[1];
				p->hit_point[2] = p->aim[2];
			}
		}
	}
	return 0;
}

/* the damage taken: at 0 health he shatters for 1000 points (the shards are kept 300 ticks),
   otherwise 50 points a hit and a flinch every tenth (the hit animation from frame 76,
   the state and frame he was in resumed after it) */
#define JESTER_HURT(o, p, e) \
	if(e->health <= 0.0f) \
	{ \
		award_points(p->index, 1000); \
		play_jesbig2_sound(0x34, 8); \
		e->ai_state = JESTER_SHATTERED; \
		jester_shard_timer = 300; \
		e->killer = p; \
		jester_shards = get_jester_shards(e, jester_submesh, 0xe, jester_verts, 0x650, jester_faces, 0x9e2, jester_normals, 0x60c); \
		mark_die(o->id, e->killer); \
		if(jester_shards == 0) \
		{ \
			printf("Could not get jester mog!\n"); \
			remove_game_object(o->id); \
		} \
	} \
	else \
	{ \
		award_points(p->index, 50); \
		jester_hits++; \
		if(jester_hits >= 10) \
		{ \
			jester_hit_sound(); \
			jester_hits = 0; \
			if(e->ai_state == JESTER_FLINCH) \
			{ \
				e->frame = 0x4c; \
			} \
			else \
			{ \
				e->resume_frame = e->frame; \
				e->resume_state = e->ai_state; \
				e->ai_state = JESTER_FLINCH; \
				e->frame = 0x4c; \
			} \
		} \
	}

/* 0x801083d0: damage given directly (nothing calls it; the shot path below does the same) */
void jester_take_damage(game_object_t *o, player_stats_t *p, float damage, enemy_t *e)
{
	e->health = e->health - damage;
	JESTER_HURT(o, p, e)
}

/* 0x80108564: the shot landed: a hit mark, and unless the jester is protected the damage */
static int jester_die(game_object_t *o, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	add_world_sprite(p->hit_point, 1, 0);
	if(D_801de94c)
	{
		return 0;
	}
	e->health = e->health - shot_damage_of(p);
	JESTER_HURT(o, p, e)
	return 0;
}

/* 0x80108724 */
void load_jester_texmap(void)
{
	load_texmap(jester_texmap, 0x1e, "jest");
}

/* 0x80108754 */
void load_jester_model(void)
{
	load_model((void **)&jester_verts, 0x650, (void **)&jester_faces, 0x9e2, (void **)&jester_normals, 0x60c, "JEST.ZM", (int)0x60021f6f);
}

/* 0x801087b4-0x80108950: the level introductions' animations */
void load_hhouse_jester_anim(void)
{
	load_anim(jester_submesh, 0xe, 0x104, "jesthhou.za", (int)0x1ecacd4f);
}

void remove_hhouse_jester_anim(void)
{
	remove_animation(jester_submesh, 0xe, 0x104, "jesthhou.za");
}

void load_freakshow_jester_anim(void)
{
	load_anim(jester_submesh, 0xe, 0xdc, "jestfrek.za", (int)0xac71c590);
}

void remove_freakshow_jester_anim(void)
{
	remove_animation(jester_submesh, 0xe, 0xdc, "jestfrek.za");
}

void load_rickity_jester_anim(void)
{
	load_anim(jester_submesh, 0xe, 0xdc, "jestrick.za", (int)0xac71c590);
}

void remove_rickity_jester_anim(void)
{
	remove_animation(jester_submesh, 0xe, 0xdc, "jestrick.za");
}

void load_bigtop_jester_anim(void)
{
	load_anim(jester_submesh, 0xe, 0xdc, "jestbtop.za", (int)0xac71c590);
}

void remove_bigtop_jester_anim(void)
{
	remove_animation(jester_submesh, 0xe, 0xdc, "jestbtop.za");
}

/* 0x80108984: the enemy's animation and path */
void load_jester_path(void)
{
	load_anim(jester_submesh, 0xe, 0x65, "jest.za", (int)0x477986fd);
	load_path((void **)&jester_path, 0xc9, "jest.pth", (int)0xdafe3f59);
}

/* 0x801089e4 */
void remove_jester_path(void)
{
	remove_path((void **)&jester_path, 0xc9, "jest.pth");
	remove_animation(jester_submesh, 0xe, 0x65, "jest.za");
}

/* 0x80108a30: the animation, model and texmaps */
void remove_jester(void)
{
	remove_animation(jester_submesh, 0xe, 0x65, "jest.za");
	remove_model((void **)&jester_verts, 0x650, (void **)&jester_faces, 0x9e2, (void **)&jester_normals, 0x60c, "JEST.ZM");
	remove_surface(jester_texmap, 0x1e);
}

/* 0x80108ab0: the record: 8 up, scale 0.75, hit radius 0.5 of it, 100 health, fourteen
   groups all shown, standing still */
int init_jester(game_object_t *o)
{
	enemy_t	*e;
	int	i;

	o->data = malloc(sizeof(enemy_t));
	if(!o->data)
	{
		return 1;
	}
	the_jester = e = (enemy_t *)o->data;
	o->hit = jester_hit;
	o->die = jester_die;
	o->remove = remove_jester_object;
	e->counter = 0;
	e->frame = 0;
	e->num = 0;
	e->flags = 0xc;
	e->ai_state = 0;
	e->target = get_target_player();
	e->rx = 0.0f;
	e->ry = 0.0f;
	e->rz = 0.0f;
	e->x = 0.0f;
	e->z = 0.0f;
	e->y = 8.0f;
	e->unkcc = 0.5f;
	set_enemy_scale(e, 0.75f);
	e->vel[0] = 0.0f;
	e->vel[1] = 0.0f;
	e->vel[2] = 0.0f;
	set_enemy_health(e, 100.0f);
	jester_max_health = e->health;
	e->groups = malloc(0xe * sizeof(group_t));
	if(e->groups == 0)
	{
		free(e);
		return 1;
	}
	for(i = 0; i < 0xe; i++)
	{
		e->groups[i].flags = 0xa000;
	}
	init_groups(e, jester_submesh, 0xe);
	e->hit_group = e->groups;
	e->hit_group->floor = 0.0f;
	e->path = 0;
	e->prio_base = 0x10;
	e->prio_flags = next_cue_priority();
	set_jester_state(e, 0);
	jester_hits = 0;
	jester_sound_timer = 0;
	return 0;
}

/* 0x80108c3c */
static void remove_jester_object(game_object_t *o)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	if(e->ai_state == JESTER_SHATTERED)
	{
		free(jester_shards);
	}
	free(e->groups);
	free(e);
}

/* the three attacks: he runs at a point 2 to the side and 12 short of the camera, hits the
   player for 100 on the attack's frame, and when the animation ends walks back to where he
   left the path */
#define JESTER_ATTACK(e, hit_frame, last_frame) \
	jester_dance_sound(); \
	e->frame++; \
	if(e->frame == hit_frame) \
	{ \
		hurt_player(e->target, 100, 4, 0.0f); \
	} \
	set_goal_near_camera(e, camera_pos, e->goal, 2.0f, 0.0f, -12.0f); \
	if(step_toward_goal(e, e->unk9c) != 2) \
	{ \
		set_goal_near_camera(e, camera_pos, e->goal, 2.0f, 0.0f, 0.0f); \
		angles_to(e->goal, &e->x, &e->rx); \
	} \
	if(e->frame < last_frame) \
	{ \
		return; \
	} \
	e->goal[0] = e->vel[0]; \
	e->goal[1] = e->vel[1]; \
	e->goal[2] = e->vel[2]; \
	e->frame = 0; \
	e->ai_state = JESTER_RETURN;

/* 0x80108c94: the jester's frame-by-frame behaviour: the loading and introduction loops
   just cycle their animations; as an enemy he walks the path (jumping between its three
   loops at their ends), and once in a while (5 in 200 a frame, past point 41) picks one of
   the three attacks, remembering where he was */
void jester_think(game_object_t *o)
{
	enemy_t	*e;
	int	r;

	e = (enemy_t *)o->data;
	if(debug_mode & DEBUG_CHARACTERS)
	{
		return;
	}
	switch(e->ai_state)
	{
	case JESTER_IDLE:
		e->frame++;
		if(e->frame >= 0x65)
		{
			e->frame = 0;
		}
		break;
	case JESTER_CUE_HHOUSE:
		e->frame++;
		if(e->frame >= 0x104)
		{
			e->frame = 0xc6;
		}
		break;
	case JESTER_CUE_FREAKSHOW:
		e->frame++;
		if(e->frame >= 0xdc)
		{
			e->frame = 0xad;
		}
		break;
	case JESTER_CUE_RICKITY:
		e->frame++;
		if(e->frame >= 0xdc)
		{
			e->frame = 0xad;
		}
		break;
	case JESTER_CUE_BIGTOP:
		e->frame++;
		if(e->frame >= 0xdc)
		{
			e->frame = 0xad;
		}
		break;
	case JESTER_WALK:
		jester_dance_sound();
		e->frame++;
		if(e->frame >= 0x13)
		{
			e->frame = 0;
		}
		path_follow_turned(e, jester_path, e->path, 1.0f);
		if(e->path == 0x64 || e->path == 0xaa || e->path == 0xc8)
		{
			r = randrng(100);
			if(r < 0x21)
			{
				e->path = 0x29;
			}
			else if(r < 0x43)
			{
				e->path = 0x65;
			}
			else
			{
				e->path = 0xab;
			}
		}
		r = randrng(200);
		if(r < 5 && e->path >= 0x29)
		{
			if(r == 0)
			{
				e->frame = 0x26;
				e->ai_state = JESTER_ATTACK2;
			}
			else if(r == 1)
			{
				e->frame = 0x39;
				e->ai_state = JESTER_ATTACK3;
			}
			else
			{
				e->frame = 0x13;
				e->ai_state = JESTER_ATTACK1;
			}
			e->target = get_target_player();
			set_goal_near_camera(e, camera_pos, e->goal, 2.0f, 0.0f, -12.0f);
			e->vel[0] = e->x;
			e->vel[1] = e->y;
			e->vel[2] = e->z;
			e->unkb4 = e->rx;
			e->unkb8 = e->ry;
			e->unkbc = e->rz;
			e->unk9c = distance_between(camera_pos, &e->x) / 10.0f;
		}
		break;
	case JESTER_ATTACK1:
		JESTER_ATTACK(e, 0x1c, 0x26)
		break;
	case JESTER_ATTACK2:
		JESTER_ATTACK(e, 0x2f, 0x39)
		break;
	case JESTER_ATTACK3:
		JESTER_ATTACK(e, 0x42, 0x4c)
		break;
	case JESTER_RETURN:
		jester_dance_sound();
		e->frame++;
		if(e->frame >= 0x13)
		{
			e->frame = 0;
		}
		if(step_toward_goal(e, e->unk9c) == 2)
		{
			e->ai_state = JESTER_WALK;
		}
		break;
	case JESTER_FLINCH:
		jester_dance_sound();
		e->frame++;
		if(e->frame >= 0x65)
		{
			e->ai_state = e->resume_state;
			e->frame = e->resume_frame;
		}
		break;
	}
}

/* 0x80109260: put the jester in a state: the loading screen's dance starts at the origin
   facing the camera with a sprite of kind 8 thirty units out, the walk with the jesbig2
   cue 0x36 and his first path point; every state restarts the animation */
void set_jester_state(enemy_t *e, int state)
{
	float	pos[3];

	e->ai_state = state;
	switch(state)
	{
	case JESTER_CUE_FREAKSHOW:
		e->frame = 0;
		break;
	case JESTER_CUE_BIGTOP:
		e->frame = 0;
		break;
	case JESTER_LOADING:
		e->frame = 0;
		e->rx = 0.0f;
		e->rz = 0.0f;
		e->x = 0.0f;
		e->y = 0.0f;
		e->z = 0.0f;
		e->ry = 3.1415927f;
		pos[0] = 0.0f;
		pos[1] = 0.0f;
		pos[2] = 30.0f;
		add_world_sprite(pos, 8, 0);
		break;
	case JESTER_WALK:
		e->frame = 0;
		play_jesbig2_sound(0x36, 8);
		path_follow_turned(e, jester_path, e->path, 1.0f);
		break;
	default:
		e->frame = 0;
		break;
	}
}

/* 0x8010934c-0x80109400: the level introductions: the jester's animation and the cue */
void jester_cue_freakshow(void)
{
	float	unused[3];		/* the original's frame holds a local it never uses */

	the_jester->ai_state = JESTER_CUE_FREAKSHOW;
	the_jester->frame = 0;
	play_rotating_cue(1, "jestfrek");
}

void jester_cue_hhouse(void)
{
	float	unused[3];		/* the original's frame holds a local it never uses */

	the_jester->ai_state = JESTER_CUE_HHOUSE;
	the_jester->frame = 0;
	play_rotating_cue(1, "jesthhou");
}

void jester_cue_rickity(void)
{
	float	unused[3];		/* the original's frame holds a local it never uses */

	the_jester->ai_state = JESTER_CUE_RICKITY;
	the_jester->frame = 0;
	play_rotating_cue(1, "jesricky");
}

void jester_cue_bigtop(void)
{
	float	unused[3];		/* the original's frame holds a local it never uses */

	the_jester->ai_state = JESTER_CUE_BIGTOP;
	the_jester->frame = 0;
	play_rotating_cue(0x38, "jesbig");
}

/* 0x8010943c */
int jester_shattered(void)
{
	return the_jester->ai_state == JESTER_SHATTERED;
}

/* 0x80109454: the name plate over his health bar: texture 0x7088, 96 by 32 at (x, y) */
static void draw_jester_nameplate(float x, float y)
{
	MidVertex	a, b, c, d;

	guTexSource(find_surface(0x7088)->handle);
	guColorCombineFunction(4);
	guAlphaSource(2);
	grAlphaTestReferenceValue(4);
	a.x = x - 48.0f;
	a.y = y - 16.0f;
	a.oow = 0.90909088f;
	a.sow = 0.0f;
	a.tow = 57.272724f;
	b.x = x + 48.0f;
	b.y = y - 16.0f;
	b.oow = 0.90909088f;
	b.sow = 231.81818f;
	b.tow = 57.272724f;
	c.x = x - 48.0f;
	c.y = y + 16.0f;
	c.oow = 0.90909088f;
	c.sow = 0.0f;
	c.tow = 0.0f;
	d.x = x + 48.0f;
	d.y = y + 16.0f;
	d.oow = 0.90909088f;
	d.sow = 231.81818f;
	d.tow = 0.0f;
	func_800d326c(&a, &d, &b);
	func_800d326c(&a, &c, &d);
}

/* 0x80109564: a dance cue every 61 frames, the fifteen in turn */
static void jester_dance_sound(void)
{
	jester_sound_timer++;
	if(jester_sound_timer >= 0x3d)
	{
		jester_sound_timer = 0;
		play_jesbig2_sound(jester_dance_cues[jester_dance_cue], 0);
		jester_dance_cue = (jester_dance_cue + 1) % 15;
	}
}

/* 0x80109604: the flinch's cue, the fourteen in turn */
static void jester_hit_sound(void)
{
	jester_sound_timer = 0;
	play_jesbig2_sound(jester_hit_cues[jester_hit_cue], 2);
	jester_hit_cue = (jester_hit_cue + 1) % 14;
}
