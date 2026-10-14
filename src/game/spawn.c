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
extern void	func_800d6758(void);
extern void	set_enemy_scale(enemy_t *, float);	/* objects.c */
extern void	level_end_stats_wrapper(game_object_t *);
#define NO_INIT		((int (*)(game_object_t *))0)
#define NO_THINK	((void (*)(game_object_t *))0)
/* every enemy's draw, init and think callbacks, and its spawn-time set-up (its own file each) */
#define ENEMY(d, i, t)	extern void d(game_object_t *); extern int i(game_object_t *); extern void t(game_object_t *);
ENEMY(func_8011bf38, func_8011c8d4, func_8011cd18)
ENEMY(func_80107e30, func_80108ab0, func_80108c94)
ENEMY(func_800f4618, func_800f4c50, func_800f4e24)
ENEMY(something_gool_anim, func_800eb150, func_800eb3e0)
ENEMY(something_mari_anim, func_800edff0, func_800ee228)
ENEMY(func_800f38c8, func_800f3f1c, func_800f40c8)
ENEMY(func_80105388, func_801058bc, func_80105a78)
ENEMY(func_800f70b8, func_800f7680, func_800f7824)
ENEMY(func_800f7988, func_800f8860, func_800f8c84)
ENEMY(func_80105e38, func_80106584, func_8010674c)
ENEMY(func_80106928, func_80106e78, func_8010701c)
ENEMY(func_80103c90, func_80104170, func_8010430c)
ENEMY(func_80102e58, func_801032c8, func_80103420)
ENEMY(func_80109cf4, func_8010aa68, func_8010acac)
ENEMY(func_801026b0, func_80102bb8, func_80102d4c)
ENEMY(func_80103568, func_80103a1c, func_80103bb0)
ENEMY(func_801163e8, func_80116e10, func_80117080)
ENEMY(func_80104418, func_80104928, func_80104abc)
ENEMY(func_80104bc8, func_801050ec, func_80105280)
ENEMY(func_8011a170, func_8011a6f8, func_8011a8d0)
ENEMY(func_8011b510, func_8011ba5c, func_8011bc34)
ENEMY(func_8011fa7c, func_80120714, func_801209ec)
ENEMY(func_8012b7d8, func_8012c2c4, func_8012c4d8)
ENEMY(func_80122e40, func_801235ec, func_8012387c)
ENEMY(func_8011dd80, func_8011e674, func_8011e890)
ENEMY(func_80118728, func_8011905c, func_80119280)
ENEMY(func_801222d0, func_80122818, func_801229d4)
ENEMY(term_draw, term_init, term_think)
ENEMY(func_80124c68, func_8012562c, func_80125824)
ENEMY(krinkle_draw, krinkle_init, func_80126fd8)
ENEMY(func_801293b8, func_801298c8, func_80129a8c)
ENEMY(func_80129d58, func_8012a818, func_8012aa28)
ENEMY(func_801348e0, func_801352f4, func_801354fc)
ENEMY(render_enetity_debug_info, func_8013c35c, func_8013c56c)
ENEMY(func_8012d4a0, spawn_krampus_, func_8012e850)
ENEMY(func_80133580, func_80133d38, func_80133f28)
ENEMY(func_80131f10, func_8013281c, func_80132a14)
ENEMY(func_80138ac4, func_801394f4, func_801397f4)
ENEMY(func_80130bf8, func_801313e0, func_801315d4)
ENEMY(func_80136840, func_80136fdc, func_801371cc)
ENEMY(func_8013ea08, func_8013ef18, func_8013f0dc)
ENEMY(func_8013cfb0, func_8013d794, func_8013d990)
#define SETUP(f)	extern void f(enemy_t *, unsigned short);
SETUP(func_80119894) SETUP(func_8010725c) SETUP(func_800eeeec) SETUP(func_800eb7d4) SETUP(func_800f96a8)
SETUP(func_800f7960) SETUP(func_80105cb4) SETUP(func_801068c8) SETUP(func_8011af54) SETUP(func_8010b384)
SETUP(func_80109260) SETUP(func_80117c88) SETUP(func_8011d704) SETUP(func_80121318) SETUP(func_8011f070)
SETUP(func_8012f320) SETUP(krinkle_setup) SETUP(func_80123cd4) SETUP(func_80125eb4) SETUP(func_80129c40)
SETUP(func_8012b260) SETUP(func_8012cce8) SETUP(func_80131a9c) SETUP(func_80134420) SETUP(func_8013cad4)
SETUP(func_80133118) SETUP(func_80135f98) SETUP(func_8013aa34) SETUP(func_8013f324) SETUP(func_8013e518)
SETUP(func_80137ae8) SETUP(term_setup) SETUP(func_80122ca8) SETUP(func_8011be94)
extern void	something_bat_state(enemy_t *, short), func_800f5278(enemy_t *, short);

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
		func_800d6758();
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
		draw = func_801293b8;
		init = func_801298c8;
		think = func_80129a8c;
		break;
	case 0x2041:
		type = (short)s->type;
		draw = krinkle_draw;
		init = krinkle_init;
		think = func_80126fd8;
		break;
	case 0x2046:
		type = (short)s->type;
		draw = func_8012d4a0;
		init = spawn_krampus_;
		think = func_8012e850;
		break;
	case 0x2045:
		type = (short)s->type;
		draw = func_8012b7d8;
		init = func_8012c2c4;
		think = func_8012c4d8;
		break;
	case 0x2040:
		type = (short)s->type;
		draw = func_80124c68;
		init = func_8012562c;
		think = func_80125824;
		break;
	case 0x2042:
		type = (short)s->type;
		draw = term_draw;
		init = term_init;
		think = term_think;
		break;
	case 0x2044:
		type = (short)s->type;
		draw = func_80129d58;
		init = func_8012a818;
		think = func_8012aa28;
		break;
	case 0x2028:
		type = (short)s->type;
		draw = func_801222d0;
		init = func_80122818;
		think = func_801229d4;
		break;
	case 0x2020:
		type = (short)s->type;
		draw = func_801163e8;
		init = func_80116e10;
		think = func_80117080;
		break;
	case 0x2029:
		type = (short)s->type;
		draw = func_80122e40;
		init = func_801235ec;
		think = func_8012387c;
		break;
	case 0x2022:
		type = (short)s->type;
		draw = func_8011a170;
		init = func_8011a6f8;
		think = func_8011a8d0;
		break;
	case 0x2023:
		type = (short)s->type;
		draw = func_8011b510;
		init = func_8011ba5c;
		think = func_8011bc34;
		break;
	case 0x2024:
		type = (short)s->type;
		draw = func_8011bf38;
		init = func_8011c8d4;
		think = func_8011cd18;
		break;
	case 0x2027:
		type = (short)s->type;
		draw = func_80118728;
		init = func_8011905c;
		think = func_80119280;
		break;
	case 0x2026:
		type = (short)s->type;
		draw = func_8011dd80;
		init = func_8011e674;
		think = func_8011e890;
		break;
	case 0x2025:
		type = (short)s->type;
		draw = func_8011fa7c;
		init = func_80120714;
		think = func_801209ec;
		break;
	case 0x200c:
		type = (short)s->type;
		draw = func_800f7988;
		init = func_800f8860;
		think = func_800f8c84;
		break;
	case 0x2011:
		type = (short)s->type;
		draw = func_80105388;
		init = func_801058bc;
		think = func_80105a78;
		break;
	case 0x2012:
		type = (short)s->type;
		draw = func_80105e38;
		init = func_80106584;
		think = func_8010674c;
		break;
	case 0x2005:
		type = (short)s->type;
		draw = func_800f38c8;
		init = func_800f3f1c;
		think = func_800f40c8;
		break;
	case 0x1102:
		type = 0x1102;
		draw = something_gool_anim;
		init = func_800eb150;
		think = func_800eb3e0;
		break;
	case 0x2007:
		type = 0x2007;
		draw = func_800f4618;
		init = func_800f4c50;
		think = func_800f4e24;
		break;
	case 0x2013:
		type = 0x2013;
		draw = func_80106928;
		init = func_80106e78;
		think = func_8010701c;
		break;
	case 0x2016:
		type = 0x2016;
		draw = func_80109cf4;
		init = func_8010aa68;
		think = func_8010acac;
		break;
	case 0x2014:
		type = 0x2014;
		draw = func_80107e30;
		init = func_80108ab0;
		think = func_80108c94;
		break;
	case 0x2008:
		type = 0x2008;
		draw = func_800f70b8;
		init = func_800f7680;
		think = func_800f7824;
		break;
	case 0x1104:
		type = 0x1104;
		draw = something_mari_anim;
		init = func_800edff0;
		think = func_800ee228;
		break;
	case 0x2018:
		type = 0x2018;
		draw = func_801026b0;
		init = func_80102bb8;
		think = func_80102d4c;
		break;
	case 0x2019:
		type = 0x2019;
		draw = func_80103568;
		init = func_80103a1c;
		think = func_80103bb0;
		break;
	case 0x2017:
		type = 0x2017;
		draw = func_80102e58;
		init = func_801032c8;
		think = func_80103420;
		break;
	case 0x201d:
		type = 0x201d;
		draw = func_80104bc8;
		init = func_801050ec;
		think = func_80105280;
		break;
	case 0x201c:
		type = 0x201c;
		draw = func_80104418;
		init = func_80104928;
		think = func_80104abc;
		break;
	case 0x201b:
		type = 0x201b;
		draw = func_80103c90;
		init = func_80104170;
		think = func_8010430c;
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
	case 0x2043: func_80129c40(e, e->ai_state); break;
	case 0x2041: krinkle_setup(e, e->ai_state); break;
	case 0x2046: func_8012f320(e, e->ai_state); break;
	case 0x2045: func_8012cce8(e, e->ai_state); break;
	case 0x2040: func_80125eb4(e, e->ai_state); break;
	case 0x2042: term_setup(e, e->ai_state); break;
	case 0x2044: func_8012b260(e, e->ai_state); break;
	case 0x2028: func_80122ca8(e, e->ai_state); break;
	case 0x2020: func_80117c88(e, e->ai_state); break;
	case 0x2029: func_80123cd4(e, e->ai_state); break;
	case 0x2022: func_8011af54(e, e->ai_state); break;
	case 0x2023: func_8011be94(e, e->ai_state); break;
	case 0x2024: func_8011d704(e, e->ai_state); break;
	case 0x2027: func_80119894(e, e->ai_state); break;
	case 0x2026: func_8011f070(e, e->ai_state); break;
	case 0x2025: func_80121318(e, e->ai_state); break;
	case 0x200c: func_800f96a8(e, e->ai_state); break;
	case 0x2011: func_80105cb4(e, e->ai_state); break;
	case 0x2012: func_801068c8(e, e->ai_state); break;
	case 0x2005: something_bat_state(e, e->ai_state); break;
	case 0x1102: func_800eb7d4(e, e->ai_state); break;
	case 0x2013: func_8010725c(e, e->ai_state); break;
	case 0x2007: func_800f5278(e, e->ai_state); break;
	case 0x2016: func_8010b384(e, e->ai_state); break;
	case 0x2008: func_800f7960(e, e->ai_state); break;
	case 0x2014: func_80109260(e, e->ai_state); break;
	case 0x1104: func_800eeeec(e, e->ai_state); break;
	}
	return(1);
}
