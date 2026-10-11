/* Host hardware abstraction for the CarnEvil port: what GOOSE expects from the BIOS/board. */
#ifndef PORT_HAL_H
#define PORT_HAL_H
void hal_init(int argc, char **argv);
void hal_wait_frame(int frames);          /* paces to the display refresh, fires the vblank handler */
int  hal_refresh_hz(void);
extern const char *hal_partition_root;    /* directory holding the extracted LFN32 partition */
extern int hal_frame_limit;               /* --frames N: stop after N frames (0 = run forever) */
extern int hal_fast;                      /* --fast: do not sleep between frames */
extern unsigned long hal_frame_count;
#endif
