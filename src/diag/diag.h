/*
 * diag.h - what DIAG.EXE's files share (see docs/diag.md). Names are ours unless the SDK or
 * the game gives them.
 */
#ifndef DIAG_H
#define DIAG_H
#include <goose/sprites.h>

/* a menu: a box of items, placed at x, y (negative / -1.0: centred) */
typedef struct menu_item {
	char	*name;
	int	kind;			/* MENU_SUB, MENU_FUNC, MENU_EXIT */
	void	*target;		/* the submenu (menu_t *) or the function (int (*)(void)) */
	char	*help;			/* the line shown under the highlighted item, or 0 */
	void	(*hook)(int);		/* called with the item's number while it is highlighted, or 0 */
} menu_item_t;
#define MENU_SUB	0
#define MENU_FUNC	1
#define MENU_EXIT	2

typedef struct menu {
	char		*title;
	float		x, y;
	int		count;			/* set by do_menu */
	int		sel;			/* the highlighted item */
	menu_item_t	*items;			/* ended by an item without a name */
} menu_t;

/* the coin-door buttons the menus read, as functions that return nonzero while held
   (0x80167d68-0x80167d94, pointing into the input file 0x80118198-) */
extern int	(*sw_up)(void);			/* volume down */
extern int	(*sw_down)(void);		/* volume up */
extern int	(*sw_left)(void);		/* 0x80167d70: nothing on CarnEvil */
extern int	(*sw_right)(void);		/* 0x80167d74: nothing on CarnEvil */
extern int	(*sw_select)(void);		/* test */
extern int	(*sw_back)(void);		/* service credit */
extern int	(*sw_8)(void);			/* 0x80167d88: nothing on CarnEvil */

extern float	hres, vres;			/* GOOSE: the screen size */
extern float	font0_height, font1_height, font2_height;	/* main(): get_font_height(0..2) */

/* draw.c, point.c, title.c, prompt.c */
sprite_info_t	*draw_rect(float, float, float, float, float, int, int);
void	message_box(char *, float, float, float);
void	clear_message_box(void);
void	draw_thick_box(float, float, float, float, float, float, int, int, sprite_info_t **);
void	draw_title(char *);
void	clear_text_box(void);
void	wait_prompt(char *);
void	wait_test_exit(void);
void	wait_start_exit(void);

/* ADJUST.FMT's adjustments, as the format file parser lists them (0x80161ee8) */
typedef struct adjust_node {
	struct adjust_node	*next;
	int			adj_num, min, max, default_val;	/* as GOOSE's adjustment_info_t */
} adjust_node_t;
extern adjust_node_t	*game_adjustments;

int	do_menu(menu_t *);
void	clear_menu(void);
void	clear_menu_box(void);
void	clear_menu_help(void);
#endif
