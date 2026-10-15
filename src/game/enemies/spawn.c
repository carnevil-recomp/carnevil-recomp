/*
 * spawn.c - the level's spawn table and die list: one original file, 0x800e9140-0x800ea8e8
 * (construct_and_spawn_object at 0x800e9570, the per-type dispatcher, is
 * decompiled). See spawn.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include "spawn.h"
#include "objects.h"
#include "credits.h"			/* player_stats, debug_mode */
#include "waves.h"
#include "enemy.h"

extern spawn_t	hhouse_spawn_blob[], freakshow_spawn_blob[], rickity_spawn_blob[], bigtop_spawn_blob[];
extern int	difficulty;			/* 0x802373fc: adjustment 0x30 */
extern int	D_801e0f0c;

spawn_t		spawn_table[MAX_SPAWN];		/* 0x801e1790 */
spawn_t		*spawn_struct_g;		/* 0x801de888 */
int		num_enemies_in_wave_g;		/* 0x801e7048 */
int		spawn_enabled = 0;		/* 0x8018db8c */
int		level_wave_id;			/* 0x8021f0fc */
int		enemies_alive;			/* 0x801e6b6c */
int		wave_lock;			/* 0x801e6e70 */
die_head_t	die_head;			/* 0x801e7068 */

/* 0x800e9140: spawn the table entries due on this frame, then expire the die list */
void spawn_wave_enemies(int frame)
{
	spawn_t	*w;
	int	i;
	short	thr;

#ifdef PORT_HOST
	{ extern int port_trace; extern unsigned long hal_frame_count; static int last = -1;
	  if(port_trace && frame != last && frame % 10 == 0) { last = frame; fprintf(stderr, "[spawn] vblank %lu frame %d enabled %d entries %d difficulty %d lock %d\n", hal_frame_count, frame, spawn_enabled, num_enemies_in_wave_g, difficulty, wave_lock); } }
#endif
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
		if(difficulty < thr)
		{
			w->count--;
			continue;
		}
		if(wave_lock + 1 < w->min_lock)
		{
			continue;
		}
		if(level_wave_id == WAVE_HHOUSE && i == 7)		/* the house's slot 7 waits for slots 9 and 10 */
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
	spawn_t	*s;
	int	i;

	if(debug_mode & DEBUG_SPAWN_LOG)
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
	case WAVE_FREAKSHOW:
		spawn_struct_g = freakshow_spawn_blob;
		num_enemies_in_wave_g = 0xbd;
		break;
	case WAVE_RICKITY:
		spawn_struct_g = rickity_spawn_blob;
		num_enemies_in_wave_g = 0x87;
		break;
	case WAVE_BIGTOP:
		spawn_struct_g = bigtop_spawn_blob;
		num_enemies_in_wave_g = 0x96;
		break;
	default:
		printf("Warning - Unknown wave 0x%08X in load_level_spawn\n", wave);
	case WAVE_HHOUSE:
		spawn_struct_g = hhouse_spawn_blob;
		num_enemies_in_wave_g = 0xd1;
		break;
	case WAVE_ATTRACT:
	case WAVE_HISCORE_ENTRY:
	case WAVE_LEVEL_SELECT:
		break;
	}
	if(num_enemies_in_wave_g > MAX_SPAWN)
	{
		printf("Error, more enemies in this wave than MAX_SPAWN!\n");
	}
	D_801e0f0c = 0xffff;
	s = spawn_struct_g;
	for(i = 0; i < MAX_SPAWN; i++)
	{
		spawn_table[i] = *s++;
	}
	spawn_enabled = 1;
}

/* 0x800e94dc: restore the table from the level's list (for a replay) */
void reload_spawn_table(void)
{
	spawn_t	*s;
	int	i;

	s = spawn_struct_g;
	for(i = 0; i < MAX_SPAWN; i++)
	{
		spawn_table[i] = *s++;
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
	if(debug_mode & DEBUG_ON)
	{
		remove_game_object(id);
		return(0);
	}
	prev = (die_t *)&die_head;
	d = die_list;
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
	die_t	*d, *next;
	int	found;

	found = 0;
	d = die_list;
	if(d)
	{
		do
		{
			next = d->next;
			if(d->id == id)
			{
				d->flag = 2;
				found = 1;
			}
			else
			{
				d = next;
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
		if((debug_mode & DEBUG_SPAWN_LOG) && wave_lock >= 31)
		{
			printf("Zdis check %d...more to go\n", index);
		}
		return(0);
	}
	d = die_list;
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

/* 0x800ea670: queue an enemy to die on a frame; 1 when out of memory (the original has no
   return on the success path: callers ignore the value) */
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
	d->next = die_list;
	die_list = d;
#ifdef PORT_HOST
	return(0);
#endif
}

/* 0x800ea6e4: queue an enemy to die with its master's frame */
void add_die_like(short id, short master)
{
	die_t	*d, *m;

	m = die_list;
	while(m)
	{
		if(m->id == master)
		{
			break;
		}
		m = m->next;
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
	d->next = die_list;
	die_list = d;
}

/* 0x800ea7a4: remove the enemies whose die frame has passed */
void die_check_outdated(int frame)
{
	die_t	*d, *prev, *next;

	prev = (die_t *)&die_head;
	d = die_list;
	while(d)
	{
		next = d->next;
		if(frame >= d->frame)
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
		else
		{
			prev = d;
		}
		d = next;
	}
}

/* 0x800ea870: free the whole die list */
void free_die_list(void)
{
	die_t	*d, *prev, *next;

	prev = (die_t *)&die_head;
	d = die_list;
	while(d)
	{
		next = d->next;
		free(d);
		d = next;
		prev->next = d;
	}
	if(debug_mode & DEBUG_SPAWN_LOG)
	{
		printf("killed zdie\n");
	}
}

/* ---- 0x800e9570: the per-type constructor ---- */
extern int	violence_level;				/* 0x801e5bfc */
extern void	disable_accuracy_tracking(void);
extern void	set_enemy_scale(enemy_t *, float);	/* objects.c */
extern void	level_end_stats_wrapper(game_object_t *);
#define NO_INIT		((int (*)(game_object_t *))0)
#define NO_THINK	((void (*)(game_object_t *))0)
/* every enemy's draw, init and think callbacks, and its spawn-time set-up (its own file each) */
#define ENEMY(d, i, t)	extern void d(game_object_t *); extern int i(game_object_t *); extern void t(game_object_t *);
ENEMY(tort_draw, tort_init, tort_think)
ENEMY(draw_jester, init_jester, jester_think)
ENEMY(spider_draw, spider_init, spider_think)
ENEMY(gool_draw, gool_init, gool_think)
ENEMY(mari_draw, mari_init, mari_think)
ENEMY(bat_draw, bat_init, bat_think)
ENEMY(inng_draw, inng_init, inng_think)
ENEMY(hand_draw, hand_init, hand_think)
ENEMY(zombie_draw, zombie_init, zombie_think)
ENEMY(vict_draw, vict_init, vict_think)
ENEMY(spdl_draw, spdl_init, spdl_think)
ENEMY(draw_acid_pickup, init_acid_pickup, acid_pickup_think)
ENEMY(draw_health_pickup, init_health_pickup, health_pickup_think)
ENEMY(hambone_draw, hambone_init, hambone_think)
ENEMY(draw_shotgun_pickup, init_shotgun_pickup, shotgun_pickup_think)
ENEMY(draw_ammo_pickup, init_ammo_pickup, ammo_pickup_think)
ENEMY(tops_draw, tops_init, tops_think)
ENEMY(draw_machinegun_pickup, init_machinegun_pickup, machinegun_pickup_think)
ENEMY(draw_flamethrower_pickup, init_flamethrower_pickup, flamethrower_pickup_think)
ENEMY(monkey_draw, monkey_init, monkey_think)
ENEMY(grub_draw, grub_init, grub_think)
ENEMY(junior_draw, junior_init, junior_think)
ENEMY(teen_draw, teen_init, teen_think)
ENEMY(flie_draw, flie_init, flie_think)
ENEMY(rodz_draw, rodz_init, rodz_think)
ENEMY(taur_draw, taur_init, taur_think)
ENEMY(finn_draw, finn_init, finn_think)
ENEMY(term_draw, term_init, term_think)
ENEMY(dino_draw, dino_init, dino_think)
ENEMY(krinkle_draw, krinkle_init, krinkle_think)
ENEMY(rinn_draw, rinn_init, rinn_think)
ENEMY(smilingbob_draw, smilingbob_init, smilingbob_think)
ENEMY(func_801348e0, func_801352f4, func_801354fc)
ENEMY(render_enetity_debug_info, func_8013c35c, func_8013c56c)
ENEMY(krampus_draw, krampus_init, krampus_think)
ENEMY(func_80133580, func_80133d38, func_80133f28)
ENEMY(func_80131f10, func_8013281c, func_80132a14)
ENEMY(func_80138ac4, func_801394f4, func_801397f4)
ENEMY(func_80130bf8, func_801313e0, func_801315d4)
ENEMY(func_80136840, func_80136fdc, func_801371cc)
ENEMY(func_8013ea08, func_8013ef18, func_8013f0dc)
ENEMY(func_8013cfb0, func_8013d794, func_8013d990)
#define SETUP(f)	extern void f(enemy_t *, unsigned short);
SETUP(taur_setup) SETUP(spdl_setup) SETUP(mari_setup) SETUP(gool_setup) SETUP(zombie_setup)
SETUP(hand_setup) SETUP(inng_setup) SETUP(vict_setup) SETUP(monkey_setup) SETUP(hambone_setup)
SETUP(set_jester_state) SETUP(tops_setup) SETUP(tort_setup) SETUP(junior_setup) SETUP(rodz_setup)
SETUP(krampus_setup) SETUP(krinkle_setup) SETUP(flie_setup) SETUP(dino_setup) SETUP(rinn_setup)
SETUP(smilingbob_setup) SETUP(teen_setup) SETUP(func_80131a9c) SETUP(func_80134420) SETUP(func_8013cad4)
SETUP(func_80133118) SETUP(func_80135f98) SETUP(func_8013aa34) SETUP(func_8013f324) SETUP(func_8013e518)
SETUP(func_80137ae8) SETUP(term_setup) SETUP(finn_setup) SETUP(grub_setup)
extern void	bat_setup(enemy_t *, short), spider_setup(enemy_t *, short);

/* the spawn table entry becomes a game object of its type (its draw/init/think callbacks),
   gets its position, path and first state from the entry, a die-list entry, two level-specific
   scale fixes, and its type's own set-up. 1 when done, 0 for an unknown type, -1 when the
   object could not be made. Low violence swaps two types for tamer ones. */
int construct_and_spawn_object(spawn_t *s, int index)
{
	short		id;
	enemy_t		*e;
	int		type;
	void		(*draw)(game_object_t *);
	int		(*init)(game_object_t *);
	void		(*think)(game_object_t *);

#ifdef PORT_HOST
	{ extern int port_trace; extern unsigned long hal_frame_count; if(port_trace) fprintf(stderr, "[spawn] vblank %lu entry %d type %04x state %04x at (%.1f %.1f %.1f)\n", hal_frame_count, index, s->type, s->state, s->x, s->y, s->z); }
#endif
	enemies_alive++;
	if(s->type == 0x201b && violence_level <= 0)
	{
		s->type = 0x201c;
	}
	if(s->type == 0x201d && violence_level <= 0)
	{
		s->type = 0x2018;
	}
	switch(s->type)
	{
	case 0x800:
		disable_accuracy_tracking();
		type = (short)s->type;
		draw = level_end_stats_wrapper;
		init = NO_INIT;
		think = NO_THINK;
		break;
	case 0x2067:
		type = (short)s->type;
		draw = func_8013ea08;
		init = func_8013ef18;
		think = func_8013f0dc;
		break;
	case 0x2065:
		type = (short)s->type;
		draw = func_80136840;
		init = func_80136fdc;
		think = func_801371cc;
		break;
	case 0x2064:
		type = (short)s->type;
		draw = func_80130bf8;
		init = func_801313e0;
		think = func_801315d4;
		break;
	case 0x2062:
		type = (short)s->type;
		draw = func_80131f10;
		init = func_8013281c;
		think = func_80132a14;
		break;
	case 0x2063:
		type = (short)s->type;
		draw = func_801348e0;
		init = func_801352f4;
		think = func_801354fc;
		break;
	case 0x2061:
		type = (short)s->type;
		draw = func_80133580;
		init = func_80133d38;
		think = func_80133f28;
		break;
	case 0x2060:
		type = (short)s->type;
		draw = render_enetity_debug_info;
		init = func_8013c35c;
		think = func_8013c56c;
		break;
	case 0x2068:
		type = (short)s->type;
		draw = func_8013cfb0;
		init = func_8013d794;
		think = func_8013d990;
		break;
	case 0x2066:
		type = (short)s->type;
		draw = func_80138ac4;
		init = func_801394f4;
		think = func_801397f4;
		break;
	case 0x2043:
		type = (short)s->type;
		draw = rinn_draw;
		init = rinn_init;
		think = rinn_think;
		break;
	case 0x2041:
		type = (short)s->type;
		draw = krinkle_draw;
		init = krinkle_init;
		think = krinkle_think;
		break;
	case 0x2046:
		type = (short)s->type;
		draw = krampus_draw;
		init = krampus_init;
		think = krampus_think;
		break;
	case 0x2045:
		type = (short)s->type;
		draw = teen_draw;
		init = teen_init;
		think = teen_think;
		break;
	case 0x2040:
		type = (short)s->type;
		draw = dino_draw;
		init = dino_init;
		think = dino_think;
		break;
	case 0x2042:
		type = (short)s->type;
		draw = term_draw;
		init = term_init;
		think = term_think;
		break;
	case 0x2044:
		type = (short)s->type;
		draw = smilingbob_draw;
		init = smilingbob_init;
		think = smilingbob_think;
		break;
	case 0x2028:
		type = (short)s->type;
		draw = finn_draw;
		init = finn_init;
		think = finn_think;
		break;
	case 0x2020:
		type = (short)s->type;
		draw = tops_draw;
		init = tops_init;
		think = tops_think;
		break;
	case 0x2029:
		type = (short)s->type;
		draw = flie_draw;
		init = flie_init;
		think = flie_think;
		break;
	case 0x2022:
		type = (short)s->type;
		draw = monkey_draw;
		init = monkey_init;
		think = monkey_think;
		break;
	case 0x2023:
		type = (short)s->type;
		draw = grub_draw;
		init = grub_init;
		think = grub_think;
		break;
	case 0x2024:
		type = (short)s->type;
		draw = tort_draw;
		init = tort_init;
		think = tort_think;
		break;
	case 0x2027:
		type = (short)s->type;
		draw = taur_draw;
		init = taur_init;
		think = taur_think;
		break;
	case 0x2026:
		type = (short)s->type;
		draw = rodz_draw;
		init = rodz_init;
		think = rodz_think;
		break;
	case 0x2025:
		type = (short)s->type;
		draw = junior_draw;
		init = junior_init;
		think = junior_think;
		break;
	case 0x200c:
		type = (short)s->type;
		draw = zombie_draw;
		init = zombie_init;
		think = zombie_think;
		break;
	case 0x2011:
		type = (short)s->type;
		draw = inng_draw;
		init = inng_init;
		think = inng_think;
		break;
	case 0x2012:
		type = (short)s->type;
		draw = vict_draw;
		init = vict_init;
		think = vict_think;
		break;
	case 0x2005:
		type = (short)s->type;
		draw = bat_draw;
		init = bat_init;
		think = bat_think;
		break;
	case 0x1102:
		type = 0x1102;
		draw = gool_draw;
		init = gool_init;
		think = gool_think;
		break;
	case 0x2007:
		type = 0x2007;
		draw = spider_draw;
		init = spider_init;
		think = spider_think;
		break;
	case 0x2013:
		type = 0x2013;
		draw = spdl_draw;
		init = spdl_init;
		think = spdl_think;
		break;
	case 0x2016:
		type = 0x2016;
		draw = hambone_draw;
		init = hambone_init;
		think = hambone_think;
		break;
	case 0x2014:
		type = 0x2014;
		draw = draw_jester;
		init = init_jester;
		think = jester_think;
		break;
	case 0x2008:
		type = 0x2008;
		draw = hand_draw;
		init = hand_init;
		think = hand_think;
		break;
	case 0x1104:
		type = 0x1104;
		draw = mari_draw;
		init = mari_init;
		think = mari_think;
		break;
	case 0x2018:
		type = 0x2018;
		draw = draw_shotgun_pickup;
		init = init_shotgun_pickup;
		think = shotgun_pickup_think;
		break;
	case 0x2019:
		type = 0x2019;
		draw = draw_ammo_pickup;
		init = init_ammo_pickup;
		think = ammo_pickup_think;
		break;
	case 0x2017:
		type = 0x2017;
		draw = draw_health_pickup;
		init = init_health_pickup;
		think = health_pickup_think;
		break;
	case 0x201d:
		type = 0x201d;
		draw = draw_flamethrower_pickup;
		init = init_flamethrower_pickup;
		think = flamethrower_pickup_think;
		break;
	case 0x201c:
		type = 0x201c;
		draw = draw_machinegun_pickup;
		init = init_machinegun_pickup;
		think = machinegun_pickup_think;
		break;
	case 0x201b:
		type = 0x201b;
		draw = draw_acid_pickup;
		init = init_acid_pickup;
		think = acid_pickup_think;
		break;
	default:
		printf("error - trying to spawn unknown type 0x%04x\n", s->type);
		return(0);
	}
	id = add_object1_wrapper(type, draw, init, think);
	s->count--;
	if(id == 0)
	{
		return(-1);
	}
	e = get_object_data(id);
#ifdef PORT_HOST
	if(e == NULL)			/* the type's init is still a stub: nothing was allocated */
	{
		extern int port_trace;
		if(port_trace) fprintf(stderr, "[spawn] entry %d type %04x has no object data (init stubbed)\n", index, s->type);
		return(-1);
	}
#endif
	if(s->type != 0x800)
	{
		e->flags |= 0x8000;
		e->x = s->x;
		e->y = s->y;
		e->z = s->z;
		e->ry = s->ry;
		e->rz = 0;
		e->path = s->path;
		e->unk90 = s->path;
		e->unk92 = s->unk2a;
		e->unk6 = s->unk10;
		e->ai_state = s->state;
		e->num = index;
		e->unk98 = s->unk24;
	}
	add_die(id, index, s->unk4);
	if(s->type == 0x800)
	{
		return(1);
	}
	switch(level_wave_id)
	{
	case WAVE_HHOUSE:
		if(index == 0x47)
		{
			set_enemy_scale(e, 1.3f);
		}
		break;
	case WAVE_FREAKSHOW:
		break;
	case WAVE_BIGTOP:
		if(index == 0x8f)
		{
			set_enemy_scale(e, 0.68f);
		}
		break;
	}
	switch(s->type)
	{
	case 0x2065: func_80137ae8(e, e->ai_state); break;
	case 0x2064: func_80131a9c(e, e->ai_state); break;
	case 0x2063: func_80135f98(e, e->ai_state); break;
	case 0x2062: func_80133118(e, e->ai_state); break;
	case 0x2061: func_80134420(e, e->ai_state); break;
	case 0x2060: func_8013cad4(e, e->ai_state); break;
	case 0x2068: func_8013e518(e, e->ai_state); break;
	case 0x2066: func_8013aa34(e, e->ai_state); break;
	case 0x2067: func_8013f324(e, e->ai_state); break;
	case 0x2043: rinn_setup(e, e->ai_state); break;
	case 0x2041: krinkle_setup(e, e->ai_state); break;
	case 0x2046: krampus_setup(e, e->ai_state); break;
	case 0x2045: teen_setup(e, e->ai_state); break;
	case 0x2040: dino_setup(e, e->ai_state); break;
	case 0x2042: term_setup(e, e->ai_state); break;
	case 0x2044: smilingbob_setup(e, e->ai_state); break;
	case 0x2028: finn_setup(e, e->ai_state); break;
	case 0x2020: tops_setup(e, e->ai_state); break;
	case 0x2029: flie_setup(e, e->ai_state); break;
	case 0x2022: monkey_setup(e, e->ai_state); break;
	case 0x2023: grub_setup(e, e->ai_state); break;
	case 0x2024: tort_setup(e, e->ai_state); break;
	case 0x2027: taur_setup(e, e->ai_state); break;
	case 0x2026: rodz_setup(e, e->ai_state); break;
	case 0x2025: junior_setup(e, e->ai_state); break;
	case 0x200c: zombie_setup(e, e->ai_state); break;
	case 0x2011: inng_setup(e, e->ai_state); break;
	case 0x2012: vict_setup(e, e->ai_state); break;
	case 0x2005: bat_setup(e, e->ai_state); break;
	case 0x1102: gool_setup(e, e->ai_state); break;
	case 0x2013: spdl_setup(e, e->ai_state); break;
	case 0x2007: spider_setup(e, e->ai_state); break;
	case 0x2016: hambone_setup(e, e->ai_state); break;
	case 0x2008: hand_setup(e, e->ai_state); break;
	case 0x2014: set_jester_state(e, e->ai_state); break;
	case 0x1104: mari_setup(e, e->ai_state); break;
	}
	return(1);
}
