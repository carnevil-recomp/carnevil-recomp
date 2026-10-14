/*
 * effects.c - the effect list on an enemy (0x800fd990-0x800fdb70, part of the effects file).
 * See effects.h.
 */
#include <stdlib.h>
#include "effects.h"

/* 0x800fd990: push an effect on the front of the list */
void attach_effect(enemy_t *e, effect_t *ef)
{
	if(ef)
	{
		ef->next = e->effects;
		e->effects = ef;
	}
}

/* 0x800fd9ac: after the model: every effect's draw */
void draw_effects(enemy_t *e)
{
	effect_t *ef;

	ef = e->effects;
	while(ef)
	{
		ef->draw(e->effects, ef->param, ef->group);
		ef = ef->next;
	}
}

/* 0x800fda34: every frame: every effect's think */
void think_effects(enemy_t *e)
{
	effect_t *ef;

	ef = e->effects;
	while(ef)
	{
		ef->think(e, ef->param);
		ef = ef->next;
	}
}

/* 0x800fda88: the enemy goes: remove and free them all */
void free_effects(enemy_t *e)
{
	effect_t *ef, *next;

	ef = e->effects;
	while(ef)
	{
		next = ef->next;
		ef->remove(e, ef->param);
		free(ef);
		ef = next;
	}
}

/* 0x800fdaf0: drop the effects riding group g */
void detach_effect(enemy_t *e, group_t *g)
{
	effect_t *ef, *next, *prev;

	prev = 0;
	ef = e->effects;
	while(ef)
	{
		next = ef->next;
		if(ef->group != g)
		{
			prev = ef;
		}
		else
		{
			ef->remove(e, ef->param);
			free(ef);
			if(prev)
			{
				prev->next = next;
			}
			else
			{
				e->effects = next;
			}
		}
		ef = next;
	}
}
