/*
 * weapons.c - the enemies' weapons: one original file (0x8010d818-0x80110270, tables at
 * 0x80193ad0-0x80194650, weapons_data.c).
 *
 * A held object is an effect (effects.h, type 4) attached to an enemy's hand group: a
 * model drawn at an offset and rotation inside that group. A thrown object is a game object
 * of type 0x1900 whose record is an enemy_t with one group: it flies at the camera (or at a
 * point, or in an arc), hurts the player when it gets within reach, and can be shot, which
 * knocks it off to the side for points. Its velocity lives in the enemy record's goal slot,
 * its life in the counter, its kind at +0xa8 and its light (the coal) at +0xd0. The kinds
 * and the loaders are in weapons.h: each level loads the weapons its enemies use.
 */
#include <stdlib.h>
#include <glide/glide.h>
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "texmap.h"
#include "effects.h"
#include "credits.h"			/* player_stats_t, debug_mode */
#include "levelsnd.h"
#include "lights.h"
#include "camera.h"
#include "waves.h"
#include "weapons.h"

extern player_stats_t *get_target_player(void);			/* objects.c */
extern player_stats_t *attack_target(enemy_t *);			/* objects.c: the player it hits */
extern void	set_enemy_scale(enemy_t *, float);			/* objects.c */
extern void	point_toward_camera(float *, float, float *);		/* objects.c */
extern int	shot_hits_sphere(float *, float, player_stats_t *);	/* objects.c */
extern void	hurt_player(player_stats_t *, int, int, float);		/* player.c */
extern void	award_points(int, int);
extern float	get_screen_pos(float *, float *);			/* vecmat.c */
extern void	rot_matrix_zyx(float *, float *), mat_mul(float *, float *, float *), mat_copy(float *, float *);
extern void	build_light_list(float *);				/* lights.c */
extern void	debug_move_point(float *);				/* scene.c */
extern void	draw_marker(float *, float, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);		/* the debug sphere */
extern void	func_800d14a4(float, float);				/* the debug map marker */
extern float	func_800fe8dc(float);					/* effects.c: a random float in +-range */
extern void	add_explosion(float *);					/* shake.c's file */
extern int	near_camera(enemy_t *, float);				/* enemy.c */
extern void	aim_at_camera(enemy_t *, float, float *), aim_at_point(enemy_t *, float, float *, float *);
extern void	knock_sideways(enemy_t *, float), lob_toward(float *, float *, float *, float, int);
extern void	set_goal_near_camera(enemy_t *, float *, float *, float, float, float);
extern void	group_point(group_t *, float *);
extern void	draw_submesh_at(group_t *, submesh_t *, float *, int, face_t *, int, float *, int, float *, float *, float *);	/* model.c */
extern int	load_model(void **, int, void **, int, void **, int, char *, int);
extern void	remove_model(void **, int, void **, int, void **, int, char *);

/* the other enemies' models the held drawer can show */
extern submesh_t skeleton_anim_blob[], submesh_info_clown[];	/* clown_data.c: part 9 is the mallet (0x8019fcf0) */
extern float	*D_801e5b28, *D_801e57e4, *clown_verts, *clown_normals;
extern face_t	*D_8021eec8, *clown_faces;

/* the weapon models */
float	*bone_verts, *bone_normals;		/* 0x801e5878, 0x801e15a0 */
face_t	*bone_faces;				/* 0x80212df8 */
float	*knife_verts, *knife_normals;		/* 0x801e5924, 0x801e1780 */
face_t	*knife_faces;				/* 0x80212e64 */
float	*hat_verts, *hat_normals;		/* 0x801e5b44, 0x801e5800 */
face_t	*hat_faces;				/* 0x8021efdc */
float	*wigs_verts, *wigs_normals;		/* 0x801e5bb8, 0x801e5884 */
face_t	*wigs_faces;				/* 0x8022b108 */
float	*sword_verts, *sword_normals;		/* 0x801e5bd8, 0x801e5b10 */
face_t	*sword_faces;				/* 0x8022b190 */
float	*bat_verts, *bat_normals;		/* 0x801e5810, 0x801e0f34 */
face_t	*bat_faces;				/* 0x80212dd8 */
float	*hook_verts, *hook_normals;		/* 0x801e5b48, 0x801e5804 */
face_t	*hook_faces;				/* 0x8021efe0 */
float	*blade_verts, *blade_normals;		/* 0x801e1788, 0x801e0ef0 */
face_t	*blade_faces;				/* 0x80212d9c */
float	*pin_verts, *pin_normals;		/* 0x801e5b20, 0x801e57cc */
face_t	*pin_faces;				/* 0x80212e98 */
float	*rattle_verts, *rattle_normals;		/* 0x801e5b68, 0x801e581c */
face_t	*rattle_faces;				/* 0x8021f07c */
float	*food_verts, *food_normals;		/* 0x801e5928, 0x801e1784 */
face_t	*food_faces;				/* 0x80212e68 */
float	*present_verts, *present_normals;	/* 0x801e5bbc, 0x801e5888 */
face_t	*present_faces;				/* 0x8022b10c */
float	*coal_verts, *coal_normals;		/* 0x801e5828, 0x801e0f48 */
face_t	*coal_faces;				/* 0x80212de0 */
float	*btop_bone_verts, *btop_bone_normals;	/* 0x80212d84, 0x801facc4 */
face_t	*btop_bone_faces;			/* 0x801e0ea0 */
float	*missile_verts, *missile_normals;	/* 0x801e5b8c, 0x801e584c */
face_t	*missile_faces;				/* 0x8021f0cc */

held_t	*last_held_object = 0;			/* 0x80194650 (.data) */

static void	draw_thrown_object(game_object_t *);
static int	init_thrown_object(game_object_t *);
static void	thrown_object_think(game_object_t *);
static int	thrown_object_hit(game_object_t *, player_stats_t *);
static int	thrown_object_die(game_object_t *, player_stats_t *);
static void	remove_thrown_object(game_object_t *);
static void	held_object_draw(effect_t *, void *, group_t *);
static void	held_object_think(enemy_t *, void *);
static void	held_object_remove(enemy_t *, void *);

#define THROWN_LIFE	0x78		/* frames before an unshot object is removed */
#define THROWN_SPEED	3.0f		/* its largest velocity component */
#define THROWN_REACH	4.5f		/* how near the camera it must come to hurt */
#define HURT_WEAPON	3		/* hurt_player kinds: a weapon ... */
#define HURT_FOOD	11		/* ... and a food (its own hit mark) */

/* the coal's light: red, falloff 6, 15 frames, a unit nearer the camera than the coal */
#define LIGHT_COAL(e, p, colour) \
	colour[0] = 255.0f; \
	colour[1] = 0.0f; \
	colour[2] = 0.0f; \
	point_toward_camera(&e->x, 1.0f, p); \
	e->unkd0 = add_dynamic_light(1, p, colour, 6.0f, 15);

/* 0x8010d818: throw a kind at the camera from pos */
void throw_at_camera(float *pos, int kind, float *scale)
{
	enemy_t	*e;
	float	colour[3], at[3];
	float	*p;
	short	id;

	id = add_object1_wrapper(0x1900, draw_thrown_object, init_thrown_object, thrown_object_think);
	if(id == 0)
	{
		return;
	}
	e = get_object_data(id);
	e->x = pos[0];
	e->y = pos[1];
	e->z = pos[2];
	e->ai_state = THROWN_STRAIGHT;
	e->unka8 = kind;
	e->scale[0] = scale[0];
	e->scale[1] = scale[1];
	e->scale[2] = scale[2];
	e->target = get_target_player();
	aim_at_camera(e, THROWN_SPEED, e->goal);
	e->counter = THROWN_LIFE;
	if(kind == WEAPON_COAL)
	{
		p = at;
		LIGHT_COAL(e, p, colour)
	}
}

/* 0x8010d944: six of a kind, each at a point up to 5 units off the camera; only the first
   coal gets a light */
void throw_volley(float *pos, int kind, float *scale)
{
	enemy_t	*e;
	float	colour[3], goal[3];
	float	side, dy;
	short	id;
	int	i;

	for(i = 0; i < 6; i++)
	{
		id = add_object1_wrapper(0x1900, draw_thrown_object, init_thrown_object, thrown_object_think);
		if(id == 0)
		{
			return;
		}
		e = get_object_data(id);
		e->x = pos[0];
		e->y = pos[1];
		e->z = pos[2];
		e->ai_state = THROWN_STRAIGHT;
		e->unka8 = kind;
		e->scale[0] = scale[0];
		e->scale[1] = scale[1];
		e->scale[2] = scale[2];
		e->target = get_target_player();
		side = func_800fe8dc(5.0f);
		dy = func_800fe8dc(5.0f);
		set_goal_near_camera(e, camera_pos, goal, side, dy, 0.0f);
		aim_at_point(e, THROWN_SPEED, e->goal, goal);
		e->counter = THROWN_LIFE;
		if(kind == WEAPON_COAL)
		{
			if(i == 0)
			{
				LIGHT_COAL(e, goal, colour)
			}
		}
	}
}

/* 0x8010daf0: throw a kind at a point; it reaches 20 units */
void throw_at(float *pos, int kind, float *scale, float *target)
{
	enemy_t	*e;
	short	id;

	id = add_object1_wrapper(0x1900, draw_thrown_object, init_thrown_object, thrown_object_think);
	if(id == 0)
	{
		return;
	}
	e = get_object_data(id);
	e->x = pos[0];
	e->y = pos[1];
	e->z = pos[2];
	e->ai_state = THROWN_AIMED;
	e->unka8 = kind;
	e->unkd0 = -1;
	e->scale[0] = scale[0];
	e->scale[1] = scale[1];
	e->scale[2] = scale[2];
	e->target = get_target_player();
	aim_at_point(e, THROWN_SPEED, e->goal, target);
	e->counter = THROWN_LIFE;
}

/* 0x8010dbe4: the heavy throw (the Freak Show's bat, pin and rattle, each with its own hit
   radius): faster, longer lived and reaching 15 units */
void throw_heavy(float *pos, int kind, float *scale)
{
	enemy_t	*e;
	short	id;

	id = add_object1_wrapper(0x1900, draw_thrown_object, init_thrown_object, thrown_object_think);
	if(id == 0)
	{
		return;
	}
	e = get_object_data(id);
	if(kind == WEAPON_BAT)
	{
		e->unkcc = 0.35f;
	}
	if(kind == WEAPON_PIN)
	{
		e->unkcc = 0.5f;
	}
	if(kind == WEAPON_RATTLE)
	{
		e->unkcc = 0.6f;
	}
	e->x = pos[0];
	e->y = pos[1];
	e->z = pos[2];
	e->ai_state = THROWN_HEAVY;
	e->unka8 = kind;
	e->scale[0] = scale[0];
	e->scale[1] = scale[1];
	e->scale[2] = scale[2];
	e->radius = e->unkcc * scale[1];
	e->target = get_target_player();
	aim_at_camera(e, 5.0f, e->goal);
	e->counter = 0x8c;
}

/* 0x8010dd10: lob a kind in an arc that comes down 8 units short of the camera, up to 2
   units to either side, over 12 frames */
void lob_at_camera(float *pos, int kind, float *scale, int flags)
{
	enemy_t	*e;
	float	goal[3];
	float	side;
	short	id;

	id = add_object1_wrapper(0x1900, draw_thrown_object, init_thrown_object, thrown_object_think);
	if(id == 0)
	{
		return;
	}
	e = get_object_data(id);
	e->x = pos[0];
	e->y = pos[1];
	e->z = pos[2];
	e->ai_state = THROWN_LOBBED;
	e->unka8 = kind;
	e->scale[0] = scale[0];
	e->scale[1] = scale[1];
	e->scale[2] = scale[2];
	e->target = get_target_player();
	side = func_800fe8dc(2.0f);
	set_goal_near_camera(e, camera_pos, goal, side, 0.0f, -8.0f);
	lob_toward(&e->x, goal, e->goal, 12.0f, flags);
	e->counter = THROWN_LIFE;
}

/* 0x8010de34: draw a thrown object: its kind's model (a knife for a kind without one), the
   coal's light moved along with it and a flickering red marker over it */
static void draw_thrown_object(game_object_t *o)
{
	enemy_t		*e;
	float		p[3];
	float		unused[10];		/* the original's frame is this much larger: a local it never used */

	e = (enemy_t *)o->data;
	switch(e->unka8)
	{
	case WEAPON_PIN:
		draw_model(e, pin_submesh, 1, pin_verts, 0x13, pin_faces, 0x1d, pin_normals, 0x22);
		break;
	case WEAPON_RATTLE:
		draw_model(e, rattle_submesh, 1, rattle_verts, 0x71, rattle_faces, 0xbd, rattle_normals, 0xac);
		break;
	case WEAPON_BLADE:
		draw_model(e, blade_submesh, 1, blade_verts, 0x13, blade_faces, 0x22, blade_normals, 0x14);
		break;
	case WEAPON_HOOK:
		draw_model(e, hook_submesh, 1, hook_verts, 0x18, hook_faces, 0x27, hook_normals, 0x2e);
		break;
	case WEAPON_BAT:
		draw_model(e, bat_submesh, 1, bat_verts, 0x1f, bat_faces, 0x21, bat_normals, 0x1a);
		break;
	case WEAPON_SWORD:
		draw_model(e, sword_submesh, 1, sword_verts, 0xf, sword_faces, 0x15, sword_normals, 0x14);
		break;
	case WEAPON_FOOD:
		draw_model(e, &food_submesh[0], 1, food_verts, 0x92, food_faces, 0x10f, food_normals, 0x95);
		break;
	case WEAPON_FOOD + 1:
		draw_model(e, &food_submesh[1], 1, food_verts, 0x92, food_faces, 0x10f, food_normals, 0x95);
		break;
	case WEAPON_FOOD + 2:
		draw_model(e, &food_submesh[2], 1, food_verts, 0x92, food_faces, 0x10f, food_normals, 0x95);
		break;
	case WEAPON_FOOD + 3:
		draw_model(e, &food_submesh[3], 1, food_verts, 0x92, food_faces, 0x10f, food_normals, 0x95);
		break;
	case WEAPON_PRESENT:
		draw_model(e, present_submesh, 1, present_verts, 0x35, present_faces, 0x50, present_normals, 0x35);
		break;
	case WEAPON_COAL:
		point_toward_camera(&e->x, 1.0f, p);
		move_light(e->unkd0, p);
		grDepthMask(0);
		draw_marker(&e->x, func_800fe8dc(0.75f) + 2.25f, 0xff0000);
		grDepthMask(1);
		draw_model(e, coal_submesh, 1, coal_verts, 0x1a, coal_faces, 0x30, coal_normals, 0x1a);
		break;
	case WEAPON_BTOP_BONE:
		draw_model(e, &btop_bone_submesh[1], 1, btop_bone_verts, 0x61, btop_bone_faces, 0x9c, btop_bone_normals, 0x5b);
		break;
	case WEAPON_BTOP_BONE + 1:
		draw_model(e, &btop_bone_submesh[0], 1, btop_bone_verts, 0x61, btop_bone_faces, 0x9c, btop_bone_normals, 0x5b);
		break;
	case WEAPON_SKELETON:
		draw_model(e, skeleton_anim_blob, 1, D_801e5b28, 0x184, D_8021eec8, 0x2d6, D_801e57e4, 0x210);
		break;
	case WEAPON_UNK33:
		draw_model(e, &submesh_info_clown[9], 1, clown_verts, 0x331, clown_faces, 0x5b1, clown_normals, 0x379);
		break;
	case WEAPON_MISSILE:
		draw_model(e, &missile_submesh[5], 1, missile_verts, 0x14e, missile_faces, 0x1a6, missile_normals, 0x158);
		break;
	case WEAPON_MISSILE + 1:
		draw_model(e, &missile_submesh[6], 1, missile_verts, 0x14e, missile_faces, 0x1a6, missile_normals, 0x158);
		break;
	default:
		draw_model(e, knife_submesh, 1, knife_verts, 0xf, knife_faces, 0x15, knife_normals, 0x15);
		break;
	}
	if(debug_mode & 0x100)
	{
		func_800c8938(e->hit_group->pos, e->radius, 0x600000);
	}
	if(debug_mode & 0x20)
	{
		guAlphaSource(1);
		guColorCombineFunction(1);
		grConstantColorValue(0xffffffff);
		grAlphaBlendFunction(1, 5, 1, 5);
		func_800d14a4(e->x * 0.12f + 200.0f, e->z * 0.12f + 128.0f);
		grAlphaBlendFunction(4, 0, 4, 0);
	}
}

/* 0x8010e450: a shot against the object's sphere */
static int thrown_object_hit(game_object_t *o, player_stats_t *p)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	if(shot_hits_sphere(e->hit_group->pos, e->radius * e->radius, p))
	{
		p->hit = 1;
		p->hit_id = o->id;
		p->die = (void *)thrown_object_die;
		p->hit_point[0] = p->aim[0];
		p->hit_point[1] = p->aim[1];
		p->hit_point[2] = p->aim[2];
	}
	return 0;
}

/* 0x8010e4dc: a shot landed. A missile explodes. Anything else is knocked off to the side
   away from the player, up a little and tumbling, for 600 points (2000 for a heavy throw;
   the side test reads the object's world x where the screen x was meant, so an object on
   screen goes right unless it is far to the left of the world origin); a second hit on
   an object already knocked away sends it back the other way, 1.5 times as hard */
static int thrown_object_die(game_object_t *o, player_stats_t *p)
{
	enemy_t	*e;
	float	scr[3];

	e = (enemy_t *)o->data;
	if(e->unka8 - WEAPON_MISSILE < 2u)
	{
		remove_game_object(o->id);
		add_explosion(p->hit_point);
		play_rotating_cue(1, "generic1");
		return 0;
	}
	if(e->ai_state != THROWN_SHOT)
	{
		if(get_screen_pos(&e->x, scr) == -1.0f || e->x < 200.0f)
		{
			e->unk9c = 1.0f;
		}
		else
		{
			e->unk9c = -1.0f;
		}
		if(e->ai_state == THROWN_HEAVY)
		{
			e->unka0 = 2.0f;
		}
		else
		{
			e->unka0 = 0.6f;
		}
		knock_sideways(e, e->unk9c * e->unka0);
		e->counter = 0x1e;
		e->goal[1] = 1.3f;
		award_points(p->index, (int)(e->unka0 * 1000.0f));
		e->ai_state = THROWN_SHOT;
	}
	else
	{
		e->unk9c = -e->unk9c;
		knock_sideways(e, e->unk9c * (e->unka0 = e->unka0 * 1.5f));
		e->counter = 0x1e;
		e->goal[1] = 1.3f;
		award_points(p->index, (int)(e->unka0 * 1000.0f));
	}
	return 0;
}

/* 0x8010e698: the Haunted House's weapons */
void load_hhouse_enemy_weap_texmaps(void)
{
	load_texmap(hhouse_enemy_bone_texmap, 1, "bone");
	load_texmap(hhouse_enemy_knife_texmap, 2, "knif");
	load_texmap(hhouse_enemy_hat_texmap, 1, "hatt");
	load_texmap(wigs_texmap, 10, "wigs");
}

/* 0x8010e710 */
void load_hhouse_enemy_weap_models(void)
{
	load_model((void **)&bone_verts, 0xf, (void **)&bone_faces, 0x1a, (void **)&bone_normals, 0x11, "BONE.ZM", (int)0xe87c602b);
	load_model((void **)&knife_verts, 0xf, (void **)&knife_faces, 0x15, (void **)&knife_normals, 0x15, "KNIF.ZM", (int)0x6338ad63);
	load_model((void **)&hat_verts, 0x1e, (void **)&hat_faces, 0x38, (void **)&hat_normals, 0x35, "HATT.ZM", (int)0x5b0d84b4);
	load_model((void **)&wigs_verts, 0x126, (void **)&wigs_faces, 0x1e3, (void **)&wigs_normals, 0x136, "WIGS.ZM", (int)0x3de70d5d);
}

/* 0x8010e848 */
void remove_hhouse_enemy_weaps(void)
{
	remove_surface(wigs_texmap, 10);
	remove_model((void **)&wigs_verts, 0x126, (void **)&wigs_faces, 0x1e3, (void **)&wigs_normals, 0x136, "WIGS.ZM");
	remove_surface(hhouse_enemy_hat_texmap, 1);
	remove_model((void **)&hat_verts, 0x1e, (void **)&hat_faces, 0x38, (void **)&hat_normals, 0x35, "HATT.ZM");
	remove_surface(hhouse_enemy_knife_texmap, 2);
	remove_model((void **)&knife_verts, 0xf, (void **)&knife_faces, 0x15, (void **)&knife_normals, 0x15, "KNIF.ZM");
	remove_surface(hhouse_enemy_bone_texmap, 1);
	remove_model((void **)&bone_verts, 0xf, (void **)&bone_faces, 0x1a, (void **)&bone_normals, 0x11, "BONE.ZM");
}

/* 0x8010e990: the record is an enemy with one group (placed at x = 4 until the thrower
   moves it), hit radius 0.3 at scale 1, no light */
static int init_thrown_object(game_object_t *o)
{
	enemy_t	*e;
	int	i;

	o->data = malloc(sizeof(enemy_t));
	if(o->data == 0)
	{
		return 1;
	}
	o->hit = thrown_object_hit;
	o->die = thrown_object_die;
	o->remove = remove_thrown_object;
	e = (enemy_t *)o->data;
	e->counter = 0;
	e->frame = 0;
	e->ai_state = 0;
	e->flags = 0xc;
	e->rx = 0.0f;
	e->ry = 0.0f;
	e->rz = 0.0f;
	e->x = 4.0f;
	e->y = 0.0f;
	e->z = 0.0f;
	e->groups = malloc(sizeof(group_t));
	if(e->groups == 0)
	{
		free(e);
		return 1;
	}
	for(i = 0; i < 1; i++)
	{
		e->groups[i].flags = 0x9000;
		e->groups[i].pos[0] = 0.0f;
		e->groups[i].pos[1] = 0.0f;
		e->groups[i].pos[2] = 0.0f;
		e->groups[i].pivot[0] = 0.0f;
		e->groups[i].pivot[1] = 0.0f;
		e->groups[i].pivot[2] = 0.0f;
		e->groups[i].rot[0] = 4.712388f;
		e->groups[i].rot[1] = 0.0f;
		e->groups[i].rot[2] = 0.0f;
	}
	e->unkcc = 0.3f;
	set_enemy_scale(e, 1.0f);
	e->hit_group = e->groups;
	e->hit_group->rot[0] = 0.0f;
	e->unk98 = 0.0f;
	e->ai_state = 0;
	e->unkd0 = -1;
	return 0;
}

/* 0x8010eb20: fly. A shot object tumbles, falls and times out. The others move along their
   velocity (the lob falling) and hurt the player for 200 when they come within reach (a food
   with the food hit mark); otherwise they time out */
static void thrown_object_think(game_object_t *o)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	switch(e->ai_state)
	{
	case THROWN_SHOT:
		e->ry = e->ry + 0.71f;
		e->rz = e->rz + 0.91f;
		e->x = e->x + e->goal[0];
		e->y = e->y + e->goal[1];
		e->z = e->z + e->goal[2];
		e->goal[1] = e->goal[1] + -0.22f;
		if(--e->counter == 0)
		{
			remove_game_object(o->id);
		}
		return;
	case THROWN_STRAIGHT:
		e->ry = e->ry + 0.2f;
		e->rz = e->rz + 0.21f;
		e->x = e->x + e->goal[0];
		e->y = e->y + e->goal[1];
		e->z = e->z + e->goal[2];
		if(near_camera(e, THROWN_REACH))
		{
			if(e->unka8 - WEAPON_FOOD < 4u)
			{
				hurt_player(attack_target(e), 200, HURT_FOOD, 0.0f);
			}
			else
			{
				hurt_player(attack_target(e), 200, HURT_WEAPON, 0.0f);
			}
			remove_game_object(o->id);
			return;
		}
		if(--e->counter == 0)
		{
			remove_game_object(o->id);
		}
		return;
	case THROWN_HEAVY:
		e->ry = e->ry - 0.2f;
		e->rz = e->rz - 0.21f;
		e->x = e->x + e->goal[0];
		e->y = e->y + e->goal[1];
		e->z = e->z + e->goal[2];
		if(near_camera(e, 15.0f))
		{
			hurt_player(attack_target(e), 200, HURT_WEAPON, 0.0f);
			remove_game_object(o->id);
			return;
		}
		if(--e->counter == 0)
		{
			remove_game_object(o->id);
		}
		return;
	case THROWN_AIMED:
		e->rz = e->rz + 0.4f;
		e->x = e->x + e->goal[0];
		e->y = e->y + e->goal[1];
		e->z = e->z + e->goal[2];
		if(near_camera(e, 20.0f))
		{
			hurt_player(attack_target(e), 200, HURT_WEAPON, 0.0f);
			remove_game_object(o->id);
			return;
		}
		if(--e->counter == 0)
		{
			remove_game_object(o->id);
		}
		return;
	case THROWN_LOBBED:
		e->ry = e->ry + 0.2f;
		e->rz = e->rz + 0.21f;
		e->x = e->x + e->goal[0];
		e->y = e->y + e->goal[1];
		e->z = e->z + e->goal[2];
		e->goal[1] = e->goal[1] + -0.22f;
		if(near_camera(e, THROWN_REACH))
		{
			hurt_player(attack_target(e), 200, HURT_WEAPON, 0.0f);
			remove_game_object(o->id);
			return;
		}
		if(--e->counter == 0)
		{
			remove_game_object(o->id);
		}
		return;
	}
}

/* 0x8010eea8 */
static void remove_thrown_object(game_object_t *o)
{
	enemy_t	*e;

	e = (enemy_t *)o->data;
	if(e->unkd0 >= 0)
	{
		remove_light(e->unkd0);
	}
	free(e->groups);
	free(e);
}

/* 0x8010eef4 */
void set_thrown_object_state(enemy_t *e, int state)
{
	e->ai_state = state;
}

/* 0x8010eefc: hang a kind in a group at an offset and three angles; the record becomes the
   debug overlay's nudgeable point */
effect_t *attach_held_object(group_t *g, int kind, float x, float y, float z, float rx, float ry, float rz, float *scale)
{
	effect_t	*fx;
	held_t		*h;
	float		angles[3];

	fx = malloc(sizeof(effect_t));
	if(fx == 0)
	{
		return 0;
	}
	fx->type = 4;
	fx->draw = held_object_draw;
	fx->think = held_object_think;
	fx->remove = held_object_remove;
	fx->group = g;
	h = malloc(sizeof(held_t));
	fx->param = h;
	if(h == 0)
	{
		free(fx);
		return 0;
	}
	h->kind = kind;
	h->off[0] = x;
	h->off[1] = y;
	h->off[2] = z;
	angles[0] = rx;
	angles[1] = ry;
	angles[2] = rz;
	rot_matrix_zyx(h->mat, angles);
	h->scale[0] = scale[0];
	h->scale[1] = scale[1];
	h->scale[2] = scale[2];
	last_held_object = h;
	return fx;
}

/* 0x8010f038: the same with a ready rotation matrix */
effect_t *attach_held_object_matrix(group_t *g, int kind, float x, float y, float z, float *mat, float *scale)
{
	effect_t	*fx;
	held_t		*h;

	fx = malloc(sizeof(effect_t));
	if(fx == 0)
	{
		return 0;
	}
	fx->type = 4;
	fx->draw = held_object_draw;
	fx->think = held_object_think;
	fx->remove = held_object_remove;
	fx->group = g;
	h = malloc(sizeof(held_t));
	fx->param = h;
	if(h == 0)
	{
		free(fx);
		return 0;
	}
	h->kind = kind;
	h->off[0] = x;
	h->off[1] = y;
	h->off[2] = z;
	mat_copy(mat, h->mat);
	h->scale[0] = scale[0];
	h->scale[1] = scale[1];
	h->scale[2] = scale[2];
	last_held_object = h;
	return fx;
}

/* 0x8010f144 */
static void held_object_draw(effect_t *fx, void *param, group_t *g)
{
	held_t	*h;

	h = (held_t *)param;
	draw_held_object(g, h->kind, h->off, h->mat, h->scale);
}

/* 0x8010f178: the debug overlay nudges its offset */
static void held_object_think(enemy_t *e, void *param)
{
	if(debug_mode & DEBUG_POINT)
	{
		debug_move_point(param);
	}
}

/* 0x8010f1b0 */
static void held_object_remove(enemy_t *e, void *param)
{
	free(param);
}

/* 0x8010f1d0 (wtf): draw a kind's model at an offset and rotation inside a group, lit from
   where it ends up; a kind without a model of its own shows the bone */
void draw_held_object(group_t *g, int kind, float *off, float *mat, float *scale)
{
	float	pos[3], m[9];

	pos[0] = off[0];
	pos[1] = off[1];
	pos[2] = off[2];
	group_point(g, pos);
	mat_mul(g->mat, mat, m);
	build_light_list(pos);
	switch(kind)
	{
	case WEAPON_BTOP_BONE:
		draw_submesh_at(g, &btop_bone_submesh[1], btop_bone_verts, 0x61, btop_bone_faces, 0x9c, btop_bone_normals, 0x5b, m, pos, scale);
		break;
	case WEAPON_BTOP_BONE + 1:
		draw_submesh_at(g, &btop_bone_submesh[0], btop_bone_verts, 0x61, btop_bone_faces, 0x9c, btop_bone_normals, 0x5b, m, pos, scale);
		break;
	case WEAPON_SKELETON:
		draw_submesh_at(g, skeleton_anim_blob, D_801e5b28, 0x184, D_8021eec8, 0x2d6, D_801e57e4, 0x210, m, pos, scale);
		break;
	case WEAPON_UNK33:
		draw_submesh_at(g, &submesh_info_clown[9], clown_verts, 0x331, clown_faces, 0x5b1, clown_normals, 0x379, m, pos, scale);
		break;
	case WEAPON_MISSILE:
		draw_submesh_at(g, &missile_submesh[5], missile_verts, 0x14e, missile_faces, 0x1a6, missile_normals, 0x158, m, pos, scale);
		break;
	case WEAPON_MISSILE + 1:
		draw_submesh_at(g, &missile_submesh[6], missile_verts, 0x14e, missile_faces, 0x1a6, missile_normals, 0x158, m, pos, scale);
		break;
	case WEAPON_PRESENT:
		draw_submesh_at(g, present_submesh, present_verts, 0x35, present_faces, 0x50, present_normals, 0x35, m, pos, scale);
		break;
	case WEAPON_COAL:
		draw_submesh_at(g, coal_submesh, coal_verts, 0x1a, coal_faces, 0x30, coal_normals, 0x1a, m, pos, scale);
		break;
	case WEAPON_FOOD:
		draw_submesh_at(g, &food_submesh[0], food_verts, 0x92, food_faces, 0x10f, food_normals, 0x95, m, pos, scale);
		break;
	case WEAPON_FOOD + 1:
		draw_submesh_at(g, &food_submesh[1], food_verts, 0x92, food_faces, 0x10f, food_normals, 0x95, m, pos, scale);
		break;
	case WEAPON_FOOD + 2:
		draw_submesh_at(g, &food_submesh[2], food_verts, 0x92, food_faces, 0x10f, food_normals, 0x95, m, pos, scale);
		break;
	case WEAPON_FOOD + 3:
		draw_submesh_at(g, &food_submesh[3], food_verts, 0x92, food_faces, 0x10f, food_normals, 0x95, m, pos, scale);
		break;
	case WEAPON_SWORD:
		draw_submesh_at(g, sword_submesh, sword_verts, 0xf, sword_faces, 0x15, sword_normals, 0x14, m, pos, scale);
		break;
	case WEAPON_BAT:
		draw_submesh_at(g, bat_submesh, bat_verts, 0x1f, bat_faces, 0x21, bat_normals, 0x1a, m, pos, scale);
		break;
	case WEAPON_HOOK:
		draw_submesh_at(g, hook_submesh, hook_verts, 0x18, hook_faces, 0x27, hook_normals, 0x2e, m, pos, scale);
		break;
	case WEAPON_BLADE:
		draw_submesh_at(g, blade_submesh, blade_verts, 0x13, blade_faces, 0x22, blade_normals, 0x14, m, pos, scale);
		break;
	case WEAPON_RATTLE:
		draw_submesh_at(g, rattle_submesh, rattle_verts, 0x71, rattle_faces, 0xbd, rattle_normals, 0xac, m, pos, scale);
		break;
	case WEAPON_PIN:
		draw_submesh_at(g, pin_submesh, pin_verts, 0x13, pin_faces, 0x1d, pin_normals, 0x22, m, pos, scale);
		break;
	case WEAPON_WIG:
		draw_submesh_at(g, &wig_submesh[0], wigs_verts, 0x126, wigs_faces, 0x1e3, wigs_normals, 0x136, m, pos, scale);
		break;
	case WEAPON_WIG + 1:
		draw_submesh_at(g, &wig_submesh[1], wigs_verts, 0x126, wigs_faces, 0x1e3, wigs_normals, 0x136, m, pos, scale);
		break;
	case WEAPON_WIG + 2:
		draw_submesh_at(g, &wig_submesh[2], wigs_verts, 0x126, wigs_faces, 0x1e3, wigs_normals, 0x136, m, pos, scale);
		break;
	case WEAPON_WIG + 3:
		draw_submesh_at(g, &wig_submesh[4], wigs_verts, 0x126, wigs_faces, 0x1e3, wigs_normals, 0x136, m, pos, scale);
		break;
	case WEAPON_WIG + 4:
		draw_submesh_at(g, &wig_submesh[3], wigs_verts, 0x126, wigs_faces, 0x1e3, wigs_normals, 0x136, m, pos, scale);
		break;
	case WEAPON_HAT:
		draw_submesh_at(g, hat_submesh, hat_verts, 0x1e, hat_faces, 0x38, hat_normals, 0x35, m, pos, scale);
		break;
	case WEAPON_KNIFE:
		draw_submesh_at(g, knife_submesh, knife_verts, 0xf, knife_faces, 0x15, knife_normals, 0x15, m, pos, scale);
		break;
	default:
		draw_submesh_at(g, bone_submesh, bone_verts, 0xf, bone_faces, 0x1a, bone_normals, 0x11, m, pos, scale);
		break;
	}
}

/* 0x8010fa2c: the Freak Show's weapons */
void load_freakshow_enemy_weap_texmaps(void)
{
	load_texmap(freakshow_enemy_sword_texmap, 2, "swrd");
	load_texmap(freakshow_enemy_club_texmap, 2, "fbat");
	load_texmap(freakshow_enemy_hook_texmap, 2, "hook");
	load_texmap(freakshow_enemy_blade_texmap, 2, "blad");
	load_texmap(freakshow_enemy_pin_texmap, 2, "fpin");
	load_texmap(freakshow_enemy_rattle, 3, "ratl");
}

/* 0x8010fad4 */
void load_freakshow_enemy_weap_models(void)
{
	load_model((void **)&sword_verts, 0xf, (void **)&sword_faces, 0x15, (void **)&sword_normals, 0x14, "SWRD.ZM", (int)0x4778b94a);
	load_model((void **)&bat_verts, 0x1f, (void **)&bat_faces, 0x21, (void **)&bat_normals, 0x1a, "FBAT.ZM", (int)0x555dec67);
	load_model((void **)&hook_verts, 0x18, (void **)&hook_faces, 0x27, (void **)&hook_normals, 0x2e, "HOOK.ZM", (int)0x69abcff5);
	load_model((void **)&blade_verts, 0x13, (void **)&blade_faces, 0x22, (void **)&blade_normals, 0x14, "BLAD.ZM", (int)0x694bae7a);
	load_model((void **)&pin_verts, 0x13, (void **)&pin_faces, 0x1d, (void **)&pin_normals, 0x22, "FPIN.ZM", (int)0x28c4ed27);
	load_model((void **)&rattle_verts, 0x71, (void **)&rattle_faces, 0xbd, (void **)&rattle_normals, 0xac, "RATL.ZM", (int)0x06642e62);
}

/* 0x8010fca0 */
void remove_freakshow_weaps(void)
{
	remove_model((void **)&rattle_verts, 0x71, (void **)&rattle_faces, 0xbd, (void **)&rattle_normals, 0xac, "RATL.ZM");
	remove_model((void **)&pin_verts, 0x13, (void **)&pin_faces, 0x1d, (void **)&pin_normals, 0x22, "FPIN.ZM");
	remove_model((void **)&blade_verts, 0x13, (void **)&blade_faces, 0x22, (void **)&blade_normals, 0x14, "BLAD.ZM");
	remove_model((void **)&hook_verts, 0x18, (void **)&hook_faces, 0x27, (void **)&hook_normals, 0x2e, "HOOK.ZM");
	remove_model((void **)&bat_verts, 0x1f, (void **)&bat_faces, 0x21, (void **)&bat_normals, 0x1a, "FBAT.ZM");
	remove_model((void **)&sword_verts, 0xf, (void **)&sword_faces, 0x15, (void **)&sword_normals, 0x14, "SWRD.ZM");
	remove_surface(freakshow_enemy_rattle, 3);
	remove_surface(freakshow_enemy_pin_texmap, 2);
	remove_surface(freakshow_enemy_blade_texmap, 2);
	remove_surface(freakshow_enemy_hook_texmap, 2);
	remove_surface(freakshow_enemy_club_texmap, 2);
	remove_surface(freakshow_enemy_sword_texmap, 2);
}

/* 0x8010fe84: Rickety Town's thrown things */
void load_rickity_object_texmaps(void)
{
	load_texmap(rickitytown_food_texmap, 8, "food");
	load_texmap(rickitytown_present_texmap, 4, "pres");
	load_texmap(coal_texmap, 1, "coal");
}

/* 0x8010fee4 */
void load_junk(void)
{
	load_model((void **)&food_verts, 0x92, (void **)&food_faces, 0x10f, (void **)&food_normals, 0x95, "FOOD.ZM", (int)0xb2f709c7);
	load_model((void **)&present_verts, 0x35, (void **)&present_faces, 0x50, (void **)&present_normals, 0x35, "PRES.ZM", (int)0x4be03bdb);
	load_model((void **)&coal_verts, 0x1a, (void **)&coal_faces, 0x30, (void **)&coal_normals, 0x1a, "COAL.ZM", (int)0x43d1fc09);
}

/* 0x8010ffd4 */
void remove_junk(void)
{
	remove_model((void **)&coal_verts, 0x1a, (void **)&coal_faces, 0x30, (void **)&coal_normals, 0x1a, "COAL.ZM");
	remove_model((void **)&present_verts, 0x35, (void **)&present_faces, 0x50, (void **)&present_normals, 0x35, "PRES.ZM");
	remove_model((void **)&food_verts, 0x92, (void **)&food_faces, 0x10f, (void **)&food_normals, 0x95, "FOOD.ZM");
	remove_surface(coal_texmap, 1);
	remove_surface(rickitytown_present_texmap, 4);
	remove_surface(rickitytown_food_texmap, 8);
}

/* 0x801100d0: the Big Top's weapons */
void load_btop_weap_texmaps(void)
{
	load_texmap(btop_bone_texmap, 3, "btbone");
	load_texmap(missile_texmap, 20, "misl");
}

/* 0x80110118 */
void load_btop_weap_models(void)
{
	load_model((void **)&btop_bone_verts, 0x61, (void **)&btop_bone_faces, 0x9c, (void **)&btop_bone_normals, 0x5b, "BTBONE.ZM", (int)0x92fd703c);
	load_model((void **)&missile_verts, 0x14e, (void **)&missile_faces, 0x1a6, (void **)&missile_normals, 0x158, "MISL.ZM", (int)0x8ba70a85);
}

/* 0x801101c0 */
void remove_btop_weaps(void)
{
	remove_model((void **)&missile_verts, 0x14e, (void **)&missile_faces, 0x1a6, (void **)&missile_normals, 0x158, "MISL.ZM");
	remove_surface(missile_texmap, 20);
	remove_model((void **)&btop_bone_verts, 0x61, (void **)&btop_bone_faces, 0x9c, (void **)&btop_bone_normals, 0x5b, "BTBONE.ZM");
	remove_surface(btop_bone_texmap, 3);
}
