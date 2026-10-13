/*
 * shake.c - the movie's vertical offset: screen shake and the one-shot scroll (0x800f3580,
 * from a file that also holds 0x800f3420-0x800f37ac, not yet decompiled).
 */
#include "movie.h"

/* screen shake / scroll (0x800f3580) */
int		shake_count;			/* 0x801e0e6c */
float		shake_amp;			/* 0x801e09dc */
float		shake_decay;			/* 0x80212dc4 */
int		scroll_request;			/* 0x8018f130 */
int		scroll_value = 4;		/* 0x8018f134 */
int		movie_scroll_center = 128;	/* 0x8017dfd8 */


/* 0x800f3580: vertical offset of the movie for the screen shake and one-shot scroll */
void update_movie_scroll(void)
{
	int	a;

	if(shake_count)
	{
		a = (int)shake_amp;
		movie.scroll = a;
		if(shake_count & 1)
		{
			movie.scroll = -a;
		}
		shake_amp = shake_amp - shake_decay;
		shake_count--;
	}
	else if(scroll_request)
	{
		movie.scroll = scroll_value;
		scroll_request = 0;
		scroll_value = -scroll_value;
	}
	else
	{
		movie.scroll = 0;
	}
	movie_scroll_center = (int)((float)movie.scroll + 128.0f);
}

