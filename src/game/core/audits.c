/*
 * audits.c - the game's audit groups (0x800f18e0-0x800f1d6c): cleared and checked by id list.
 * Built at -O (the matcher says so); adjust.c next to it is -O2.
 */
#include <stdio.h>

extern int	get_audit(int, int *), set_audit(int, int), increment_audit(int);

/* audit id groups (0x8018ea60..) */
static int	audits_coins[11] = { 0, 1, 2, 4, 5, 3, 0xb, 0xc, 0xd, 0x21, 0x22 };
static int	audits_credits[1] = { 0xa };
static int	audits_general[0x2c] = { 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32 };
static int	audits_levels[0x3d] = { 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f };
static int	audits_system[0x15] = { 6, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x81, 0x7f, 0x80, 0x82, 0x83, 0x84 };
static int	audits_weapons[0x30] = { 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa2, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3 };

/* 0x800f18e0: zero a group of audits, stopping at the first that cannot be read back */
static void clear_audits(int n, int *ids)
{
	int	val;

	while(n--)
	{
		set_audit(*ids, 0);
		if(get_audit(*ids, &val))
		{
			return;
		}
		ids++;
	}
}

/* 0x800f1948-0x800f19f8 */
void clear_coin_audits(void)	{ clear_audits(11, audits_coins); }
void clear_credit_audits(void)	{ clear_audits(1, audits_credits); }
void clear_game_audits(void)	{ clear_audits(0x2c, audits_general); clear_audits(0x3d, audits_levels); }
void clear_system_audits(void)	{ clear_audits(0x15, audits_system); }
void clear_weapon_audits(void)	{ clear_audits(0x30, audits_weapons); }

/* 0x800f1a20 */
void clear_all_audits(void)
{
	clear_coin_audits();
	clear_credit_audits();
	clear_game_audits();
	clear_system_audits();
	clear_weapon_audits();
}

/* 0x800f1a60: every audit in the group must be readable, clearing those that are not; 0 = failure */
static int check_audits(int n, int *ids)
{
	int	val;

	while(n--)
	{
		if(get_audit(*ids, &val))
		{
			set_audit(*ids, 0);
			if(get_audit(*ids, &val))
			{
				return(0);
			}
		}
		ids++;
	}
	return(1);
}

/* 0x800f1ae4-0x800f1b84 */
int check_coin_audits(void)	{ return(check_audits(11, audits_coins)); }
int check_credit_audits(void)	{ return(check_audits(1, audits_credits)); }
int check_game_audits(void)	{ return(check_audits(0x2c, audits_general)); }
int check_system_audits(void)	{ return(check_audits(0x15, audits_system)); }
int check_weapon_audits(void)	{ return(check_audits(0x30, audits_weapons)); }

/* 0x800f1bac: 1 when every audit group reads back */
int check_all_audits(void)
{
	int	ok;

	ok = check_coin_audits() != 0;
	if(check_credit_audits() == 0) ok = 0;
	if(check_game_audits() == 0) ok = 0;
	if(check_system_audits() == 0) ok = 0;
	if(check_weapon_audits() == 0) ok = 0;
	return(ok);
}

/* 0x800f1c14: count audit a, then audit c = audit a as a percentage of audit b */
void set_percent_audit_inc(int a, int b, int c)
{
	int	total, part;

	increment_audit(a);
	if(get_audit(b, &total) == 0 && total > 0)
	{
		get_audit(a, &part);
		part = part * 100;
		part = part / total;
		set_audit(c, part);
	}
}

/* 0x800f1cd4: audit c = audit a as a percentage of audit b */
void set_percent_audit(int a, int b, int c)
{
	int	total, part;

	if(get_audit(b, &total) == 0 && total > 0)
	{
		get_audit(a, &part);
		part = part * 100;
		part = part / total;
		set_audit(c, part);
	}
}

