/* --mice: every mouse, light gun or DolphinBar is read on its own (the system merges them into one
   pointer), and the first two to click become players 1 and 2, like the cabinet's two guns.

   The video backends report raw device input here: relative motion in device counts (a mouse) or
   an absolute position already turned into drawable pixels (a light gun, a DolphinBar, a tablet).
   Buttons: left is the trigger, right reloads (points the gun off the screen and fires, as the
   cabinet does), middle is the pump. A light gun that reports leaving the screen reloads by
   itself, as on the cabinet. */
#include <stdio.h>
#include <stdint.h>
#include "hal.h"
#include "mice.h"

int	port_mice;			/* --mice */
float	port_mice_speed = 0.25f;	/* --mouse-speed: game pixels per count for an ordinary mouse */

#define MAX_DEVS	16
static struct {
	uintptr_t	id;
	int		player;		/* -1 until it clicks */
	float		x, y;
	int		on;
	unsigned int	buttons;
} devs[MAX_DEVS];
static int	ndevs, next_player;

extern int video_to_game(float wx, float wy, float *gx, float *gy);

static int find(uintptr_t id)
{
	int	i;
	for(i = 0; i < ndevs; i++)
		if(devs[i].id == id) return i;
	if(ndevs == MAX_DEVS) return -1;
	devs[ndevs].id = id; devs[ndevs].player = -1;
	devs[ndevs].x = 200.0f; devs[ndevs].y = 128.0f; devs[ndevs].on = 1;
	return ndevs++;
}
static void report(int i)
{
	if(devs[i].player < 0) return;
	hal_gun_aim(devs[i].player, devs[i].x, devs[i].y, devs[i].on);
	hal_gun_buttons(devs[i].player, devs[i].buttons);
}
void mice_rel(uintptr_t id, float dx, float dy)
{
	int i = find(id);
	if(i < 0) return;
	devs[i].x += dx * port_mice_speed;
	devs[i].y -= dy * port_mice_speed;		/* device y grows downwards, the game's upwards */
	if(devs[i].x < 0) devs[i].x = 0; else if(devs[i].x > 400) devs[i].x = 400;
	if(devs[i].y < 0) devs[i].y = 0; else if(devs[i].y > 256) devs[i].y = 256;
	devs[i].on = 1;
	report(i);
}
void mice_abs(uintptr_t id, float bx, float by)
{
	int i = find(id);
	if(i < 0) return;
	devs[i].on = video_to_game(bx, by, &devs[i].x, &devs[i].y);
	report(i);
}
void mice_button(uintptr_t id, int which, int down)
{
	static const unsigned int bit[3] = { PAD_TRIGGER, PAD_RELOAD, PAD_PUMP };
	int i = find(id);
	if(i < 0 || which < 0 || which > 2) return;
	if(down && devs[i].player < 0 && next_player < 2)
	{
		devs[i].player = next_player++;
		fprintf(stderr, "[mice] device %#lx is player %d's gun\n", (unsigned long)id, devs[i].player + 1);
	}
	if(down) devs[i].buttons |= bit[which]; else devs[i].buttons &= ~bit[which];
	report(i);
}

/* tests: CARNEVIL_MICESIM="FRAME:DEVICE:EVENT;..." with EVENT rel:DX,DY | down:BUTTON | up:BUTTON
   stands in for device input (hal.c calls mice_sim each frame) */
#include <stdlib.h>
#include <string.h>
static struct { unsigned long at; unsigned id; char kind[8]; float a, b; } sim[64];
static int	nsim = -1;
void mice_sim(unsigned long frame)
{
	int	i;
	if(nsim < 0)
	{
		const char *e = getenv("CARNEVIL_MICESIM");
		const char *tok;
		nsim = 0;
		if(!e) return;
		for(tok = e; tok && *tok && nsim < 64; tok = strchr(tok, ';') ? strchr(tok, ';') + 1 : NULL)
			if(sscanf(tok, "%lu:%u:%7[a-z]:%f,%f", &sim[nsim].at, &sim[nsim].id, sim[nsim].kind, &sim[nsim].a, &sim[nsim].b) >= 4) nsim++;
		if(nsim) port_mice = 1;
	}
	for(i = 0; i < nsim; i++)
	{
		if(sim[i].at != frame) continue;
		if(!strcmp(sim[i].kind, "rel")) mice_rel(sim[i].id, sim[i].a, sim[i].b);
		else if(!strcmp(sim[i].kind, "down")) mice_button(sim[i].id, (int)sim[i].a, 1);
		else if(!strcmp(sim[i].kind, "up")) mice_button(sim[i].id, (int)sim[i].a, 0);
	}
}
