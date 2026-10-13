/*
 * camera.h - the level camera, the projection and the light guns (src/game/camera.c,
 * 0x800d4070-0x800d4f6c).
 */
#ifndef CAMERA_H
#define CAMERA_H

/* a light gun's record (0x801e70b0 player 1, 0x801e70d4 player 2) */
typedef struct gun
{
	int	unk0, unk4, unk8;
	float	scale_x, scale_y;	/* +0x0c, +0x10: raw gun units per pixel */
	float	off_x, off_y;		/* +0x14, +0x18 */
	int	unk1c, unk20;
} gun_t;

extern gun_t	guns[2];
extern void	*camera_data;			/* the level's camera file, 0 when none */
extern int	camera_data_count;
extern float	camera_x, camera_y, camera_z;	/* 0x801de8f0..f8: the view point */
extern float	camera_rx, camera_ry, camera_rz;	/* 0x801de890..98: its angles */
extern float	view_matrix[9], camera_neg_pos[3];
extern int	camera_follow;
extern float	fov_x, fov_y, cull_x, cull_y, proj_x, proj_y, screen_aspect, screen_w, screen_h;
extern int	half_height, guns_active;

void	set_field_of_view(float x_degrees, float y_degrees);
void	update_camera(int frame);
void	update_view_matrix(void);
void	init_camera(void);
void	get_camera_pos(float *out);
void	load_level_camera(int wave);
void	free_level_camera(void);
void	enable_camera_follow(void), disable_camera_follow(void);
int	gun_board_reset(void);
void	enable_guns(void), disable_guns(void);
int	both_guns_bit4(void), both_guns_bit5(void), either_gun_bit4(void);
void	poll_guns(void);
int	kss_reset_(void), kss_reset_w_calib_(void);
int	take_calibration_request(void);
void	default_calibration(int player);

#endif
