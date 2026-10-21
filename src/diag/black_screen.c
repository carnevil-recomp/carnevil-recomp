/*
 * black_screen.c - the monitor menu's BLACK SCREEN: the background in one colour until a button.
 */
#include "diag.h"

extern void	func_8012a0a0(int);

/* 0x801044c0 */
int black_screen(void)
{
	func_8012a0a0(0xff000000);
	wait_prompt(0);
	return 0;
}
