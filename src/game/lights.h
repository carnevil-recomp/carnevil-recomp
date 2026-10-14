/*
 * lights.h - the level lights (src/game/lights.c, 0x800d6ef0-0x800d8c60). A level's light
 * table (lights_data.c) lists light groups with the movie frames they switch on and off at;
 * update_lights() keeps the live light records in step with the movie, and build_light_list()
 * turns the records into the directional lights light_vertices() shades a model with.
 */
#ifndef LIGHTS_H
#define LIGHTS_H

#define MAX_LIGHTS	0x6e

/* a light table entry (56 bytes) */
typedef struct light_info
{
	int	start;			/* 0x00: the movie frame it comes on */
	int	end;			/* 0x04: the frame it goes off */
	int	type;			/* 0x08: 2 directional, 4 point (1 unused) */
	int	flags;			/* 0x0c: 1 on, 4 attached to the camera, 0x40 fades from the previous
					   entry's colour, 0x80 fades towards the next entry's colour */
	float	v[3];			/* 0x10: a position in tenths (point) or two angles in degrees (directional) */
	float	color[3];		/* 0x1c: 0..255 */
	float	falloff;		/* 0x28: a point light's range in tenths */
	float	intensity;		/* 0x2c: a directional light's strength */
	int	unk30;
	int	life;			/* 0x34: a camera light's lifetime in frames */
} light_info_t;

/* a live light record (0x48 bytes), on the list rooted at light_root */
typedef struct level_light
{
	int	id;			/* 0x00: wave | group, or a dynamic id */
	int	type;			/* 0x04 */
	int	flags;			/* 0x08: as above, plus 0x10 = counts life down */
	float	pos[3];			/* 0x0c: a point light's position, a directional light's direction */
	float	color[3];		/* 0x18 */
	float	dcolor[3];		/* 0x24: the colour step per frame while fading */
	int	unk30, unk34, unk38;
	float	falloff;		/* 0x3c */
	int	life;			/* 0x40 */
	struct level_light *next;	/* 0x44 */
} level_light_t;

/* a directional light built for one model (0x20 bytes) */
typedef struct light
{
	float		dir[3];		/* 0x00: scaled by the point light's attenuation */
	float		color[3];	/* 0x0c */
	struct light	*next;		/* 0x18 */
	int		pad;
} light_t;

extern level_light_t	light_root;		/* 0x8017c1d0: flags bit 0 = everything lit by its colour (ambient_override) */
extern light_t		*light_list;		/* 0x8017c2a8 */
extern light_info_t	*level_light_info_global;	/* 0x8021f084 */
extern int		num_light_groups;	/* 0x8021f0d4 */

extern light_info_t	hiscore_light_info[6], hhouse_light_info[0x44], freakshow_light_info[0x30], rickitytown_light_info[0x24], bigtop_light_info[3], attract_light_info[2];

void	load_level_lights(int wave);
void	free_lights(void);
void	remove_light_group(int n);
void	add_light_group(int n);
void	init_lights(void);
int	add_dynamic_light(int flags, float *pos, float *color, float falloff, int life);
void	update_lights(int frame);
void	set_ambient_override(float r, float g, float b);
void	clear_ambient_override(void);
void	light_vertices(int n, float *normals, float *colors);
void	build_light_list(float *pos);
int	append_light_record(level_light_t *l);
int	move_light(int id, float *pos);
int	check_if_light_exists(int id);
int	remove_light(int id);
int	free_light_records(void);
int	append_light(light_t *l);
int	free_light_list(void);
int	next_light_id(void);
int	add_light_to_camera_(int flags, float *color, float falloff, int life);
void	print_light_records(void);

#endif
