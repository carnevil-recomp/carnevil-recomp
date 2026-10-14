/*
 * effects.h - the effects attached to an enemy (blood, sparks, a part flying off): a list of
 * records on enemy_t.effects, each with its own draw, think and remove callbacks.
 */
#ifndef EFFECTS_H
#define EFFECTS_H

#include "enemy.h"
#include "model.h"

typedef struct effect
{
	int		type;			/* 0x00 */
	void		(*draw)(struct effect *, void *, struct group *);	/* 0x04: (the enemy's list, param, group) */
	void		(*think)(enemy_t *, void *);				/* 0x08 */
	void		(*remove)(enemy_t *, void *);				/* 0x0c */
	struct group	*group;			/* 0x10: the group it rides */
	void		*param;			/* 0x14: its own record */
	struct effect	*next;			/* 0x18 */
} effect_t;					/* sizeof = 0x1c */

void	attach_effect(enemy_t *e, effect_t *ef);
void	draw_effects(enemy_t *e);
void	think_effects(enemy_t *e);
void	free_effects(enemy_t *e);
void	detach_effect(enemy_t *e, group_t *g);

#endif
