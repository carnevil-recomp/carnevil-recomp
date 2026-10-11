/* $Revision: 2 $ */

char	goose_draw_c_version[] = {"$Revision: 2 $"};

int	background_color = 0xff000000;
int	draw_unused = 0x73;		/* present in the game's .data (revision 2 of this file); not referenced by any code */

void set_bgnd_color(int color)
{
	background_color = color;
}

int get_bgnd_color(void)
{
	return(background_color);
}
