/* Host hardware abstraction for the CarnEvil port: what GOOSE expects from the BIOS/board. */
#ifndef PORT_HAL_H
#define PORT_HAL_H
void hal_init(int argc, char **argv);
void hal_wait_frame(int frames);          /* paces to the display refresh, fires the vblank handler */
int  hal_refresh_hz(void);
extern const char *hal_partition_root;    /* directory holding the extracted LFN32 partition */
extern const char *hal_raw_image;         /* --raw: the whole disk image (movies live in raw partitions) */
extern int hal_frame_limit;               /* --frames N: stop after N frames (0 = run forever) */
extern int hal_fast;                      /* --fast: do not sleep between frames */
extern int hal_headless;                  /* --headless: no window */
extern unsigned long hal_frame_count;
extern const char *hal_state_root;
void hal_key(const char *name, int down);
/* gamepads: the video backend reports each connected pad once a frame (pad 0 plays player 1,
   pad 1 player 2): the left stick in -1..1 (y up) and the PAD_* buttons held */
#define PAD_TRIGGER	0x01
#define PAD_PUMP	0x02
#define PAD_START	0x04
#define PAD_COIN	0x08
#define PAD_RELOAD	0x10
void hal_pad(int pad, float lx, float ly, unsigned int buttons);
extern int port_crosshair;		/* --crosshair: 1 (default) draws the port's crosshairs */
int  hal_crosshair(int player, float *gx, float *gy);	/* 1 when there is a crosshair to draw for that player (mouse or pad aim): where */
void hal_mouse(float gx, float gy, int on);   /* the mouse as player 1's gun: game-screen x 0..400, y 0..256 from the bottom; on = over the picture */  /* a cabinet switch by name (see hal.c), pressed or released */
#endif
void	stub_reached(const char *name);
