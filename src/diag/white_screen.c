/*
 * white_screen.c - the monitor menu's WHITE SCREEN: the background in one colour until a button.
 */
#include "diag.h"

extern void	func_8012a0a0(int);

/* 0x80104498 */
int white_screen(void)
{
	func_8012a0a0(0xffffffff);
	wait_prompt(0);
	return 0;
}
