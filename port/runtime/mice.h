/* per-device mice and light guns (--mice), see mice.c */
#ifndef PORT_MICE_H
#define PORT_MICE_H
#include <stdint.h>
extern int	port_mice;
extern float	port_mice_speed;
void	mice_rel(uintptr_t id, float dx, float dy);	/* counts, y down */
void	mice_abs(uintptr_t id, float bx, float by);	/* drawable pixels, origin bottom left */
void	mice_button(uintptr_t id, int which, int down);	/* 0 left, 1 right, 2 middle */
int	mice_open(void);				/* the backend's device reader: 0 = unavailable */
void	mice_poll(void);
void	mice_sim(unsigned long frame);			/* CARNEVIL_MICESIM (tests) */
#endif
