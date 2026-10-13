/*
 * spawn.h - the level's spawn table and "die list" (src/game/spawn.c, 0x800e9140-0x800ea8e8).
 * A level's spawn list (hhouse_spawn_blob, ...) is copied into spawn_table when the level
 * loads; each frame spawn_wave_enemies() creates the entries whose frame has come. Enemies
 * that must vanish at a later frame are queued on the die list.
 */
#ifndef SPAWN_H
#define SPAWN_H

#define MAX_SPAWN	256

typedef struct spawn
{
	short		count;			/* 0x00: how many times this entry may still spawn */
	short		frame;			/* 0x02: movie frame it spawns on */
	short		unk4;
	short		min_lock;		/* 0x06: needs wave_lock + 1 >= this */
	short		thr1;			/* 0x08: difficulty needed (one player) */
	short		thr2;			/* 0x0a: difficulty needed (two players) */
	int		rest[8];		/* 0x0c: type, position, ... (construct_and_spawn_object); word aligned */
} spawn_t;					/* sizeof = 0x2c */

typedef struct die
{
	short		id;			/* game object id */
	short		flag;			/* 1 pending, 2 marked */
	int		master;			/* spawn table index it came from (-1 when copied) */
	int		frame;			/* frame it dies on */
	struct die	*next;
} die_t;

extern spawn_t	spawn_table[MAX_SPAWN];		/* 0x801e1790 */
extern spawn_t	*spawn_struct_g;		/* 0x801de888: the level's list */
extern int	num_enemies_in_wave_g;		/* 0x801e7048 */
extern int	spawn_enabled;			/* 0x8018db8c */
extern int	level_wave_id;			/* 0x8021f0fc */
extern int	enemies_alive;			/* 0x801e6b6c */
extern int	wave_lock;			/* 0x801e6e70 */
/* the list hangs off a 12-byte dummy node (so unlinking never special-cases the head) */
typedef struct die_head
{
	short		id, flag;
	int		master, frame;
	die_t		*list;			/* 0x801e7074: the first real node */
} die_head_t;
extern die_head_t	die_head;		/* 0x801e7068 */
#define die_list	die_head.list

void	spawn_wave_enemies(int frame);
void	load_level_spawn(int wave);
void	reload_spawn_table(void);
void	disable_spawning(void);
int	remove_die(int id);
int	mark_die(int id);
int	wave_slot_done(int index);
void	reset_wave_lock(void);
int	add_die(short id, int master, int frame);
void	add_die_like(short id, short master);
void	die_check_outdated(int frame);
void	free_die_list(void);
int	construct_and_spawn_object(spawn_t *s, int index);	/* 0x800e9570 */

#endif
