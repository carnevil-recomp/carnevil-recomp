/*
 * player.c - the players' records (0x800e2d48-0x800e5xxx: the start, the continue, the HUD, the
 * high-score entry). Only the start and the boot-time set-up are in C so far.
 */
#include "credits.h"			/* player_stats */
#include "waves.h"

extern int	increment_audit(int);
extern void	set_weapon(player_stats_t *, int);
extern void	set_player_gun(player_stats_t *);			/* 0x800d6718 */
extern int	loaded_wave, D_801facc8;
extern int	game_state[4];						/* credits.h: [0] is the phase */
extern void	start_shake(int);					/* shake.c */
extern float	func_800fe8dc(float range);				/* a random float in +-range */
extern void	kill_player(player_stats_t *);
extern void	average_audit(int count_audit, int mean_audit, int value);	/* adjust.c */
extern int	snd_stop_track(unsigned int);
extern int	printf(const char *, ...);
#include <glide/glide.h>
#include "levelsnd.h"
#include "sprites.h"
#include "texmap.h"
#include "objects.h"			/* remove_all_objects */
#include "spawn.h"			/* free_die_list */
extern int	D_80237420;						/* adjustment 0x3b: show the scores (adjust.c) */
extern int	have_credits_to_start(void), have_credits_to_continue(void);	/* credits.c */
extern char	text_buffer[];						/* text.c */
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_scaled(int, char *, float, float, unsigned int, float);
extern void	draw_string_scaled_right(int, char *, float, float, unsigned int, float);
extern void	draw_string_scaled_centered(int, char *, float, float, unsigned int, float);
extern void	draw_string_centered_alpha(int, char *, float, float, unsigned int, float);
extern void	func_800d3528(MidVertexWithTA *, MidVertexWithTA *, MidVertexWithTA *);	/* x, y, q, s/w, t/w, a with the depth gradients */
extern void	func_800e7568(void);					/* movie stop */
extern void	unload_wave(void), free_blood_drops(void), free_smoke(void), free_sparks(void), free_snow(void), free_flecks(void), free_chunks(void);
extern int	something_with_reload_p1, something_with_reload_p2;
extern int	D_801de8a4;						/* adjustment 0x31 (adjust.c) */

/* 0x800e4bc0 */
void set_level_start_time(player_stats_t *p)
{
	p->level_start_time = D_801facc8;
}

/* 0x800e2d48: a player starts or continues: the audits (starts, two-player games, continues
   per level and in total), the counters, then state 4 with the difficulty's health */
void init_player_stats(int player)
{
	player_stats_t	*p, *other;

	other = player_stats;
	p = other + player;
	if(player == 0)
	{
		increment_audit(0x1f);
		other++;
	}
	else
	{
		increment_audit(0x20);
	}
	p->clicks = 0;
	p->autofire = 0;
	p->shot_pending = 0;
	p->offscreen_timer = 0;
	p->trigger_was = 0;
	p->pistol_ammo = 6;
	set_weapon(p, 1);
	p->unk10c = 0;
	increment_audit(0x21);
	if(other->state == 4)
	{
		increment_audit(0x1d);
	}
	else
	{
		increment_audit(0x1e);
	}
	if(p->state != 8)
	{
		p->score = 0;
		p->hits = 0;
		p->shots = 0;
		p->accuracy = 0;
		p->continues = 0;
		increment_audit(0xc);
		p->unkd4 = 0;
	}
	else
	{
		switch(loaded_wave)
		{
		case WAVE_HHOUSE:
			increment_audit(0x58);
			break;
		case WAVE_FREAKSHOW:
			increment_audit(0x5d);
			break;
		case WAVE_RICKITY:
			increment_audit(0x62);
			break;
		case WAVE_BIGTOP:
			increment_audit(0x67);
			break;
		}
		set_level_start_time(p);
		p->continues++;
		increment_audit(0xd);
		if(p->continues < 15)
		{
			increment_audit(p->continues + 0xd);
		}
		else
		{
			increment_audit(0x1c);
		}
		p->unkd4 = 100;
	}
	p->state = 4;
	p->health = p->max_health = (float)(1300 - D_801de8a4 * 200);
	p->start_time = D_801facc8;
}

/* 0x800e2f08: a player's constants at boot: index, gun, colour, sound priorities, screen position */
void init_player(int player)
{
	player_stats_t	*p;

	p = &player_stats[player];
	p->index = player;
	set_player_gun(p);
	if(p->index == 0)
	{
		p->volume = 10;
		p->colour = 0xff00;
		p->priority = 0x80c1;
		p->unk6c = 100.0f;
		p->priority2 = 0x80c2;
	}
	else
	{
		p->volume = 0xf5;
		p->colour = 0xff00ff;
		p->priority = 0x8141;
		p->unk6c = 300.0f;
		p->priority2 = 0x8142;
	}
	p->unk7c = 0xff;
	p->unk80 = 0xff;
	p->shot_pending = 0;
	p->offscreen_timer = 0;
	p->unk10c = 0;
	p->unk110 = 0;
	p->unk114 = 0;
	p->unk118 = 0;
	p->unk11c = 0;
	p->unk70 = 128.0f;
}

/* 0x800e2958: points for a player, scaled by the accuracy so far, in tens */
void award_points(int player, int points)
{
	player_stats_t *p;
	float	f;

	p = &player_stats[player];
	f = (float)points;
	if(p->shots > 0)
	{
		f = f * p->accuracy;
	}
	p->score = p->score + (int)f * 10;
}

/* 0x800e29b0, 0x800e29bc: the damage of the shot being resolved, and the same scaled down
   (1.350000023841858) unless both players are in the game */
float shot_damage_of(player_stats_t *p)
{
	return(p->shot_damage);
}

float shot_damage_shared(player_stats_t *p)
{
	if((player_stats[0].state & 4) && (player_stats[1].state & 4))
	{
		return(p->shot_damage);
	}
	return(p->shot_damage * 1.350000023841858f);
}

/* 0x800e2a08: health back, up to the maximum */
void heal_player(player_stats_t *p, float amount)
{
	float	h;

	h = p->health + amount;
	if(p->max_health < h)
	{
		p->health = h;
		p->health = p->max_health;
	}
	else
	{
		p->health = h;
	}
}

int	hurt_cue_count;			/* 0x8017ddf0: the hurt cues alternate 0xa2/0xa3 */

/* 0x800e2a34: an enemy's attack lands: the damage comes off the health (a dead player goes
   to the continue countdown), the screen shakes, a hurt cue plays and the attack's hit mark
   (a screen sprite, kinds 6-15) appears over the player's side. kind 6 scatters its mark by
   param and plays no cue; 1 and 7 draw it mirrored when param is set; 5 only shakes and is
   the one attack that lands during the grace after a continue. */
void hurt_player(player_stats_t *p, int damage, int kind, float param)
{
	if(game_state[0] != 2)
	{
		return;
	}
	if(p->state != 4)
	{
		return;
	}
	if(p->unkd4 > 0)
	{
		if(kind != 5)
		{
			return;
		}
	}
	p->health = p->health - (float)damage;
	if(p->health <= 0.0f)
	{
		kill_player(p);
	}
	switch(kind)
	{
	case 6:
		add_screen_sprite(p->unk6c + param, func_800fe8dc(10.0f) + 128.0f, 11, 0);
		start_shake(1);
		break;
	case 5:
		start_shake(1);
		break;
	case 8:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		break;
	case 4:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite(p->unk6c, 128.0f, 9, 0);
		break;
	case 1:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite_mirror(p->unk6c, 128.0f, 7, 0, param);
		break;
	case 7:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite_mirror(p->unk6c, 128.0f, 8, 0, param);
		break;
	case 0:
	case 3:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite(p->unk6c, 128.0f, 6, 0);
		break;
	case 9:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite(p->unk6c, 128.0f, 12, 0);
		break;
	case 10:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite(p->unk6c, 128.0f, 13, 0);
		break;
	case 11:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite(p->unk6c, 128.0f, 14, 0);
		break;
	case 12:
		start_shake(1);
		hurt_cue_count++;
		play_generic_rotating_cue(p, (hurt_cue_count & 1) | 0xa2);
		add_screen_sprite(p->unk6c, 128.0f, 15, 0);
		break;
	default:
		printf("Unknown hurt in decrement_health\n");
		break;
	}
}

/* 0x800e4bd0: the time the player spent in the level goes into its average audit */
void audit_level_time(player_stats_t *p)
{
	int	now, value, wave;

	now = D_801facc8;
	value = now - p->level_start_time;
	p->unk3c = now;
	wave = loaded_wave;
	switch(wave)
	{
	case WAVE_HHOUSE:
		average_audit(0x57, 0x56, value);
		break;
	case WAVE_FREAKSHOW:
		average_audit(0x5c, 0x5b, value);
		break;
	case WAVE_RICKITY:
		average_audit(0x61, 0x60, value);
		break;
	case WAVE_BIGTOP:
		average_audit(0x66, 0x65, value);
		break;
	}
}

/* 0x800e3c74: the player's health ran out: audit the time played, wait to continue (300
   frames), and stop the player's own music track */
void kill_player(player_stats_t *p)
{
	int	now;

	now = D_801facc8;
	p->unk34 = now;
	average_audit(0x6b, 0x6a, now - p->start_time);
	audit_level_time(p);
	p->state = 8;
	p->clicks = 0;
	p->health = 0.0f;
	p->time_to_continue = 300;
	if(p->index)
	{
		snd_stop_track(4);
	}
	else
	{
		snd_stop_track(2);
	}
}

/* ---- the HUD (0x800e2fe4-0x800e4bc0): the score, the pickup messages, the continue count,
   the credit line, the health bar and the ammo (bullets, or a gauge for the automatic weapons)
   for each player; drawn by the draw pass after the level ---- */

int		hud_pulse = 0x808080;		/* 0x8017ddf4: the health bar's colour pulses between 0x5f and 0xee */
int		hud_pulse_step = 0x101010;	/* 0x8017ddf8 */
int		hud_frames;			/* 0x8017ddfc */
int		hud_gauge_frame;		/* 0x8017de00: 0..2, the animated gauges */
int		hud_blink;			/* 0x8017de04: 0..100, the credit line alternates at 50 */

#define HUD_OOW		0.90909088f

/* 0x800e3704: the health bar at (x, y): the fill texture (0x6007) scaled by the pulsing
   colour and the player's own, then the frame (0x6006). During the grace after a continue the
   fill blinks (every other frame at first, then every fourth) */
void draw_health_bar(player_stats_t *p, float x, float y)
{
	MidVertexWithTA	a, b, c, d;
	float		fill, x0, x1;

	fill = 0.0f;
	if(p->health > 0.0f)
	{
		fill = p->health / p->max_health;
	}
	fill = fill * 0.78125f + 0.1015625f;
	guTexSource(find_surface(0x6007)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB);
	grConstantColorValue(hud_pulse & p->colour);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA);
	grAlphaTestReferenceValue(15);
	x0 = x - 80.0f;
	x1 = x + 80.0f;
	a.a = b.a = c.a = d.a = 188.0f;
	a.oow = b.oow = c.oow = d.oow = HUD_OOW;
	a.sow = c.sow = 0.5f * HUD_OOW;
	a.y = b.y = y - 14.0f;
	c.y = d.y = y + 16.0f;
	a.tow = b.tow = 63.125f * HUD_OOW;
	c.tow = d.tow = 0.125f * HUD_OOW;
	b.sow = d.sow = fill * 255.5f * HUD_OOW;
	a.x = c.x = x0;
	b.x = d.x = x0 + fill * (x1 - x0);
	if(p->unkd4 <= 0 || (p->unkd4 < 31 ? (p->unkd4 & 2) : (p->unkd4 & 4)))
	{
		func_800d3528(&a, &d, &b);
		func_800d3528(&a, &c, &d);
	}
	guTexSource(find_surface(0x6006)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
	x0 = x - 80.0f;
	x1 = x + 80.0f;
	a.a = b.a = c.a = d.a = 118.0f;
	a.y = b.y = y - 16.0f;
	c.y = d.y = y + 16.0f;
	a.oow = b.oow = c.oow = d.oow = HUD_OOW;
	a.sow = c.sow = 0.5f * HUD_OOW;
	b.sow = d.sow = 255.5f * HUD_OOW;
	a.tow = b.tow = 63.125f * HUD_OOW;
	c.tow = d.tow = 0.125f * HUD_OOW;
	a.x = c.x = x0;
	b.x = d.x = x1;
	func_800d3528(&a, &d, &b);
	func_800d3528(&a, &c, &d);
}

/* 0x800e39bc: the same filling from the right, for player 2 */
void draw_health_bar_mirrored(player_stats_t *p, float x, float y)
{
	MidVertexWithTA	a, b, c, d;
	float		fill, x0, x1;

	fill = 0.0f;
	if(p->health > 0.0f)
	{
		fill = p->health / p->max_health;
	}
	fill = fill * 0.78125f + 0.1015625f;
	guTexSource(find_surface(0x6007)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_TEXTURE_TIMES_CCRGB);
	grConstantColorValue(hud_pulse & p->colour);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA);
	grAlphaTestReferenceValue(15);
	x0 = x + 80.0f;
	x1 = x - 80.0f;
	a.a = b.a = c.a = d.a = 188.0f;
	a.oow = b.oow = c.oow = d.oow = HUD_OOW;
	a.sow = c.sow = 0.5f * HUD_OOW;
	a.y = b.y = y - 14.0f;
	c.y = d.y = y + 16.0f;
	a.tow = b.tow = 63.125f * HUD_OOW;
	c.tow = d.tow = 0.125f * HUD_OOW;
	b.sow = d.sow = fill * 255.5f * HUD_OOW;
	a.x = c.x = x0;
	b.x = d.x = x0 + fill * (x1 - x0);
	if(p->unkd4 <= 0 || (p->unkd4 < 31 ? (p->unkd4 & 2) : (p->unkd4 & 4)))
	{
		func_800d3528(&a, &d, &b);
		func_800d3528(&a, &c, &d);
	}
	guTexSource(find_surface(0x6006)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
	x0 = x + 80.0f;
	x1 = x - 80.0f;
	a.a = b.a = c.a = d.a = 118.0f;
	a.y = b.y = y - 16.0f;
	c.y = d.y = y + 16.0f;
	a.oow = b.oow = c.oow = d.oow = HUD_OOW;
	a.sow = c.sow = 0.5f * HUD_OOW;
	b.sow = d.sow = 255.5f * HUD_OOW;
	a.tow = b.tow = 63.125f * HUD_OOW;
	c.tow = d.tow = 0.125f * HUD_OOW;
	a.x = c.x = x0;
	b.x = d.x = x1;
	func_800d3528(&a, &d, &b);
	func_800d3528(&a, &c, &d);
}

/* 0x800e3de8: a player's status: the score (adjustment 0x3b), the pickup message fading
   out (unk10c, unk110 its alpha), the reload reminder after four empty pulls, the continue
   countdown (and the game over when both players are out), and the credit line */
void draw_player_status(player_stats_t *p)
{
	int	state;
	float	x, a, alpha;

	hud_blink += 2;
	if(hud_blink >= 101)
	{
		hud_blink = 0;
	}
	if(p->unkd4 > 0)
	{
		p->unkd4--;
	}
	if(D_80237420 && (p->state & 0xc))
	{
		graphical_printf(text_buffer, "%d", p->score);
		if(p->index == 0)
		{
			draw_string_scaled(1, text_buffer, 20.0f, 236.0f, 0x999999, 0.75f);
		}
		else
		{
			draw_string_scaled_right(1, text_buffer, 380.0f, 236.0f, 0x999999, 0.75f);
		}
	}
	state = p->state;
	if(state & 0x24)
	{
		if(p->unk10c)
		{
			x = 314.0f;
			if(p->index == 0)
			{
				x = 86.0f;
			}
			switch(p->unk10c - 1)
			{
			case 0:
				draw_string_centered_alpha(1, "HEALTH UP", x, p->unk70 - 100.0f, p->colour, *(float *)&p->unk110);
				a = *(float *)&p->unk110 - 10.0f;
				break;
			case 1:
				draw_string_centered_alpha(1, "SHOTGUN X 10", x, p->unk70 - 100.0f, p->colour, *(float *)&p->unk110);
				a = *(float *)&p->unk110 - 10.0f;
				break;
			case 5:
				draw_string_centered_alpha(1, "MACHINE GUN", x, p->unk70 - 100.0f, p->colour, *(float *)&p->unk110);
				a = *(float *)&p->unk110 - 10.0f;
				break;
			case 7:
				draw_string_centered_alpha(1, "ACID BATH", x, p->unk70 - 100.0f, p->colour, *(float *)&p->unk110);
				a = *(float *)&p->unk110 - 10.0f;
				break;
			case 6:
				draw_string_centered_alpha(1, "FLAMETHROWER", x, p->unk70 - 100.0f, p->colour, *(float *)&p->unk110);
				a = *(float *)&p->unk110 - 10.0f;
				break;
			case 3:
				draw_string_centered_alpha(1, "INCREASE CLIP", x, p->unk70 - 100.0f, p->colour, *(float *)&p->unk110);
				a = *(float *)&p->unk110 - 10.0f;
				break;
			case 4:
				draw_string_centered_alpha(1, "INNOCENT SHOT", x, p->unk70 - 75.0f, 0xff0000, *(float *)&p->unk110);
				draw_string_centered_alpha(1, "LIFE DOWN", x, p->unk70 - 100.0f, 0xff0000, *(float *)&p->unk110);
				a = *(float *)&p->unk110 - 8.0f;
				break;
			default:
				goto done_message;
			}
			*(float *)&p->unk110 = a;
			if(a < 0.0f)
			{
				p->unk10c = 0;
			}
		}
done_message:
		if(p->clicks >= 4)
		{
			guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
			guTexSource(find_surface(0x600a)->handle);
			draw_quad(p->unk6c, p->unk70 + 10.0f, 128.0f, 64.0f, 255.0f, 128.0f);
		}
	}
	else if(state == 8)
	{
		guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
		guTexSource(find_surface(0x7010)->handle);
		draw_quad(p->unk6c - 10.0f, p->unk70 + 32.0f, 256.0f, 64.0f, 255.0f, 63.0f);
		graphical_printf(text_buffer, "%d", p->time_to_continue / 20 + 1);
		draw_string_scaled_centered(1, text_buffer, p->unk6c, p->unk70 - 32.0f, 0xffffff, 1.5f);
		p->time_to_continue--;
		if(p->time_to_continue < 0)
		{
			p->clicks = 0;
			p->state = 0;
			if(player_stats[0].state == 0 && player_stats[1].state == 0)
			{
				printf("end of game\n");
				game_state[0] = state;
				func_800e7568();
				clean_up_level_sounds();
				remove_all_objects();
				free_blood_drops();
				free_smoke();
				free_sparks();
				free_snow();
				free_flecks();
				free_chunks();
				free_screen_sprites();
				free_world_sprites();
				free_die_list();
				unload_wave();
				player_stats[0].volume = 0x7f;
				player_stats[1].volume = 0x7f;
				player_stats[0].state = 0;
				player_stats[1].state = 0;
				something_with_reload_p1 = 0;
				something_with_reload_p2 = 0;
			}
		}
		alpha = 100.0f;
		if(have_credits_to_continue())
		{
			alpha = 255.0f;
		}
		credit_info(2);
		if(hud_blink < 50)
		{
			draw_string_centered_alpha(1, credit_line1, p->unk6c, 3.0f, 0xffffff, alpha);
		}
		else
		{
			draw_string_centered_alpha(1, credit_line2, p->unk6c, 3.0f, 0xffffff, alpha);
		}
	}
	else if(state == 0 && game_state[0] == 2)
	{
		alpha = 100.0f;
		if(have_credits_to_start())
		{
			alpha = 255.0f;
		}
		credit_info(1);
		if(hud_blink < 50)
		{
			draw_string_centered_alpha(1, credit_line1, p->unk6c, 3.0f, 0xffffff, alpha);
		}
		else
		{
			draw_string_centered_alpha(1, credit_line2, p->unk6c, 3.0f, 0xffffff, alpha);
		}
	}
}

/* 0x800e44e0: show a pickup (or penalty) message: its kind, at full alpha; the status
   display fades it (draw_player_status) */
void show_pickup_message(player_stats_t *p, int kind)
{
	p->unk10c = kind;
	*(float *)&p->unk110 = 255.0f;
}

/* 0x800e4650: an automatic weapon's gauge: a 32 by 128 column at (x - 16, y) filled from
   the bottom by ammo / ammo_max of the texture rows u0..u1 of key + 1 + frame, under the
   frame texture key. w is the player's weapon record (weapon, ammo, ammo_max) */
void draw_ammo_gauge(int *w, float x, float y, int key, float u0, float u1, int frame)
{
	MidVertexWithTA	a, b, c, d;
	float		fill, x0, x1;

	fill = (float)w[1];
	if(fill > 0.0f)
	{
		fill = fill / (float)w[2];
	}
	else
	{
		fill = 0.0f;
	}
	fill = u0 / 128.0f + fill * ((u1 - u0) / 128.0f);
	guTexSource(find_surface(key + 1 + frame)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA);
	grAlphaTestReferenceValue(4);
	x0 = x - 16.0f;
	x1 = x + 16.0f;
	a.oow = b.oow = c.oow = d.oow = HUD_OOW;
	a.a = b.a = c.a = d.a = 188.0f;
	a.x = c.x = x0;
	b.x = d.x = x1;
	a.y = b.y = y;
	c.y = d.y = y + fill * 128.0f;
	a.sow = c.sow = 1.0f * HUD_OOW;
	b.sow = d.sow = 63.0f * HUD_OOW;
	a.tow = b.tow = 255.0f * HUD_OOW;
	c.tow = d.tow = (255.0f - fill * 255.5f) * HUD_OOW;
	func_800d3528(&a, &d, &b);
	func_800d3528(&a, &c, &d);
	guTexSource(find_surface(key)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
	a.a = b.a = c.a = d.a = 118.0f;
	a.x = c.x = x0;
	b.x = d.x = x1;
	a.y = b.y = y;
	c.y = d.y = y + 128.0f;
	a.oow = b.oow = c.oow = d.oow = HUD_OOW;
	a.sow = c.sow = 1.0f * HUD_OOW;
	b.sow = d.sow = 63.0f * HUD_OOW;
	a.tow = b.tow = 255.0f * HUD_OOW;
	c.tow = d.tow = 1.0f * HUD_OOW;
	func_800d3528(&a, &d, &b);
	func_800d3528(&a, &c, &d);
}

/* 0x800e490c: the same for a gauge that does not animate, dir -1 mirrors it (player 2) */
void draw_ammo_gauge_facing(int *w, float x, float y, int key, float u0, float u1, float dir)
{
	MidVertexWithTA	a, b, c, d;
	float		fill, half, x0, x1;

	fill = (float)w[1];
	if(fill > 0.0f)
	{
		fill = fill / (float)w[2];
	}
	else
	{
		fill = 0.0f;
	}
	fill = u0 / 128.0f + fill * ((u1 - u0) / 128.0f);
	guTexSource(find_surface(key + 1)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA_TIMES_ITERATED_ALPHA);
	grAlphaTestReferenceValue(4);
	half = dir * 16.0f;
	x0 = x - half;
	x1 = x + half;
	a.oow = b.oow = c.oow = d.oow = HUD_OOW;
	a.a = b.a = c.a = d.a = 255.0f;
	a.x = c.x = x0;
	b.x = d.x = x1;
	a.y = b.y = y;
	c.y = d.y = y + fill * 128.0f;
	a.sow = c.sow = 1.0f * HUD_OOW;
	b.sow = d.sow = 63.0f * HUD_OOW;
	a.tow = b.tow = 255.0f * HUD_OOW;
	c.tow = d.tow = (255.0f - fill * 255.5f) * HUD_OOW;
	func_800d3528(&a, &d, &b);
	func_800d3528(&a, &c, &d);
	guTexSource(find_surface(key)->handle);
	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
	a.a = b.a = c.a = d.a = 118.0f;
	a.x = c.x = x0;
	b.x = d.x = x1;
	a.y = b.y = y;
	c.y = d.y = y + 128.0f;
	a.oow = b.oow = c.oow = d.oow = HUD_OOW;
	a.sow = c.sow = 1.0f * HUD_OOW;
	b.sow = d.sow = 63.0f * HUD_OOW;
	a.tow = b.tow = 255.0f * HUD_OOW;
	c.tow = d.tow = 1.0f * HUD_OOW;
	func_800d3528(&a, &d, &b);
	func_800d3528(&a, &c, &d);
}

/* 0x800e2fe4: the draw pass' HUD: both players' status, then for each player in the game
   the weapon (bullets for the pistol and shotgun at 20 or 380, a gauge for the automatics)
   and the health bar */
void draw_hud(void)
{
	player_stats_t	*p;
	int		*w;
	int		i, key;
	float		y;

	hud_pulse += hud_pulse_step;
	if(hud_pulse > 0xeeeeee)
	{
		hud_pulse_step = -hud_pulse_step;
	}
	if(hud_pulse <= 0x5fffff)
	{
		hud_pulse_step = -hud_pulse_step;
	}
	hud_gauge_frame++;
	if(hud_gauge_frame >= 3)
	{
		hud_gauge_frame = 0;
	}
	hud_frames++;
	guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
	guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
	grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
	grChromakeyMode(GR_CHROMAKEY_DISABLE);
	grAlphaTestFunction(GR_CMP_GREATER);
	grAlphaTestReferenceValue(8);
	draw_player_status(&player_stats[0]);
	draw_player_status(&player_stats[1]);
	p = &player_stats[0];
	if(player_stats[0].state & 0x24)
	{
		w = &p->weapon;
		if(p->weapon == 0x1000005)
		{
			draw_ammo_gauge(w, 20.0f, 34.0f, 0x6010, 16.0f, 116.0f, hud_gauge_frame);
		}
		else if(p->weapon == 0x1000004)
		{
			draw_ammo_gauge(w, 20.0f, 34.0f, 0x6020, 1.0f, 127.0f, hud_gauge_frame);
		}
		else if(p->weapon & 0x1000000)
		{
			draw_ammo_gauge_facing(w, 20.0f, 34.0f, 0x6030, 7.0f, 120.0f, 1.0f);
		}
		else
		{
			if(p->weapon == 1)
			{
				key = 0x6003;
			}
			else if(p->weapon == 2)
			{
				key = 0x6004;
			}
			guTexSource(find_surface(key)->handle);
			guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
			y = 50.0f;
			for(i = 0; i < w[1]; i++)
			{
				draw_quad(20.0f, y, 32.0f, 32.0f, 255.0f, 255.0f);
				y += 16.0f;
			}
		}
		draw_health_bar(p, 80.0f, 18.0f);
	}
	p = &player_stats[1];
	if(player_stats[1].state == 4)
	{
		guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
		guAlphaSource(GR_ALPHASOURCE_TEXTURE_ALPHA);
		grAlphaBlendFunction(GR_BLEND_SRC_ALPHA, GR_BLEND_ONE_MINUS_SRC_ALPHA, GR_BLEND_ONE, GR_BLEND_ZERO);
		grChromakeyMode(GR_CHROMAKEY_DISABLE);
		grAlphaTestFunction(GR_CMP_GREATER);
		grAlphaTestReferenceValue(4);
		w = &p->weapon;
		if(p->weapon == 0x1000005)
		{
			draw_ammo_gauge(w, 380.0f, 34.0f, 0x6010, 16.0f, 116.0f, hud_gauge_frame);
		}
		else if(p->weapon == 0x1000004)
		{
			draw_ammo_gauge(w, 380.0f, 34.0f, 0x6020, 1.0f, 127.0f, hud_gauge_frame);
		}
		else if(p->weapon & 0x1000000)
		{
			draw_ammo_gauge_facing(w, 380.0f, 34.0f, 0x6030, 7.0f, 120.0f, -1.0f);
		}
		else
		{
			if(p->weapon == 1)
			{
				key = 0x6003;
			}
			else if(p->weapon == 2)
			{
				key = 0x6004;
			}
			guTexSource(find_surface(key)->handle);
			y = 50.0f;
			for(i = 0; i < w[1]; i++)
			{
				draw_quad_mirrored(380.0f, y, 32.0f, 32.0f, 255.0f, 255.0f);
				y += 16.0f;
			}
		}
		draw_health_bar_mirrored(p, 320.0f, 18.0f);
	}
}
