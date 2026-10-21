/*
 * fmt.h - the records the format file scanner (fmtlex.c) builds from AUDITS.FMT and
 * ADJUST.FMT, shared with the audit and adjustment screens.
 */
#ifndef DIAG_FMT_H
#define DIAG_FMT_H

/* one audit of an AUDITS menu item's list (STARTAUDIT ... AUDITDONE), or a page break */
typedef struct audit_fmt {
	struct audit_fmt	*next;
	int	audit;			/* the audit number; -1: PAGEBREAK */
	int	font;
	int	prompt_color;
	int	prompt_adjust;		/* PADJUST */
	int	audit_color;
	int	audit_adjust;		/* AADJUST */
	int	type;			/* DECIMAL 0 .. DODURATION 10; -1: PAGEBREAK */
	char	*prompt;
	int	px, py, ax, ay;		/* -1: placed by the list */
	int	linespace;
	int	columns;
	int	denom;			/* DENOMAUDIT */
} audit_fmt_t;

/* one ADDITIONAL ADJUSTMENT (STARTADJUST ... ADJUSTDONE) */
typedef struct adjust_fmt {
	struct adjust_fmt	*next;
	int	adjustment;		/* the adjustment number */
	int	min, max, def;
	int	control;		/* 1: a list of LISTITEMs */
	int	length;			/* LENGTH, or the number of LISTITEMs */
	int	index;			/* the named ones counted from 0, else -1 */
	char	*list_title;
	char	*list_items[32];
	char	*name;
} adjust_fmt_t;

extern adjust_fmt_t	*additional_adjustments;	/* 0x80161ee8: ADJUST.FMT's STARTADJUST list */
#endif
