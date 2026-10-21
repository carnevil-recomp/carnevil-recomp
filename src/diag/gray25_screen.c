/*
 * gray25_screen.c - the monitor menu's 25 PCT. GRAY SCREEN: the background in one colour until a button.
 */
#include "diag.h"

extern void	func_8012a0a0(int);

/* 0x80104518 */
int gray25_screen(void)
{
	func_8012a0a0(0xff3f3f3f);
	wait_prompt(0);
	return 0;
}
