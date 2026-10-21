/*
 * red_screen.c - the monitor menu's RED SCREEN: the background in one colour until a button.
 */
#include "diag.h"

extern void	func_8012a0a0(int);

/* 0x80104410 */
int red_screen(void)
{
	func_8012a0a0(0xffff0000);
	wait_prompt(0);
	return 0;
}
