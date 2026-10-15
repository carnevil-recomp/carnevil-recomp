/* taur.h - TAUR's glow records (taur.c, taur_data.c) */
#ifndef TAUR_H
#define TAUR_H
typedef struct taur_glow
{
	int	group;			/* the part that glows */
	int	count;			/* frames in this state */
	int	state;			/* 0 dark, 4 lighting up, 1 lit, 3 fading */
	int	pad;
} taur_glow_t;				/* sizeof = 0x10 */
#endif
