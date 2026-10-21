/*
 * prompt.c - waiting for the player to leave a screen (0x80101390-0x80101614): an optional
 * message in a box, then test or service credit (or whatever sw_select and sw_back point at),
 * a sound, and the button let go.
 */
#include "diag.h"

extern void	sleep(int);
extern int	snd_scall_bank(char *, int, int, int, int);
extern void	message_box(char *, float, float, float);
extern void	clear_message_box(void);

/* 0x80101390 */
inline void wait_prompt(char *msg)
{
	if(msg)
		message_box(msg, -1.0F, 5.0F, 1.0F);
	while(!sw_select() && !sw_back())
		sleep(1);
	snd_scall_bank("ttest", 11, 180, 127, 0x8000);
	while(sw_select() || sw_back())
		sleep(1);
}

/* 0x8010145c */
void wait_test_exit(void)
{
	wait_prompt("PRESS CREDIT OR TEST TO EXIT");
	clear_message_box();
}

/* 0x80101538 */
void wait_start_exit(void)
{
	wait_prompt("PRESS ANY START BUTTON TO EXIT");
	clear_message_box();
}
