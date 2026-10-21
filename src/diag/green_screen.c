/*
 * green_screen.c - the monitor menu's GREEN SCREEN: the background in one colour until a button.
 */
#include "diag.h"

extern void	func_8012a0a0(int);

/* 0x80104438 */
int green_screen(void)
{
	func_8012a0a0(0xff00ff00);
	wait_prompt(0);
	return 0;
}
