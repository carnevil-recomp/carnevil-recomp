/*
 * sndtest.c - SOUND SUBSYSTEM TEST (0x801105a0-0x80110a50): resets the sound board and shows
 * its boot and SDRC versions, the port, the program checksum, its SRAM, DRAM and tone tests
 * and the operating system it then loads; and the pricing menu's country hook.
 */
#include <goose/sprites.h>
#include <goose/ostrings.h>
#include <goose/sound.h>
#include "diag.h"

extern void	sleep(int);
extern void	main_init_sound(void);

int	pricing_country;

/* 0x801105a0: the country menu's hook */
void set_pricing_country(int n)
{
	pricing_country = n;
}

/* 0x801105b0 */
int sound_test(void)
{
	int	v;
	float	x, y;

	draw_title("SOUND SUBSYSTEM TEST");
	snd_reset();
	v = (unsigned short)snd_get_boot_version();
	set_text_font(0);
	x = hres / 2.0F;
	y = vres * 0.75F;
	set_text_position((int)x, (int)y, 1.0F);
	set_text_justification_mode(2);
	oprintf("\x1b%dcBOOT VERSION: \x1b%dc%X.%02X", -1, 0xff00ff00, v >> 8, v & 0xff);
	sleep(1);

	v = (unsigned short)snd_get_sdrc_version();
	y -= font0_height * 1.5F;
	set_text_position((int)x, (int)y, 1.0F);
	oprintf("\x1b%dcSDRC VERSION: \x1b%dc%X", -1, 0xff00ff00, v);
	sleep(1);

	v = (unsigned short)snd_test_port();
	y -= font0_height * 1.5F;
	set_text_position((int)x, (int)y, 1.0F);
	oprintf("\x1b%dcPORT STATUS: \x1b%dc%s", -1, v == 0xeeee ? 0xffff0000 : 0xff00ff00, v == 0xeeee ? "BAD" : "GOOD");
	sleep(1);

	v = (unsigned short)snd_get_pmint_sum();
	y -= font0_height * 1.5F;
	set_text_position((int)x, (int)y, 1.0F);
	oprintf("\x1b%dcCHECKSUM: \x1b%dc%X", -1, 0xff00ff00, v);
	sleep(1);

	snd_send_command(0x3a);
	v = (unsigned short)snd_wait_for_completion();
	y -= font0_height * 1.5F;
	set_text_position((int)x, (int)y, 1.0F);
	switch(v)
	{
		case 0xcc01:
			oprintf("\x1b%dcSRAM:  \x1b%dcOK", -1, 0xff00ff00);
			break;
		case 0xee01:
			oprintf("\x1b%dcSRAM:  \x1b%dcSRAM 1 FAILED", -1, 0xffff0000);
			break;
		case 0xee02:
			oprintf("\x1b%dcSRAM:  \x1b%dcSRAM 2 FAILED", -1, 0xffff0000);
			break;
		case 0xee03:
			oprintf("\x1b%dcSRAM:  \x1b%dcSRAM 3 FAILED", -1, 0xffff0000);
			break;
		default:
			oprintf("\x1b%dcSRAM:  \x1b%dcUNKNOWN ERROR", -1, 0xffff0000);
			break;
	}
	sleep(1);

	snd_send_command(0x4a);
	v = (unsigned short)snd_wait_for_completion();
	y -= font0_height * 1.5F;
	set_text_position((int)x, (int)y, 1.0F);
	switch(v)
	{
		case 0xcc02:
			oprintf("\x1b%dcDRAM:  \x1b%dcOK", -1, 0xff00ff00);
			break;
		case 0xee04:
			oprintf("\x1b%dcDRAM:  \x1b%dcDRAM FAILED", -1, 0xffff0000);
			break;
		default:
			oprintf("\x1b%dcDRAM:  \x1b%dcUNKNOWN ERROR", -1, 0xffff0000);
			break;
	}
	sleep(1);

	snd_send_command(0xaa);
	v = (unsigned short)snd_wait_for_completion();
	y -= font0_height * 1.5F;
	set_text_position((int)x, (int)y, 1.0F);
	oprintf("\x1b%dcTONE STATUS: \x1b%dc%s", -1, v != 0xcc04 ? 0xffff0000 : 0xff00ff00, v == 0xcc04 ? "GOOD" : "BAD");
	sleep(1);

	snd_load_opsys();
	v = (unsigned short)snd_get_opsys_ver();
	y -= font0_height * 1.5F;
	set_text_position((int)x, (int)y, 1.0F);
	oprintf("\x1b%dcOS VERSION: \x1b%dc%X.%02X", -1, 0xff00ff00, v >> 8, v & 0xff);
	main_init_sound();
	wait_test_exit();
	delete_multiple_strings(0, 0);
	del1c(0, 0);
	return 0;
}
