/*
 * camera.c - the level camera and the light-gun interface (0x800d4070-0x800d4f6c).
 *
 * Each level has a camera file (HHOU.RAW ...: one 28-byte entry per movie frame: position
 * in tenths of a unit, three angles, the lens) loaded by load_level_camera; update_camera()
 * moves the view to the entry of the frame on screen and rebuilds the view matrix. The
 * projection is four scales derived from the field of view: proj_x/proj_y map world units to
 * pixels, cull_x/cull_y are the same for a view ten degrees wider.
 *
 * The gun board's control byte lives at 0xb7f00000 (reached as the variable nss_control by
 * the kss_ routines and through a pointer by gun_board_reset, as the code shows) and its
 * switch bytes at 0xb6800004 (P1) and 0xb6800014 (P2); the calibration constants are the
 * factory defaults, kss_reset_w_calib_ takes them from the CMOS instead.
 */
#include <stdio.h>
#include <stdlib.h>
#include "credits.h"			/* debug_mode */
#include "waves.h"
#include "camera.h"

extern float	fatan(float), ftan(float);			/* vecmat.c */
extern void	rot_matrix_neg_zxy(float *, float *), vec_negate(float *, float *);
extern void	add_snow_around(float *pos);			/* something drawn around the camera when camera_follow is on */
extern void	debug_move_camera(void);			/* 0x800c93c4: the player 3/4 switches move the camera */
extern int	load_camera_file(void **, int, char *, int);	/* loaders.c */
extern int	get_adjustment(int, int *), set_adjustment(int, int);
void	cmos_calibration_(void);			/* 0x800d4d00 */
extern void	gun_trigger(int player, int bit), gun_pump(player_stats_t *player, int bit);

void		*camera_data;		/* 0x801e5cbc: the level's camera file, 0 when none */
int		camera_data_count;	/* 0x801e70fc: entries in it */
float		camera_x, camera_y, camera_z;		/* 0x801de8f0..f8: the view point */
float		camera_rx, camera_ry, camera_rz;	/* 0x801de890..98: its angles */
float		view_matrix[9];		/* 0x801fad48 */
float		camera_neg_pos[3];	/* 0x801e5b58: -camera, for the view transform */
int		camera_follow;		/* 0x801e5dd4: run add_snow_around on the camera position */
float		fov_x, fov_y;		/* 0x801e5d98/9c: the field of view in degrees */
float		cull_x, cull_y;		/* 0x801e5d8c/90 */
float		proj_x;			/* 0x801e5db0 */
float		proj_y;			/* 0x801e5db4 */
float		screen_aspect;		/* 0x801e5dc8: 400 / 256 */
float		screen_w, screen_h;	/* 0x8023744c, 0x80237424 */
int		half_height = 128;	/* 0x8017dfd8 */
gun_t		guns[2];		/* 0x801e70b0 (player 1), 0x801e70d4 (player 2) */
int		guns_active;		/* 0x80179d54 (.data) */
int		D_80212da4, D_80212da8;
extern int	accuracy_tracking;					/* shoot.c */
int		gun_min_x, gun_max_x, gun_min_y, gun_max_y;	/* 0x801e0e10/18/1c/20: the gun's raw range */

extern signed char nss_control;				/* 0xb7f00000: the gun board's control byte, as a variable */
#define GUN_P1		((volatile unsigned char *)0xb6800004)
#define GUN_P2		((volatile unsigned char *)0xb6800014)
#define DEG		0.0174533f
#define CAMERA_ENTRY	28
#define VEC_Y(v)	(*(float *)((char *)(v) + 4))
#define VEC_Z(v)	(*(float *)((char *)(v) + 8))

/* the four projection scales for a view of the given angles (degrees): proj_x/y are the
   distance at which a unit is a pixel, cull_x/y the slope of a view ten degrees wider (the
   division round trip is the original's) */
#define SET_PROJECTION(fx, fy) \
{ \
	float	d; \
	d = 200.0f / ftan(((fx) + 10.0f) / 2.0f * DEG); \
	cull_x = 200.0f / d; \
	d = (float)half_height / ftan(((fy) + 10.0f) / 2.0f * DEG); \
	cull_y = (float)half_height / d; \
	proj_x = 200.0f / ftan((fx) / 2.0f * DEG); \
	proj_y = (float)half_height / ftan((fy) / 2.0f * DEG); \
}

/* 0x800d4070: the projection for a view of the given angles (clamped to 11..169) */
void set_field_of_view(float fx, float fy)
{
	if(fx > 169.0f) fx = 169.0f;
	if(fx < 11.0f) fx = 11.0f;
	if(fy > 169.0f) fy = 169.0f;
	if(fy < 11.0f) fy = 11.0f;
	SET_PROJECTION(fx, fy);
}

/* 0x800d41b4: the camera for a movie frame */
void update_camera(int frame)
{
	char	*p;
	float	lens, fx, fy;

	if(debug_mode & DEBUG_NO_CAMERA)
	{
		if(!(debug_mode & (DEBUG_POINT | DEBUG_VECTORS)))
		{
			debug_move_camera();
		}
	}
	else if(!camera_data)
	{
		camera_x = 0;
		camera_y = 0;
		camera_z = 0;
		camera_rx = 0;
		camera_ry = 0;
		camera_rz = 0;
	}
	else if(frame >= camera_data_count)
	{
		p = (char *)(camera_data_count * CAMERA_ENTRY + (long)camera_data - CAMERA_ENTRY);
		camera_x = *(float *)p * 10.0f;
		camera_y = *(float *)(p + 4) * 10.0f;
		camera_z = *(float *)(p + 8) * 10.0f;
		camera_rx = *(float *)(p + 12);
		camera_ry = *(float *)(p + 16);
		camera_rz = *(float *)(p + 20);
	}
	else
	{
		p = (char *)(frame * CAMERA_ENTRY + (long)camera_data);
		camera_x = *(float *)p * 10.0f;
		camera_y = *(float *)(p + 4) * 10.0f;
		camera_z = *(float *)(p + 8) * 10.0f;
		camera_rx = *(float *)(p + 12);
		camera_ry = *(float *)(p + 16);
		camera_rz = *(float *)(p + 20);
		lens = *(float *)(p + 24);
		fx = 2.0f * fatan(1.5625f / lens);		/* 400/256: the screen's aspect */
		fy = 2.0f * fatan(1.0f / lens);
		set_field_of_view(fx * 180.0f / 3.14159f, fy * 180.0f / 3.14159f);
	}
	rot_matrix_neg_zxy(view_matrix, &camera_rx);
	vec_negate(&camera_x, camera_neg_pos);
	if(camera_follow)
	{
		add_snow_around(&camera_x);
	}
}

/* 0x800d43dc: the view matrix for the camera as it stands */
void update_view_matrix(void)
{
	rot_matrix_neg_zxy(view_matrix, &camera_rx);
	vec_negate(&camera_x, camera_neg_pos);
}

/* 0x800d4424: no camera file, the camera at the origin, the default lens */
void init_camera(void)
{
	camera_data = 0;
	camera_x = 0;
	camera_y = 0;
	camera_z = 0;
	screen_w = 400.0f;
	screen_h = 256.0f;
	screen_aspect = 1.5625f;
	fov_x = 67.74f;
	fov_y = 53.13f;
	SET_PROJECTION(fov_x, fov_y);
	camera_rx = 0;
	camera_ry = 0;
	camera_rz = 0;
	camera_follow = 0;
}

/* 0x800d45a4, 0x800d45ac */
void func_800d45a4(void)
{
}

void func_800d45ac(void)
{
}

/* 0x800d45b4 */
void get_camera_pos(float *out)
{
	*out = camera_x;
	VEC_Y(out) = camera_y;
	VEC_Z(out) = camera_z;
}

/* 0x800d45dc: the camera file of a level (the attract waves share the gallery's) */
void load_level_camera(int wave)
{
	void	*data;

	if(camera_data)
	{
		printf("Error, already have a camera!\n");
	}
	camera_follow = 0;
	switch(wave)
	{
	case WAVE_HHOUSE:
		camera_data_count = 0xccf;
		load_camera_file(&data, 0xccf, "HHOU.RAW", 0x2b38bce1);
		break;
	case WAVE_FREAKSHOW:
		camera_data_count = 0xef7;
		load_camera_file(&data, 0xef7, "FREK.RAW", 0x82a506da);
		break;
	case WAVE_RICKITY:
		camera_data_count = 0xf56;
		load_camera_file(&data, 0xf56, "RICK.RAW", 0x79e8f699);
		break;
	case WAVE_BIGTOP:
		camera_data_count = 0x109b;
		load_camera_file(&data, 0x109b, "BTOP.RAW", 0xc1f5071d);
		break;
	case WAVE_ATTRACT:
	case WAVE_HISCORE_ENTRY:
	case WAVE_LEVEL_SELECT:
		camera_data_count = 0x3d;
		load_camera_file(&data, 0x3d, "GALL.RAW", 0x4bc0df8f);
		break;
	default:
		printf("Hit unknown wave in load level camera - 0x%08x\n", wave);
		camera_data_count = 0;
		data = 0;
		break;
	}
	fov_x = 52.05f;
	fov_y = 34.71f;
	SET_PROJECTION(fov_x, fov_y);
	camera_data = data;
}

/* 0x800d48a8 */
void free_level_camera(void)
{
	if(camera_data)
	{
		free(camera_data);
	}
	camera_follow = 0;
	camera_data = 0;
}

/* 0x800d48e4, 0x800d48f8 */
void enable_camera_follow(void)
{
	camera_follow = 1;
}

void disable_camera_follow(void)
{
	camera_follow = 0;
}

/* 0x800d4908: reset the gun board (bit 4 of its control byte) */
int gun_board_reset(void)
{
	signed char	*control = (signed char *)0xb7f00000;
	char		v;

	v = *control;
	printf("NSS reset = %x\n", v);
	*control = v | 0x10;
	return(0);
}

/* 0x800d4954, 0x800d4968 */
void enable_guns(void)
{
	guns_active = 1;
}

void disable_guns(void)
{
	guns_active = 0;
}

/* 0x800d4978 */
void func_800d4978(void)
{
	D_80212da4 = 0;
	D_80212da8 = 0;
}

/* 0x800d4990, 0x800d49c4, 0x800d49f8: both guns' bit 4, both guns' bit 5, either gun's bit 4 */
int both_guns_bit4(void)
{
	return((*GUN_P1 & 0x10) && (*GUN_P2 & 0x10));
}

int both_guns_bit5(void)
{
	return((*GUN_P1 & 0x20) && (*GUN_P2 & 0x20));
}

int either_gun_bit4(void)
{
	return((*GUN_P1 & 0x10) || (*GUN_P2 & 0x10));
}

/* 0x800d4a34: feed each gun's two switch bits to their handlers */
void poll_guns(void)
{
	if(guns_active)
	{
		gun_trigger(0, *GUN_P1 & 0x10);
		gun_trigger(1, *GUN_P2 & 0x10);
		gun_pump(&player_stats[0], *GUN_P1 & 0x20);
		gun_pump(&player_stats[1], *GUN_P2 & 0x20);
	}
}

/* 0x800d4ac8: factory calibration and a board reset */
int kss_reset_(void)
{
	char	v;

	guns[0].scale_x = 4.0235300f;
	guns[0].scale_y = 0.94736803f;
	guns[0].off_x = 285.52942f;
	guns[0].off_y = 25.052631f;
	guns[1].scale_x = 4.0117650f;
	guns[1].scale_y = 0.89473701f;
	guns[1].off_x = 271.76471f;
	guns[1].off_y = 39.105263f;
	v = nss_control;
	printf("NSS reset = %x\n", v);
	nss_control = v | 0x10;
	accuracy_tracking = 0;
	return(2);
}

/* 0x800d4b98: the CMOS calibration and a board reset */
int kss_reset_w_calib_(void)
{
	char	v;

	v = nss_control;
	printf("NSS reset = %x\n", v);
	nss_control = v | 0x10;
	cmos_calibration_();
	accuracy_tracking = 0;
	return(2);
}

/* 0x800d4bf0: take the "calibrate" request (adjustment 0x39): 1 when it was set, and clear it */
int take_calibration_request(void)
{
	int	val;

	if(get_adjustment(0x39, &val) == 0)
	{
		if(val == 0)
		{
			return(0);
		}
	}
	set_adjustment(0x39, 0);
	return(1);
}

/* 0x800d4c34: a player's calibration from the gun's raw range */
void default_calibration(int player)
{
	guns[player].scale_x = 3.8611112f;
	gun_min_x = 330;
	guns[player].off_x = ((float)gun_min_x / guns[player].scale_x - 20.0f) * guns[player].scale_x;
	guns[player].scale_y = 0.85185188f;
	gun_min_y = 36;
	guns[player].off_y = ((float)gun_min_y / guns[player].scale_y - 20.0f) * guns[player].scale_y;
	gun_max_x = 1720;
	gun_max_y = 220;
}

extern int	set_float_adjustment(int, float), get_float_adjustment(int, float *, float);	/* adjust.c */
extern int	set_adjustment(int, int);
extern int	do_draw_background;
extern int	D_801de8a0;			/* shoot.c: the calibration step */
int	D_80179d50 = 0;			/* 0x80179d50 (.data): 1 once the guns may be sampled every vblank */

/* 0x800d4d00: the guns' calibration from the CMOS adjustments 0x53-0x6a (each in triplicate),
   guarded by a magic word at 0x52 and the "calibrate" request at 0x39: when any of that is
   wrong the factory values are written back first */
void cmos_calibration_(void)
{
	int	bad;
	int	magic;
	int	req;
	int	phase;
	int	r;

	bad = 0;
	D_80179d50 = 1;
	D_801de8a0 = 0;
	guns[0].scale_x = -1.0f;
	guns[0].scale_y = -1.0f;
	guns[1].scale_x = -1.0f;
	guns[1].scale_y = -1.0f;
	if(get_adjustment(0x52, &magic))
	{
		printf("bad cmos valid calib\n");
		bad = 1;
	}
	if(magic != 0x23a4729)
	{
		printf("invalid calib, got %d (0x%08X), wanted %d\n", magic, magic, 0x23a4729);
		bad = 1;
	}
	if(get_adjustment(0x39, &req) == 0 && req == 0)
	{
		r = 0;
	}
	else
	{
		set_adjustment(0x39, 0);
		r = 1;
	}
	if(r)
	{
		printf("samhack calib\n");
		bad = 1;
	}
	if(bad)
	{
		phase = game_phase;
		do_draw_background = 1;
		default_calibration(0);
		default_calibration(1);
		game_phase = phase;
		set_float_adjustment(0x53, guns[0].scale_x);
		set_float_adjustment(0x56, guns[0].scale_y);
		set_float_adjustment(0x59, guns[0].off_x);
		set_float_adjustment(0x5c, guns[0].off_y);
		set_float_adjustment(0x5f, guns[1].scale_x);
		set_float_adjustment(0x62, guns[1].scale_y);
		set_float_adjustment(0x65, guns[1].off_x);
		set_float_adjustment(0x68, guns[1].off_y);
		set_adjustment(0x52, 0x23a4729);
	}
	get_float_adjustment(0x53, &guns[0].scale_x, 4.0235300f);
	get_float_adjustment(0x56, &guns[0].scale_y, 0.94736803f);
	get_float_adjustment(0x59, &guns[0].off_x, 285.52942f);
	get_float_adjustment(0x5c, &guns[0].off_y, 25.052631f);
	get_float_adjustment(0x5f, &guns[1].scale_x, 4.0117650f);
	get_float_adjustment(0x62, &guns[1].scale_y, 0.89473701f);
	get_float_adjustment(0x65, &guns[1].off_x, 271.76471f);
	get_float_adjustment(0x68, &guns[1].off_y, 39.105263f);
	D_801de8a0 = 0;
}
