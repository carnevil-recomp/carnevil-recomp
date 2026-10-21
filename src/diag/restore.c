/*
 * restore.c - restoring the factory settings (0x80114848-0x80114b90): FULL FACTORY RESTORE
 * (adjustments and audits), the adjustments alone, and RESET HIGH SCORES. The game's own
 * adjustments, listed in ADJUST.FMT, are restored from that list.
 */
#include <goose/adjust.h>
#include "diag.h"

extern int	get_adjustment(int, int *);
extern int	set_adjustment(int, int);
extern int	dialog(char *, float, float, float, int);
extern int	clear_all_audits(int);

static void	restore_game_adjustments(void);
static int	check_game_adjustments(void);

/* 0x80114848 */
int factory_restore(void)
{
	int	err;

	err = dialog("RESTORE ADJUSTMENTS & AUDITS ?", -1.0F, -1.0F, 1.0F, 3);
	if(err == 1)
	{
		restore_factory_adjustments(0, 0);
		restore_game_adjustments();
		if(!check_system_adjustments(0, 0))
			err = 0;
		if(check_game_adjustments())
			err |= 2;
		clear_all_audits(-1);
		if(err)
			message_box("FAILURE PERFORMING FACTORY RESTORE", -1.0F, -1.0F, 1.0F);
		else
			message_box("AUDITS & ADJUSTMENTS RESTORED", -1.0F, -1.0F, 1.0F);
	}
	else
		message_box("FACTORY RESTORE ABORTED", -1.0F, -1.0F, 1.0F);
	wait_test_exit();
	return 0;
}

/* 0x80114930 */
int default_adjustments(void)
{
	int	err;

	err = dialog("RESTORE ADJUSTMENTS ?", -1.0F, -1.0F, 1.0F, 3);
	if(err == 1)
	{
		restore_factory_adjustments(0, 0);
		restore_game_adjustments();
		if(!check_system_adjustments(0, 0))
			err = 0;
		if(check_game_adjustments())
			err |= 2;
		if(err)
			message_box("FAILURE PERFORMING AJUSTMENTS RESTORE", -1.0F, -1.0F, 1.0F);
		else
			message_box("ADJUSTMENTS RESTORED", -1.0F, -1.0F, 1.0F);
	}
	else
		message_box("ADJUSTMENTS RESTORE ABORTED", -1.0F, -1.0F, 1.0F);
	wait_test_exit();
	return 0;
}

/* 0x80114a10 */
int reset_high_scores(void)
{
	if(dialog("RESET HIGH SCORES ?", -1.0F, -1.0F, 1.0F, 3) == 1)
	{
		set_adjustment(HIGH_SCORE_RESET_ADJ, 1);
		message_box("HIGH SCORES RESET", -1.0F, -1.0F, 1.0F);
	}
	else
		message_box("RESET HIGH SCORES ABORTED", -1.0F, -1.0F, 1.0F);
	wait_test_exit();
	return 0;
}

/* 0x80114ab0 */
static void restore_game_adjustments(void)
{
	adjust_node_t	*p;

	for(p = game_adjustments; p; p = p->next)
		set_adjustment(p->adj_num, p->default_val);
}

/* 0x80114af8: 1 if one could not be set */
static int check_game_adjustments(void)
{
	adjust_node_t	*p;
	int	val, err = 0;

	for(p = game_adjustments; p; p = p->next)
	{
		if(get_adjustment(p->adj_num, &val))
		{
			if(set_adjustment(p->adj_num, p->default_val))
				err = 1;
		}
		else if(val != p->default_val)
		{
			if(set_adjustment(p->adj_num, p->default_val))
				err = 1;
		}
	}
	return err;
}
