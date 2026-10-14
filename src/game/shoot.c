/*
 * shoot.c - the light-gun shots (0x800d4f6c-0x800d6768: sampling the guns, the hit tests, the
 * players' weapons). Only three small helpers are in C so far.
 */
#include "credits.h"			/* player_stats_t */
#include "camera.h"			/* guns */

int	accuracy_tracking;		/* 0x801f8848: 1 while a level plays (set at its start, cleared with the game): shots and hits are counted */

/* 0x800d6718 */
void set_player_gun(player_stats_t *p)
{
	if(p->index == 0)
	{
		p->gun = &guns[0];
	}
	else
	{
		p->gun = &guns[1];
	}
}

/* 0x800d6744, 0x800d6758 */
void enable_accuracy_tracking(void)
{
	accuracy_tracking = 1;
}

void disable_accuracy_tracking(void)
{
	accuracy_tracking = 0;
}

#include <stdio.h>
#include "levelsnd.h"
#include "waves.h"
#include "objects.h"

extern int	tick_counter;
int	D_801e5ce0;			/* the player calibrating */
int	D_801de8a0;			/* the calibration step */
extern int	please_wait_timer;
int	D_801e7508;
extern int	D_801e5c10;			/* reload with the pump instead of off screen */
extern int	D_80212da4, D_80212da8;		/* pumps during the load screen */
extern int	loaded_wave;
extern int	increment_audit(int);
extern unsigned int read_coin_direct(void);
extern void	func_800e7130(player_stats_t *);
extern void	func_800db810(float *, int, int);	/* a bullet hole */
extern void	func_800db158(float, float, int, int);	/* the muzzle flash at a screen position */
extern void	shoot_objects(player_stats_t *);	/* objects.c */
extern int	randrng(int);
extern void	specific_gamma_load(float, float);
extern void	func_800e59b8(float, float);		/* the continue screen's crosshair */
player_stats_t *player_stats_g;		/* the player on the continue screen */
void	fire_shot(player_stats_t *);
void	reload_gun(player_stats_t *);
void	set_weapon(player_stats_t *, int);
int	use_ammo(player_stats_t *);

/* 0x800d4f6c: every vblank: the two guns' raw positions and whether they see the screen, from
   the I/O ASIC's gun bytes; their screen positions through the calibration; then the
   calibration screen's sampling, or the pending shots and the off-screen reload timers */
void read_guns(void)
{
	int	rx0, ry0, rx1, ry1;
	int	i;
	player_stats_t *p;
	gun_t	*g;

	guns[0].raw_x = *(volatile unsigned char *)0xb6800000;
	guns[0].raw_x |= (*(volatile unsigned char *)0xb6800004 & 0xf) << 8;
	guns[0].raw_y = *(volatile unsigned char *)0xb6800008;
	ry0 = guns[0].raw_y | ((*(volatile unsigned char *)0xb680000c & 3) << 8);
	guns[0].raw_y = ry0;
	rx0 = guns[0].raw_x & 0xfff;
	guns[0].on_screen = *(volatile unsigned char *)0xb6800004 & 0x40;
	guns[0].raw_x = rx0;
	guns[1].raw_x = *(volatile unsigned char *)0xb6800010;
	guns[1].raw_x |= (*(volatile unsigned char *)0xb6800014 & 0xf) << 8;
	guns[1].raw_y = *(volatile unsigned char *)0xb6800018;
	ry1 = guns[1].raw_y | ((*(volatile unsigned char *)0xb680001c & 3) << 8);
	guns[1].raw_y = ry1;
	rx1 = guns[1].raw_x & 0xfff;
	guns[1].raw_x = rx1;
	guns[1].on_screen = *(volatile unsigned char *)0xb6800014 & 0x40;
	*(volatile unsigned char *)0xb6800004 = (*(volatile unsigned char *)0xb6800004 & 0x7f) | ((tick_counter & 4) << 5);
	if(guns[1].scale_x != -1.0f)
	{
		guns[0].screen_x = (int)(((float)rx0 - guns[0].off_x) / guns[0].scale_x);
		if((float)guns[0].screen_x < 1.0f)
		{
			guns[0].screen_x = 1;
		}
		if(399.0f < (float)guns[0].screen_x)
		{
			guns[0].screen_x = 399;
		}
		guns[0].screen_y = (int)(256.0f - ((float)ry0 - guns[0].off_y) / guns[0].scale_y);
		if((float)guns[0].screen_y < 1.0f)
		{
			guns[0].screen_x = 1;
		}
		if(255.0f < (float)guns[0].screen_y)
		{
			guns[0].screen_x = 255;
		}
		guns[1].screen_x = (int)(((float)rx1 - guns[1].off_x) / guns[1].scale_x);
		if((float)guns[1].screen_x < 1.0f)
		{
			guns[1].screen_x = 1;
		}
		if(399.0f < (float)guns[1].screen_x)
		{
			guns[1].screen_x = 399;
		}
		guns[1].screen_y = (int)(256.0f - ((float)ry1 - guns[1].off_y) / guns[1].scale_y);
		if((float)guns[1].screen_y < 1.0f)
		{
			guns[1].screen_x = 1;
		}
		if(255.0f < (float)guns[1].screen_y)
		{
			guns[1].screen_x = 255;
		}
	}
	for(i = 0; i < 2; i++)
	{
		g = &guns[i];
		p = &player_stats[i];
		if(game_phase == 0x40)
		{
			if(D_801de8a0 == 1)
			{
				if(g->on_screen)
				{
					if(gun_min_x == -1)
					{
						gun_min_x = g->raw_x;
						gun_min_y = g->raw_y;
					}
					else
					{
						gun_max_x = g->raw_x;
						gun_max_y = g->raw_y;
					}
					D_801de8a0 = 0;
				}
			}
			if(D_801de8a0 == 2)
			{
				gun_min_x = 0x14a;
				gun_min_y = 0x24;
				gun_max_x = 0x6b8;
				gun_max_y = 0xdc;
			}
		}
		else if(p->shot_pending == 1)
		{
			if(g->on_screen)
			{
				fire_shot(p);
			}
			else if(p->offscreen_timer > 0)
			{
				p->offscreen_timer--;
				if(p->offscreen_timer <= 0)
				{
					reload_gun(p);
				}
			}
		}
		else if(p->shot_pending == 2)
		{
			if(g->on_screen)
			{
				p->shot_pending = 0;
				p->offscreen_timer = 0;
			}
			else if(p->offscreen_timer > 0)
			{
				p->offscreen_timer--;
				if(p->offscreen_timer <= 0)
				{
					reload_gun(p);
				}
			}
		}
		*(volatile unsigned int *)0xb6800000 = 0;
	}
}

/* 0x800d5478: a player's trigger bit this vblank, by game phase: the calibration screen
   samples a corner, the level select and the continue screen take a shot, the load screen
   counts it, and in play it fires (now, or when the gun next sees the screen); the automatic
   weapons repeat every sixth frame while held */
void gun_trigger(int player, int down)
{
	player_stats_t *p;

	p = &player_stats[player];
#ifdef PORT_HOST
	{ extern int port_trace; extern unsigned long hal_frame_count;
	  if(port_trace && down != p->trigger_was) fprintf(stderr, "[gun] vblank %lu player %d trigger %d phase %#x state %d on_screen %d at (%d, %d)\n", hal_frame_count, player, down, game_phase, p->state, p->gun ? p->gun->on_screen : -1, p->gun ? p->gun->screen_x : -1, p->gun ? p->gun->screen_y : -1); }
#endif
	if(down && !p->trigger_was)
	{
		increment_audit(p->index ? 0x6d : 0x6c);
	}
	switch(game_phase)
	{
	case 0x1000:
		if((*(volatile unsigned char *)0xb6800004 & 0x10) || (*(volatile unsigned char *)0xb6800014 & 0x10))
		{
			game_phase = 0x2000;
		}
		p->trigger_was = down;
		break;
	case 0x40:
		if(down && !p->trigger_was && player == D_801e5ce0)
		{
			D_801de8a0 = 1;
		}
		if(~read_coin_direct() & 0x24)
		{
			D_801de8a0 = 2;
		}
		p->trigger_was = down;
		break;
	case 0x20:
		if(p->state == 4 && down && !p->trigger_was)
		{
			if(p->gun->on_screen)
			{
				fire_shot(p);
			}
			else
			{
				p->shot_pending = 1;
			}
		}
		p->trigger_was = down;
		break;
	case 0x4000:
		if(down && !p->trigger_was)
		{
			if(p->gun->on_screen)
			{
				func_800e7130(p);
			}
			else
			{
				play_generic_sound((snd_object_t *)p, 0x21);
			}
		}
		p->trigger_was = down;
		break;
	case 0x80:
		if(down && !p->trigger_was)
		{
			play_generic_sound((snd_object_t *)p, 0x21);
			please_wait_timer = 0xf;
			D_801e7508 = 1;
		}
		p->trigger_was = down;
		break;
	case 2:
		if(p->state != 4)
		{
			p->trigger_was = down;
			break;
		}
		if(p->weapon == 0x1000004)
		{
			if(down)
			{
				if(!p->trigger_was)
				{
					play_generic_sound((snd_object_t *)p, 0x9c);
				}
			}
			else if(p->trigger_was)
			{
				play_generic_sound((snd_object_t *)p, 0x9d);
			}
		}
		if(p->weapon == 0x1000005)
		{
			if(down)
			{
				if(!p->trigger_was)
				{
					play_bank_sound((snd_object_t *)p, 1, "rickity");
				}
			}
			else if(p->trigger_was)
			{
				play_bank_sound((snd_object_t *)p, 2, "rickity");
			}
		}
		if(!down)
		{
			p->autofire = 0;
			p->trigger_was = down;
			break;
		}
		if(!p->trigger_was)
		{
			if(p->gun->on_screen)
			{
				fire_shot(p);
			}
			else
			{
				p->shot_pending = 1;
			}
		}
		else if(p->weapon & 0x1000000)
		{
			p->autofire++;
			if(p->autofire == (p->autofire / 6) * 6)
			{
				if(p->gun->on_screen)
				{
					fire_shot(p);
				}
				else
				{
					p->shot_pending = 1;
				}
			}
		}
		p->trigger_was = down;
		break;
	default:
		p->trigger_was = down;
		break;
	}
}

/* 0x800d5a14: the off-screen reload: the pistol's magazine refills with a sound */
void reload_gun(player_stats_t *p)
{
	if(p->has_mag)
	{
		play_generic_sound((snd_object_t *)p, 0x20);
		p->clicks = 0;
		p->ammo = p->ammo_max;
	}
	p->shot_pending = 0;
	p->offscreen_timer = 0;
}

/* 0x800d5a60 */
int ammo_missing(player_stats_t *p)
{
	if(p->has_mag)
	{
		return(p->ammo != p->ammo_max);
	}
	return(0);
}

/* 0x800d5a84: the shot is resolved against the objects, its statistics kept, the flash and
   the sound played; an empty gun clicks */
player_stats_t *resolve_shot(player_stats_t *p)
{
	p->shot_weapon = p->weapon;
	p->shot_damage = p->damage;
	switch(p->weapon)
	{
	case 1:
		if(use_ammo(p))
		{
			p->clicks++;
			if((p->clicks & 3) == 3)
			{
				play_generic_sound((snd_object_t *)p, 0x59);
			}
			else
			{
				play_generic_sound((snd_object_t *)p, 0x21);
			}
			p->shooting = 0;
			break;
		}
		if(accuracy_tracking)
		{
			p->shots++;
		}
		shoot_objects(p);
		if(p->hit)
		{
			if(accuracy_tracking)
			{
				p->hits++;
			}
		}
		else if(p->shot_depth > 4.0f)
		{
			func_800db810(p->hit_point, 2, 0);
		}
		if(accuracy_tracking)
		{
			p->accuracy = (float)p->hits / (float)p->shots;
		}
		func_800db158(p->shot_x, p->shot_y, p->index, 0);
		play_generic_sound((snd_object_t *)p, randrng(1) + 0x3d);
		p->shooting = 0;
		break;
	case 2:
		if(use_ammo(p))
		{
			p->clicks++;
			if((p->clicks & 3) == 3)
			{
				play_generic_sound((snd_object_t *)p, 0x59);
			}
			else
			{
				play_generic_sound((snd_object_t *)p, 0x21);
			}
			p->shooting = 0;
			break;
		}
		if(accuracy_tracking)
		{
			p->shots++;
		}
		shoot_objects(p);
		if(p->hit)
		{
			if(accuracy_tracking)
			{
				p->hits++;
			}
		}
		func_800db158(p->shot_x, p->shot_y, p->index + 2, 0);
		play_generic_sound((snd_object_t *)p, randrng(2) + 0x38);
		if(accuracy_tracking)
		{
			p->accuracy = (float)p->hits / (float)p->shots;
		}
		p->shooting = 0;
		break;
	case 0x1000003:
		if(use_ammo(p))
		{
			p->clicks++;
			if((p->clicks & 3) == 3)
			{
				play_generic_sound((snd_object_t *)p, 0x59);
			}
			else
			{
				play_generic_sound((snd_object_t *)p, 0x21);
			}
			p->shooting = 0;
			break;
		}
		if(accuracy_tracking)
		{
			p->shots++;
		}
		shoot_objects(p);
		if(p->hit)
		{
			if(accuracy_tracking)
			{
				p->hits++;
			}
		}
		func_800db158(p->shot_x, p->shot_y, p->index, 0);
		play_generic_sound((snd_object_t *)p, randrng(2) + 0x38);
		if(accuracy_tracking)
		{
			p->accuracy = (float)p->hits / (float)p->shots;
		}
		p->shooting = 0;
		break;
	case 0x1000004:
		if(use_ammo(p))
		{
			p->shooting = 0;
			break;
		}
		if(accuracy_tracking)
		{
			p->shots++;
		}
		shoot_objects(p);
		if(accuracy_tracking)
		{
			p->hits++;
		}
		func_800db158(p->shot_x, p->shot_y, 5, 0);
		if(accuracy_tracking)
		{
			p->accuracy = (float)p->hits / (float)p->shots;
		}
		p->shooting = 0;
		break;
	case 0x1000005:
		if(use_ammo(p))
		{
			p->shooting = 0;
			break;
		}
		if(accuracy_tracking)
		{
			p->shots++;
		}
		shoot_objects(p);
		if(accuracy_tracking)
		{
			p->hits++;
		}
		func_800db158(p->shot_x, p->shot_y, 4, 0);
		if(accuracy_tracking)
		{
			p->accuracy = (float)p->hits / (float)p->shots;
		}
		p->shooting = 0;
		break;
	default:
		p->shooting = 0;
		break;
	}
	return(p);
}

/* 0x800d5e44: the gun's screen position becomes the shot and the draw pass resolves it */
void fire_shot(player_stats_t *p)
{
	p->shooting = 1;
	p->shot_pending = 0;
	p->offscreen_timer = 0;
	p->shot_x = (float)p->gun->screen_x;
	p->shot_y = (float)p->gun->screen_y;
}

/* 0x800d5e78: a round is spent: 1 when there was none; the last round of a pick-up weapon
   hands the pistol back */
int use_ammo(player_stats_t *p)
{
	if(loaded_wave == WAVE_LEVEL_SELECT)
	{
		return(0);
	}
	if(loaded_wave == WAVE_ATTRACT)
	{
		return(0);
	}
	if(p->ammo <= 0)
	{
		return(1);
	}
	p->ammo--;
	if(p->ammo > 0)
	{
		return(0);
	}
	if(p->weapon == 0x1000004)
	{
		play_generic_sound((snd_object_t *)p, 0x9d);
	}
	if(p->weapon == 0x1000005)
	{
		play_bank_sound((snd_object_t *)p, 2, "rickity");
	}
	if(!p->has_mag)
	{
		set_weapon(p, 1);
	}
	return(0);
}

/* 0x800d5f34: a weapon with its rounds and damage */
void set_weapon(player_stats_t *p, int weapon)
{
	switch(weapon)
	{
	case 1:
		p->weapon = weapon;
		p->has_mag = weapon;
		p->ammo = p->pistol_ammo;
		p->ammo_max = p->pistol_ammo;
		p->damage = 2.0f;
		break;
	case 2:
		p->weapon = weapon;
		p->ammo = 10;
		p->ammo_max = 10;
		p->has_mag = 0;
		p->damage = 10.0f;
		break;
	case 0x1000005:
		p->weapon = weapon;
		p->ammo = 18;
		p->ammo_max = 18;
		p->has_mag = 0;
		p->damage = 22.0f;
		break;
	case 0x1000003:
		p->weapon = weapon;
		p->ammo = 50;
		p->ammo_max = 50;
		p->has_mag = 0;
		p->damage = 2.0f;
		break;
	case 0x1000004:
		p->weapon = weapon;
		p->ammo = 30;
		p->ammo_max = 30;
		p->has_mag = 0;
		p->damage = 2.0f;
		break;
	}
}

/* 0x800d6004: the ten-round pistol */
void give_pistol(player_stats_t *p)
{
	p->pistol_ammo = 10;
	p->weapon = 1;
	p->has_mag = 1;
	p->ammo = p->pistol_ammo;
	p->ammo_max = p->pistol_ammo;
	p->damage = 2.0f;
}

/* 0x800d6038: a player's pump bit this vblank: in play a pump reloads (when the adjustment
   says so, else only off screen); on the load screen it is counted */
void gun_pump(player_stats_t *p, int down)
{
	if(down && !p->pump_was)
	{
		increment_audit(p->index ? 0x6f : 0x6e);
	}
	if(game_phase == 2)
	{
		if(p->state == 4 && down && !p->pump_was)
		{
			if(D_801e5c10)
			{
				if(ammo_missing(p))
				{
					if(p->has_mag)
					{
						play_generic_sound((snd_object_t *)p, 0x20);
						p->clicks = 0;
						p->ammo = p->ammo_max;
					}
					p->shot_pending = 0;
					p->offscreen_timer = 0;
				}
			}
			else if(!p->gun->on_screen)
			{
				if(ammo_missing(p))
				{
					p->shot_pending = 2;
				}
			}
		}
	}
	else if(game_phase == 0x80)
	{
		if(down && !p->pump_was)
		{
			if(p->index == 0)
			{
				D_80212da4++;
			}
			else
			{
				D_80212da8++;
			}
		}
	}
	p->pump_was = down;
}

extern float	D_80212df4;			/* the gamma the level plays at */
extern int	func_80162570(void);		/* a fade is running */
int	D_80179d60 = 0;				/* 0x80179d60 (.data): the screen is held bright for a gun to see */

/* 0x800d57b0: every vblank: both guns' trigger and pump bits, then the screen brightness:
   bright while a player's shot waits for the gun to see the screen, normal again after */
void gun_vblank(void)
{
	player_stats_t *p;

	if(guns_active)
	{
		gun_trigger(0, *(volatile unsigned char *)0xb6800004 & 0x10);
		gun_trigger(1, *(volatile unsigned char *)0xb6800014 & 0x10);
		gun_pump(&player_stats[0], *(volatile unsigned char *)0xb6800004 & 0x20);
		gun_pump(&player_stats[1], *(volatile unsigned char *)0xb6800014 & 0x20);
	}
	if(game_phase == 0x100)
	{
		p = player_stats_g;
		if(p->trigger_was)
		{
			if(D_80212df4 < 0.6f)
			{
				specific_gamma_load(0.6f, 1.0f);
			}
			else
			{
				specific_gamma_load(D_80212df4, 1.0f);
			}
			if(p->gun->on_screen)
			{
				func_800e59b8((float)p->gun->screen_x, (float)p->gun->screen_y);
			}
		}
		else
		{
			specific_gamma_load(0.0f, 1.0f);
		}
	}
	else if(game_phase == 0x40)
	{
		if(D_801de8a0 == 1)
		{
			specific_gamma_load(0.8f, 1.0f);
		}
		else
		{
			specific_gamma_load(0.0f, 1.0f);
		}
	}
	else if(player_stats[0].shot_pending || player_stats[1].shot_pending)
	{
		if(!func_80162570())
		{
			specific_gamma_load(D_80212df4, 1.0f);
			D_80179d60 = 1;
			if(player_stats[0].shot_pending && !player_stats[0].offscreen_timer)
			{
				player_stats[0].offscreen_timer = 2;
			}
			if(player_stats[1].shot_pending && !player_stats[1].offscreen_timer)
			{
				player_stats[1].offscreen_timer = 2;
			}
		}
	}
	else if(!func_80162570() && D_80179d60 == 1)
	{
		D_80179d60 = 0;
		specific_gamma_load(0.0f, 1.0f);
	}
}
