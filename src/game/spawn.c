/*
 * spawn.c - the level's spawn table and die list: one original file, 0x800e9140-0x800ea8e8
 * (construct_and_spawn_object at 0x800e9570, the 3.8 KB per-type dispatcher, is not yet
 * decompiled). See spawn.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include "spawn.h"
#include "objects.h"
#include "credits.h"			/* player_stats, debug_mode */

extern spawn_t	hhouse_spawn_blob[], freakshow_spawn_blob[], rickity_spawn_blob[], bigtop_spawn_blob[];
extern int	D_802373fc;			/* adjustment 0x30: the difficulty */
extern int	D_801e0f0c;

spawn_t		spawn_table[MAX_SPAWN];		/* 0x801e1790 */
spawn_t		*spawn_struct_g;		/* 0x801de888 */
int		num_enemies_in_wave_g;		/* 0x801e7048 */
int		spawn_enabled = 0;		/* 0x8018db8c */
int		level_wave_id;			/* 0x8021f0fc */
int		enemies_alive;			/* 0x801e6b6c */
int		wave_lock;			/* 0x801e6e70 */
die_t		die_list_head;			/* 0x801e7068 */

/* 0x800e9140: spawn the table entries due on this frame, then expire the die list */
void spawn_wave_enemies(int frame)
{
	spawn_t	*w;
	int	i;
	short	thr;

	if(!spawn_enabled)
	{
		return;
	}
	for(i = 0; i < num_enemies_in_wave_g; i++)
	{
		w = &spawn_table[i];
		if(w->frame != frame)
		{
			continue;
		}
		if(w->count <= 0)
		{
			continue;
		}
		if((player_stats[0].state & 4) && (player_stats[1].state & 4))
		{
			thr = w->thr2;
		}
		else
		{
			thr = w->thr1;
		}
		if(D_802373fc < thr)
		{
			w->count--;
			continue;
		}
		if(wave_lock + 1 < w->min_lock)
		{
			continue;
		}
		if(level_wave_id == 0x1020000 && i == 7)
		{
			if(!wave_slot_done(9))
			{
				continue;
			}
			if(!wave_slot_done(10))
			{
				continue;
			}
			construct_and_spawn_object(w, 7);
		}
		else
		{
			construct_and_spawn_object(w, i);
		}
	}
	die_check_outdated(frame);
}

/* 0x800e92bc: pick a level's spawn list and copy it into the table (the attract waves keep
   the previous list) */
void load_level_spawn(int wave)
{
	int	i;

	if(debug_mode & 0x400)
	{
		printf("Loading spawn level 0x%08x\n", wave);
	}
	enemies_alive = 0;
	if(!(wave & 0x1000000))
	{
		spawn_enabled = 0;
		return;
	}
	level_wave_id = wave;
	switch(wave)
	{
	case 0x1060000:
		spawn_struct_g = freakshow_spawn_blob;
		num_enemies_in_wave_g = 0xbd;
		break;
	case 0x1040000:
		spawn_struct_g = rickity_spawn_blob;
		num_enemies_in_wave_g = 0x87;
		break;
	case 0x1070000:
		spawn_struct_g = bigtop_spawn_blob;
		num_enemies_in_wave_g = 0x96;
		break;
	default:
		printf("Warning - Unknown wave 0x%08X in load_level_spawn\n", wave);
	case 0x1020000:
		spawn_struct_g = hhouse_spawn_blob;
		num_enemies_in_wave_g = 0xd1;
		break;
	case 0xa0000:
	case 0xb0000:
	case 0xc0000:
		break;
	}
	if(num_enemies_in_wave_g > MAX_SPAWN)
	{
		printf("Error, more enemies in this wave than MAX_SPAWN!\n");
	}
	D_801e0f0c = 0xffff;
	for(i = 0; i < MAX_SPAWN; i++)
	{
		spawn_table[i] = spawn_struct_g[i];
	}
	spawn_enabled = 1;
}

/* 0x800e94dc: restore the table from the level's list (for a replay) */
void reload_spawn_table(void)
{
	int	i;

	for(i = 0; i < MAX_SPAWN; i++)
	{
		spawn_table[i] = spawn_struct_g[i];
	}
}

/* 0x800e9560 */
void disable_spawning(void)
{
	spawn_enabled = 0;
}

/* 0x800ea484: an enemy was killed: drop its die entry and the game object; in debug mode
   (bit 0) only the object. Returns 0 */
int remove_die(int id)
{
	die_t	*d, *prev, *next;
	int	found;

	found = 0;
	if(debug_mode & 1)
	{
		remove_game_object(id);
		return(0);
	}
	d = die_list_head.next;
	prev = &die_list_head;
	if(d)
	{
		do
		{
			next = d->next;
			if(d->id == id)
			{
				enemies_alive--;
				remove_game_object(d->id);
				free(d);
				d = next;
				found = 1;
				prev->next = d;
			}
			else
			{
				prev = d;
				d = next;
			}
		} while(d && !found);
	}
	return(0);
}

/* 0x800ea55c: flag an enemy's die entry (2 = marked) */
int mark_die(int id)
{
	die_t	*d;
	int	found;

	found = 0;
	d = die_list_head.next;
	if(d)
	{
		do
		{
			if(d->id == id)
			{
				d->flag = 2;
				found = 1;
			}
			else
			{
				d = d->next;
			}
		} while(d && !found);
	}
	return(0);
}

/* 0x800ea5a0: 1 when a spawn table slot is used up and none of its enemies is still
   pending on the die list */
int wave_slot_done(int index)
{
	die_t	*d, *next;
	int	done;

	done = 1;
	if(spawn_table[index].count > 0)
	{
		if((debug_mode & 0x400) && wave_lock >= 31)
		{
			printf("Zdis check %d...more to go\n", index);
		}
		return(0);
	}
	d = die_list_head.next;
	if(d)
	{
		do
		{
			next = d->next;
			if(d->master == index && d->flag == 1)
			{
				done = 0;
			}
			d = next;
		} while(d);
	}
	return(done);
}

/* 0x800ea660 */
void reset_wave_lock(void)
{
	wave_lock = 0;
}

/* 0x800ea670: queue an enemy to die on a frame; 1 when out of memory */
int add_die(short id, int master, int frame)
{
	die_t	*d;

	d = malloc(sizeof(die_t));
	if(!d)
	{
		return(1);
	}
	d->id = id;
	d->master = master;
	d->flag = 1;
	d->frame = frame;
	d->next = die_list_head.next;
	die_list_head.next = d;
	return(0);
}

/* 0x800ea6e4: queue an enemy to die with its master's frame */
void add_die_like(short id, short master)
{
	die_t	*d, *m;

	m = die_list_head.next;
	if(m)
	{
		while(m->id != master)
		{
			m = m->next;
			if(!m)
			{
				break;
			}
		}
	}
	if(!m)
	{
		printf("Could not find master id %d in zdie list.\n", master);
		return;
	}
	d = malloc(sizeof(die_t));
	if(!d)
	{
		return;
	}
	d->master = -1;
	d->id = id;
	d->flag = 1;
	d->frame = m->frame;
	d->next = die_list_head.next;
	die_list_head.next = d;
}

/* 0x800ea7a4: remove the enemies whose die frame has passed */
void die_check_outdated(int frame)
{
	die_t	*d, *prev, *next;

	d = die_list_head.next;
	prev = &die_list_head;
	while(d)
	{
		next = d->next;
		if(frame < d->frame)
		{
			prev = d;
		}
		else
		{
			if(d->id != -7)
			{
				enemies_alive--;
				remove_game_object(d->id);
			}
			else
			{
				printf("zdis_check_outdated bad id!\n");
			}
			free(d);
			prev->next = next;
		}
		d = next;
	}
}

/* 0x800ea870: free the whole die list */
void free_die_list(void)
{
	die_t	*d, *next;

	d = die_list_head.next;
	while(d)
	{
		next = d->next;
		free(d);
		d = next;
		die_list_head.next = d;
	}
	if(debug_mode & 0x400)
	{
		printf("killed zdie\n");
	}
}
