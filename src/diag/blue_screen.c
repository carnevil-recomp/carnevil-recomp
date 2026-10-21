/*
 * blue_screen.c - the monitor menu's BLUE SCREEN: the background in one colour until a button.
 */
#include "diag.h"

extern void	func_8012a0a0(int);

/* 0x80104468 */
int blue_screen(void)
{
	func_8012a0a0(0xff0000ff);
	wait_prompt(0);
	return 0;
}
