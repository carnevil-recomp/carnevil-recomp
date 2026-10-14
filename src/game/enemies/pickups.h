/*
 * pickups.h - the weapon and health pickups (src/game/enemies/pickup_*.c): six original files
 * of one shape at 0x801026b0-0x80105388. A pickup is a spawn-table object whose record is an
 * enemy_t with one group: it spins in place until shot, then gives the shooter its reward,
 * shrinks away over 16 frames in a light of the player's colour and is removed.
 */
#ifndef PICKUPS_H
#define PICKUPS_H

#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "texmap.h"
#include "credits.h"			/* player_stats_t, debug_mode */

#define PICKUP_WAITING		0x8000		/* ai_state: spinning, can be shot */
#define PICKUP_TAKEN		0x4000		/* shot: shrinking away */
#define PICKUP_SHRINK_FRAMES	16

/* the message the HUD shows for a pickup (show_pickup_message's kind, player.c's switch) */
#define PICKUP_MSG_HEALTH	1		/* "HEALTH UP" */
#define PICKUP_MSG_SHOTGUN	2		/* "SHOTGUN X 10" */
#define PICKUP_MSG_CLIP		4		/* "INCREASE CLIP" */
#define PICKUP_MSG_MACHINEGUN	6		/* "MACHINE GUN" */
#define PICKUP_MSG_FLAMETHROWER	7		/* "FLAMETHROWER" */
#define PICKUP_MSG_ACID		8		/* "ACID BATH" */

#define DEBUG_CHARACTERS_BIT	0x10000		/* waves.h DEBUG_CHARACTERS: a shot selects the object instead */

extern void	show_pickup_message(player_stats_t *, int);		/* player.c's file: show a pickup message (kind, full alpha) */
extern void	set_weapon(player_stats_t *, int);		/* shoot.c */
extern int	increment_audit(int);
extern void	award_points(int, int);
extern void	play_rotating_cue(int, char *);
extern int	remove_die();					/* spawn.c: (id, killer) */
extern void	set_enemy_scale(enemy_t *, float);
extern int	shot_hits_sphere(float *, float, player_stats_t *);
extern void	point_toward_camera(float *, float, float *);
extern int	add_dynamic_light(int, float *, float *, float, int);
extern int	remove_light(int);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_at_color(int, char *, int, unsigned int);
extern void	func_800c8938(float *, float, unsigned int);		/* debug sphere */
extern void	func_800d14a4(float, float);				/* debug map marker */
extern void	func_800c99e4(enemy_t *, int);				/* debug group frames */
extern int	load_model(void **, int, void **, int, void **, int, char *, int);
extern void	remove_model(void **, int, void **, int, void **, int, char *);
extern enemy_t	*debug_enemy;
extern char	text_buffer[];

/* pickups_data.c */
extern texmap_t	shotgun_texmap[2], health_texmap[1], ammo_texmap[2], acid_texmap[1], machinegun_texmap[2], flamethrower_texmap[2];
extern float	health_pulse[21];
extern submesh_t shotgun_submesh[1], health_submesh[1], ammo_submesh[1], acid_submesh[1], machinegun_submesh[1], flamethrower_submesh[1];

#endif
