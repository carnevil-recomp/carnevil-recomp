/*
 * gray50_screen.c - the monitor menu's 50 PCT. GRAY SCREEN: the background in one colour until a button.
 */
#include "diag.h"

extern void	func_8012a0a0(int);

/* 0x801044e8 */
int gray50_screen(void)
{
	func_8012a0a0(0xff7f7f7f);
	wait_prompt(0);
	return 0;
}
