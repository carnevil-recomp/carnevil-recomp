/*
 * weapons.h - the enemies' weapons (src/game/enemies/weapons.c): the things an enemy holds in a
 * hand group and the things it throws at the player. A kind number picks the model; the
 * held and the thrown drawers both switch on it.
 */
#ifndef WEAPONS_H
#define WEAPONS_H

#include "model.h"
#include "texmap.h"
#include "effects.h"

/* the kinds (the gaps draw the bone when held, the knife when thrown) */
#define WEAPON_KNIFE		0x01		/* Haunted House: KNIF.ZM */
#define WEAPON_HAT		0x02		/* HATT.ZM */
#define WEAPON_BONE		0x03		/* BONE.ZM */
#define WEAPON_SWORD		0x10		/* Freak Show: SWRD.ZM */
#define WEAPON_HOOK		0x11		/* HOOK.ZM */
#define WEAPON_BLADE		0x12		/* BLAD.ZM */
#define WEAPON_BAT		0x13		/* FBAT.ZM */
#define WEAPON_PIN		0x14		/* FPIN.ZM */
#define WEAPON_RATTLE		0x15		/* RATL.ZM */
#define WEAPON_FOOD		0x20		/* Rickety Town: the four foods in FOOD.ZM, 0x20-0x23 */
#define WEAPON_PRESENT		0x24		/* PRES.ZM */
#define WEAPON_COAL		0x25		/* COAL.ZM, lit by its own light when thrown */
#define WEAPON_BTOP_BONE	0x30		/* Big Top: the two parts of BTBONE.ZM, 0x30 and 0x31 */
#define WEAPON_SKELETON		0x32		/* the skeleton's own model */
#define WEAPON_UNK33		0x33		/* the model at 0x8019fcf0 */
#define WEAPON_MISSILE		0x34		/* the two parts of MISL.ZM, 0x34 and 0x35: they explode when shot */
#define WEAPON_WIG		0x40		/* Haunted House: the five wigs in WIGS.ZM, 0x40-0x44 */

/* a thrown object's states (enemy_t.ai_state) */
#define THROWN_LOBBED		0x100		/* an arc that lands near the camera */
#define THROWN_SHOT		0x200		/* knocked away by a shot, tumbling and falling */
#define THROWN_HEAVY		0x400		/* the bat, pin and rattle: faster, a wider reach, worth double */
#define THROWN_STRAIGHT		0x800		/* straight at the camera */
#define THROWN_AIMED		0x1000		/* straight at a point */

/* a held object (0x40 bytes): where it sits in the hand group */
typedef struct held
{
	float	off[3];			/* 0x00: offset in the group */
	float	mat[9];			/* 0x0c: its rotation in the group */
	float	scale[3];		/* 0x30 */
	int	kind;			/* 0x3c */
} held_t;

extern held_t	*last_held_object;	/* 0x80194650: the debug overlay nudges it */

/* weapons_data.c */
extern texmap_t	hhouse_enemy_bone_texmap[1], hhouse_enemy_knife_texmap[2], hhouse_enemy_hat_texmap[1], wigs_texmap[10];
extern texmap_t	freakshow_enemy_sword_texmap[2], freakshow_enemy_club_texmap[2], freakshow_enemy_hook_texmap[2];
extern texmap_t	freakshow_enemy_blade_texmap[2], freakshow_enemy_pin_texmap[2], freakshow_enemy_rattle[3];
extern texmap_t	rickitytown_food_texmap[8], rickitytown_present_texmap[4], coal_texmap[1];
extern texmap_t	btop_bone_texmap[3], missile_texmap[20];
extern submesh_t bone_submesh[1], knife_submesh[1], hat_submesh[1], wig_submesh[5];
extern submesh_t sword_submesh[1], bat_submesh[1], hook_submesh[1], blade_submesh[1], pin_submesh[1], rattle_submesh[1];
extern submesh_t food_submesh[4], present_submesh[1], coal_submesh[1], btop_bone_submesh[2], missile_submesh[7];

void	throw_at_camera(float *pos, int kind, float *scale);
void	throw_volley(float *pos, int kind, float *scale);
void	throw_at(float *pos, int kind, float *scale, float *target);
void	throw_heavy(float *pos, int kind, float *scale);
void	lob_at_camera(float *pos, int kind, float *scale, int flags);
void	set_thrown_object_state(enemy_t *e, int state);
effect_t *attach_held_object(group_t *g, int kind, float x, float y, float z, float rx, float ry, float rz, float *scale);
effect_t *attach_held_object_matrix(group_t *g, int kind, float x, float y, float z, float *mat, float *scale);
void	draw_held_object(group_t *g, int kind, float *off, float *mat, float *scale);

void	load_hhouse_enemy_weap_texmaps(void), load_hhouse_enemy_weap_models(void), remove_hhouse_enemy_weaps(void);
void	load_freakshow_enemy_weap_texmaps(void), load_freakshow_enemy_weap_models(void), remove_freakshow_weaps(void);
void	load_rickity_object_texmaps(void), load_junk(void), remove_junk(void);
void	load_btop_weap_texmaps(void), load_btop_weap_models(void), remove_btop_weaps(void);

#endif
