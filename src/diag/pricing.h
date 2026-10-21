/*
 * pricing.h - a pricing mode: the coin adjustments in GOOSE's order (LEFT_SLOT_UNITS_ADJ ..
 * CURRENCY_ADJ), then its text. The country tables are in pricing.c; custom modes are read
 * from *.prc files.
 */
#ifndef DIAG_PRICING_H
#define DIAG_PRICING_H

typedef struct pricing {
	int	left_slot_units;
	int	right_slot_units;
	int	center_slot_units;
	int	extra_slot_units;
	int	bill_validator_units;
	int	units_per_credit;
	int	units_per_bonus;
	int	minimum_units;
	int	credits_to_start;
	int	credits_to_continue;
	int	maximum_credits;
	int	coins_per_bill;
	int	show_fractions;
	int	left_chute_count;
	int	right_chute_count;
	int	center_chute_count;
	int	extra_chute_count;
	int	bill_validator_count;
	int	currency;
	char	*info;			/* PRICING_INFO_STRING_ADJ.. */
	char	*name;
	char	*coins;			/* what each slot takes, tab separated */
} pricing_t;

extern pricing_t	**continent_pricing[];	/* 0x80167420 */
#endif
