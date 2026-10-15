/*
 * enemy.c - helpers shared by the enemy types: health by the health adjustment, placing an
 * enemy on a path point and following a path, and the reach test for an attack. One original
 * file (0x800fb310-0x800fd100); the steering and facing routines in it are still assembly.
 */
#include "objects.h"
#include "enemy.h"
#include "model.h"
#include "credits.h"			/* player_stats */
#include "camera.h"			/* camera_x/y/z */
#include "waves.h"

extern int	D_80237438;		/* adjust.c: adjustment 0x32 (0..4) */

/* 0x800fb4a0: the enemy's health scaled by adjustment 0x32 */
void set_enemy_health(enemy_t *e, float health)
{
	switch(D_80237438)
	{
	case 0:
		e->health = health * 0.5f;
		break;
	case 1:
		e->health = health * 0.75f;
		break;
	case 2:
		e->health = health;
		break;
	case 3:
		e->health = health * 1.25f;
		break;
	case 4:
		e->health = health * 1.5f;
		break;
	default:
		e->health = health;
		break;
	}
}

/* 0x800fb51c: the same with a flatter scale (0.8, 0.9, 1, 1.25, 1.5) */
void set_enemy_health2(enemy_t *e, float health)
{
	switch(D_80237438)
	{
	case 0:
		e->health = health * 0.8f;
		break;
	case 1:
		e->health = health * 0.9f;
		break;
	case 2:
		e->health = health;
		break;
	case 3:
		e->health = health * 1.25f;
		break;
	case 4:
		e->health = health * 1.5f;
		break;
	default:
		e->health = health;
		break;
	}
}

/* 0x800fb70c: put the enemy on path point n: a position (scaled) and three angles */
void path_place(enemy_t *e, float *path, int n, float scale)
{
	float	*p;

	p = path + n * 6;
	e->x = p[0] * scale;
	e->y = p[1] * scale;
	e->z = p[2] * scale;
	e->rx = p[3];
	e->ry = p[4];
	e->rz = p[5];
}

/* 0x800fb760: the same without the roll */
void path_place_upright(enemy_t *e, float *path, int n, float scale)
{
	float	*p;

	p = path + n * 6;
	e->x = p[0] * scale;
	e->y = p[1] * scale;
	e->z = p[2] * scale;
	e->rx = p[3];
	e->ry = p[4];
	e->rz = 0;
}

/* 0x800fb7b0: path_place, then advance along the path while a player is in the game */
void path_follow(enemy_t *e, float *path, int n, float scale)
{
	float	*p;

	p = path + n * 6;
	e->x = p[0] * scale;
	e->y = p[1] * scale;
	e->z = p[2] * scale;
	e->rx = p[3];
	e->ry = p[4];
	e->rz = p[5];
	if((player_stats[0].state & 0x44) || (player_stats[1].state & 0x44))
	{
		e->path++;
	}
}

/* 0x800fb83c: path_follow facing the other way */
void path_follow_turned(enemy_t *e, float *path, int n, float scale)
{
	float	*p;

	p = path + n * 6;
	e->x = p[0] * scale;
	e->y = p[1] * scale;
	e->z = p[2] * scale;
	e->rx = p[3];
	e->ry = p[4] + 3.1415927f;
	e->rz = p[5];
	if((player_stats[0].state & 0x44) || (player_stats[1].state & 0x44))
	{
		e->path++;
	}
}

/* 0x800fb8d4: path_place facing the other way */
void path_place_turned(enemy_t *e, float *path, int n, float scale)
{
	float	*p;

	p = path + n * 6;
	e->x = p[0] * scale;
	e->y = p[1] * scale;
	e->z = p[2] * scale;
	e->rx = p[3];
	e->ry = p[4] + 3.1415927f;
	e->rz = p[5];
}

/* 0x800fcfc4: a visible group within r of the view point (an attack lands) */
int group_near_camera(group_t *g, float r)
{
	if(!(g->flags & 0x2000))
	{
		return(0);
	}
	if((g->pos[0] - camera_x) * (g->pos[0] - camera_x) + (g->pos[1] - camera_y) * (g->pos[1] - camera_y) + (g->pos[2] - camera_z) * (g->pos[2] - camera_z) < r * r)
	{
		return(1);
	}
	return(0);
}

extern float	fatan(float);
extern float	ffabs(float);
int	move_cd(enemy_t *, float, float, float);

/* 0x800fc550: angles_to on the turn alone: rot gets the heading from from to to, no pitch or
   roll (Mari's thrown axe) */
void func_800fc550(float *from, float *to, float *rot)
{
	float	d[3], a;

	d[0] = to[0] - from[0];
	d[1] = to[1] - from[1];
	d[2] = to[2] - from[2];
	if(d[2] == 0.0f)
	{
		if(d[0] > 0.0f)
		{
			rot[1] = 1.570796f;
		}
		else
		{
			rot[1] = 4.712388f;
		}
	}
	else
	{
		a = fatan(ffabs(d[0] / d[2]));
		if(d[0] > 0.0f)
		{
			if(d[2] > 0.0f)
			{
				rot[1] = 3.1415927f - a;
			}
			else
			{
				rot[1] = a;
			}
		}
		else
		{
			if(d[2] > 0.0f)
			{
				rot[1] = a + 3.1415927f;
			}
			else
			{
				rot[1] = 6.2831855f - a;
			}
		}
	}
	rot[0] = 0;
	rot[2] = 0;
}

/* 0x800fc66c: turn the enemy towards the view point by at most rate radians: 1 when it turned,
   0 when it already faces it (within the rate) */
int face_camera(enemy_t *e, float rate)
{
	float	d[3];
	float	v[3];
	float	a, diff;

	d[0] = e->x - camera_x;
	d[1] = e->y - camera_y;
	d[2] = e->z - camera_z;
	if(d[2] == 0.0f)
	{
		if(0.0f < d[0])
		{
			v[1] = 1.570796f;
		}
		else
		{
			v[1] = 4.712388f;
		}
	}
	else
	{
		a = fatan(ffabs(d[0] / d[2]));
		if(0.0f < d[0])
		{
			if(0.0f < d[2])
			{
				v[1] = 3.1415927f - a;
			}
			else
			{
				v[1] = a;
			}
		}
		else
		{
			if(0.0f < d[2])
			{
				v[1] = a + 3.1415927f;
			}
			else
			{
				v[1] = 6.2831855f - a;
			}
		}
	}
	v[0] = 0;
	v[2] = 0;
	diff = v[1] - e->ry;
	if(diff < -3.1415927f)
	{
		diff += 6.2831855f;
		while(diff < -3.1415927f)
		{
			diff += 6.2831855f;
		}
	}
	if(3.1415927f < diff)
	{
		diff -= 6.2831855f;
		while(3.1415927f < diff)
		{
			diff -= 6.2831855f;
		}
	}
	if(diff == 0.0f)
	{
		return(0);
	}
	if(0.0f < diff ? diff < rate : rate < diff)
	{
		return(0);
	}
	if(diff < -3.1415927f)
	{
		e->ry = e->ry + rate;
	}
	else if(diff < 0.0f)
	{
		e->ry = e->ry - rate;
	}
	else if(diff < 3.1415927f)
	{
		e->ry = e->ry + rate;
	}
	else
	{
		e->ry = e->ry - rate;
	}
	return(1);
}

/* ---- steering (0x800fb934-0x800fc0f4) ---- */

extern void	world_to_view(float *, float *), view_to_world(float *);	/* vecmat.c */
extern int	loaded_wave;
extern float	fatan(float), ffabs(float), fsqrt(float);		/* vecmat.c, objects.c */
extern void	rot_y(float *, float *), rot_matrix_neg_zyx(float *, float *), rot_matrix_neg_yxz(float *, float *), mat_vec_mul(float *, float *, float *);
extern int	func_800cd094(float *, float *, int), func_800cd31c(float *, float *, int), step_blocked_rickity(float *, float *, int), func_800cd904(float *, float *, int);
extern int	printf(const char *, ...);

/* 0x800fb934: the enemy's goal: a point dz in front of from (the view point), dy below it
   and side to the left for player 1, the right for player 2 */
void set_goal_near_camera(enemy_t *e, float *from, float *goal, float side, float dy, float dz)
{
	world_to_view(from, goal);
	goal[2] = goal[2] - dz;
	goal[1] = goal[1] - dy;
	if(e->target == player_stats)
	{
		goal[0] = goal[0] - side;
	}
	else
	{
		goal[0] = goal[0] + side;
	}
	view_to_world(goal);
}

/* 0x800fbaac: come at the camera: turn towards it (0.2 rad at most), set the goal side to the
   left (player 1) or right of the view point, dy below it and dz beyond it, and take a free
   step of at most `step` towards it: 2 when the goal was within the step (reached), else 0 */
int func_800fc16c(enemy_t *e, float step);
int func_800fbaac(enemy_t *e, float side, float dy, float dz, float step)
{
	float	*goal;

	face_camera(e, 0.2f);
	goal = e->goal;
	world_to_view(&camera_x, goal);
	goal[2] = goal[2] - dz;
	goal[1] = goal[1] - dy;
	if(e->target == player_stats)
	{
		goal[0] = goal[0] - side;
	}
	else
	{
		goal[0] = goal[0] + side;
	}
	view_to_world(goal);
	return((func_800fc16c(e, step) != 0) * 2);
}

int	step_toward_goal_xz(enemy_t *, float);

/* 0x800fbba0: face the camera, set the goal as set_goal_near_camera does (side, dy, dz) and
   take a ground step towards it: 2 when the goal is reached or the step was clear, else 0 */
int func_800fbba0(enemy_t *e, float side, float dy, float dz, float step)
{
	float	*goal;

	face_camera(e, 0.2f);
	goal = e->goal;
	world_to_view(&camera_x, goal);
	e->goal[2] = e->goal[2] - dz;
	e->goal[1] = e->goal[1] - dy;
	if(e->target == player_stats)
	{
		e->goal[0] = e->goal[0] - side;
	}
	else
	{
		e->goal[0] = e->goal[0] + side;
	}
	view_to_world(goal);
	return((step_toward_goal_xz(e, step) != 0) << 1);
}

/* 0x800fbc6c: a step of at most `step` on the largest axis towards the goal: 2 when the goal
   is within a step (the enemy snaps to it), else whether the move was clear */
int step_toward_goal(enemy_t *e, float step)
{
	float	dx, dy, dz;
	float	ax, ay, az;
	float	mx, my, mz;

	dx = e->goal[0] - e->hit_group->pos[0];
	dy = e->goal[1] - e->hit_group->pos[1];
	dz = e->goal[2] - e->hit_group->pos[2];
	ax = ffabs(dx);
	ay = ffabs(dy);
	az = ffabs(dz);
	if(ax < step && ay < step && az < step)
	{
		e->x = e->x + dx;
		e->y = e->y + dy;
		e->z = e->z + dz;
		return(2);
	}
	if(ay < ax && az < ax)
	{
		step = step / ax;
	}
	else if(ax < ay && az < ay)
	{
		step = step / ay;
	}
	else
	{
		step = step / az;
	}
	mx = ffabs(dx * step);
	my = ffabs(dy * step);
	mz = ffabs(dz * step);
	if(mx < dx)
	{
		dx = mx;
	}
	else if(dx < -mx)
	{
		dx = -mx;
	}
	if(my < dy)
	{
		dy = my;
	}
	else if(dy < -my)
	{
		dy = -my;
	}
	if(mz < dz)
	{
		dz = mz;
	}
	else if(dz < -mz)
	{
		dz = -mz;
	}
	return(move_cd(e, dx, dy, dz) == 0);
}

/* 0x800fbe5c: the same on the ground (x and z only) */
int step_toward_goal_xz(enemy_t *e, float step)
{
	float	dx, dz;
	float	ax, az;
	float	mx, mz;

	dx = e->goal[0] - e->hit_group->pos[0];
	dz = e->goal[2] - e->hit_group->pos[2];
	ax = ffabs(dx);
	az = ffabs(dz);
	if(ax < step && az < step)
	{
		e->x = e->x + dx;
		e->z = e->z + dz;
		return(2);
	}
	if(az < ax)
	{
		step = step / ax;
	}
	else
	{
		step = step / az;
	}
	mx = ffabs(dx * step);
	mz = ffabs(dz * step);
	if(mx < dx)
	{
		dx = mx;
	}
	else if(dx < -mx)
	{
		dx = -mx;
	}
	if(mz < dz)
	{
		dz = mz;
	}
	else if(dz < -mz)
	{
		dz = -mz;
	}
	return(move_cd(e, dx, 0.0f, dz) == 0);
}

/* 0x800fbfa0: move by (dx, dy, dz) unless the level's collision test objects: 1 when moved */
int move_cd(enemy_t *e, float dx, float dy, float dz)
{
	float	np[3];
	int	blocked;

	np[0] = e->x + dx;
	np[1] = e->y + dy;
	np[2] = e->z + dz;
	switch(loaded_wave)
	{
	case WAVE_HHOUSE:
		blocked = func_800cd094(np, &e->x, e->id);
		break;
	case WAVE_FREAKSHOW:
		blocked = func_800cd31c(np, &e->x, e->id);
		break;
	case WAVE_RICKITY:
		blocked = step_blocked_rickity(np, &e->x, e->id);
		break;
	case WAVE_BIGTOP:
		blocked = func_800cd904(np, &e->x, e->id);
		break;
	default:
		printf("unknown wave in move_cd!\n");
		blocked = 1;
		break;
	}
	if(blocked)
	{
		return(0);
	}
	e->x = e->x + dx;
	e->y = e->y + dy;
	e->z = e->z + dz;
	return(1);
}

/* 0x800fc16c: step_toward_goal measured from the enemy's own position and moved without the
   collision test (func_800cde74): 1 when the goal was within a step (it moves there), else 0 */
extern int	func_800cde74(enemy_t *, float, float, float);
int func_800fc16c(enemy_t *e, float step)
{
	float	dx, dy, dz;
	float	ax, ay, az;
	float	mx, my, mz;

	dx = e->goal[0] - e->x;
	dy = e->goal[1] - e->y;
	dz = e->goal[2] - e->z;
	ax = ffabs(dx);
	ay = ffabs(dy);
	az = ffabs(dz);
	if(ax < step && ay < step && az < step)
	{
		func_800cde74(e, dx, dy, dz);
		return(1);
	}
	if(ay < ax && az < ax)
	{
		step = step / ax;
	}
	else if(ax < ay && az < ay)
	{
		step = step / ay;
	}
	else
	{
		step = step / az;
	}
	mx = ffabs(dx * step);
	my = ffabs(dy * step);
	mz = ffabs(dz * step);
	if(mx < dx)
	{
		dx = mx;
	}
	else if(dx < -mx)
	{
		dx = -mx;
	}
	if(my < dy)
	{
		dy = my;
	}
	else if(dy < -my)
	{
		dy = -my;
	}
	if(mz < dz)
	{
		dz = mz;
	}
	else if(dz < -mz)
	{
		dz = -mz;
	}
	func_800cde74(e, dx, dy, dz);
	return(0);
}

/* 0x800fc34c: the pitch (rot[0]) and yaw (rot[1]) that point from `from` at `to`; rot[2] = 0.
   The yaw comes from the x and z difference, the pitch from the y difference and the depth
   once the difference is turned by that yaw */
void angles_to(float *from, float *to, float *rot)
{
	float	d[3], m[9], a;

	d[0] = to[0] - from[0];
	d[1] = to[1] - from[1];
	d[2] = to[2] - from[2];
	if(d[2] == 0.0f)
	{
		if(d[0] >= 0.0f)
		{
			rot[1] = 1.5707960f;
		}
		else
		{
			rot[1] = 4.7123880f;
		}
	}
	else
	{
		a = fatan(ffabs(d[0] / d[2]));
		if(d[0] >= 0.0f)
		{
			if(d[2] >= 0.0f)
			{
				rot[1] = 3.1415927f - a;
			}
			else
			{
				rot[1] = a;
			}
		}
		else
		{
			if(d[2] >= 0.0f)
			{
				rot[1] = a + 3.1415927f;
			}
			else
			{
				rot[1] = 6.2831855f - a;
			}
		}
	}
	rot_y(m, rot);
	mat_vec_mul(d, m, d);
	if(d[2] == 0.0f)
	{
		if(d[1] >= 0.0f)
		{
			rot[0] = 1.5707960f;
		}
		else
		{
			rot[0] = 4.7123880f;
		}
	}
	else
	{
		a = fatan(ffabs(d[1] / d[2]));
		if(d[1] <= 0.0f)
		{
			if(d[2] >= 0.0f)
			{
				rot[0] = 3.1415927f - a;
			}
			else
			{
				rot[0] = 6.2831855f - a;
			}
		}
		else
		{
			if(d[2] >= 0.0f)
			{
				rot[0] = a + 3.1415927f;
			}
			else
			{
				rot[0] = a;
			}
		}
	}
	rot[2] = 0.0f;
}

/* 0x800fd6b0: the enemy's angles become those looking from the point 100 units along its
   heading (the rotation by minus its yaw applied to (0, 0, 100)) back at the origin */
void func_800fd6b0(enemy_t *e)
{
	float	origin[3] = { 0.0f, 0.0f, 0.0f };
	float	ahead[3] = { 0.0f, 0.0f, 100.0f };
	float	m[9], p[3], angles[3];

	angles[0] = angles[2] = 0.0f;
	angles[1] = e->ry;
	rot_matrix_neg_zyx(m, angles);
	mat_vec_mul(ahead, m, p);
	angles_to(p, origin, &e->rx);
}

/* 0x800fd03c: is the enemy within r of the camera */
int near_camera(enemy_t *e, float r)
{
	float	dx, dy, dz;

	dx = e->x - camera_x;
	dx = dx * dx;
	dy = e->y - camera_y;
	dy = dy * dy;
	dz = e->z - camera_z;
	dz = dz * dz;
	if(dx + dy + dz < r * r)
	{
		return 1;
	}
	return 0;
}

/* 0x800fd1e8: a point in a group's own space moved into the world */
void group_point(group_t *g, float *p)
{
	mat_vec_mul(p, g->mat, p);
	p[0] = p[0] + g->pos[0];
	p[1] = p[1] + g->pos[1];
	p[2] = p[2] + g->pos[2];
}

/* 0x800fd258: the velocity that carries the enemy to the camera: the vector to it scaled
   so its largest component is speed, each component clamped to the distance left on its
   axis (it never overshoots in one step) */
void aim_at_camera(enemy_t *e, float speed, float *vel)
{
	float	dx, dy, dz, ax, ay, az;

	dx = camera_x - e->x;
	dy = camera_y - e->y;
	dz = camera_z - e->z;
	ax = ffabs(dx);
	ay = ffabs(dy);
	az = ffabs(dz);
	if(ay < ax && az < ax)
	{
		speed = speed / ax;
	}
	else if(ax < ay && az < ay)
	{
		speed = speed / ay;
	}
	else
	{
		speed = speed / az;
	}
	ax = ffabs(dx * speed);
	ay = ffabs(dy * speed);
	az = ffabs(dz * speed);
	if(ax < dx)
	{
		dx = ax;
	}
	else if(dx < -ax)
	{
		dx = -ax;
	}
	if(ay < dy)
	{
		dy = ay;
	}
	else if(dy < -ay)
	{
		dy = -ay;
	}
	if(az < dz)
	{
		dz = az;
	}
	else if(dz < -az)
	{
		dz = -az;
	}
	vel[0] = dx;
	vel[1] = dy;
	vel[2] = dz;
}

/* 0x800fd3f0: the same toward a point */
void aim_at_point(enemy_t *e, float speed, float *vel, float *target)
{
	float	dx, dy, dz, ax, ay, az;

	dx = target[0] - e->x;
	dy = target[1] - e->y;
	dz = target[2] - e->z;
	ax = ffabs(dx);
	ay = ffabs(dy);
	az = ffabs(dz);
	if(ay < ax && az < ax)
	{
		speed = speed / ax;
	}
	else if(ax < ay && az < ay)
	{
		speed = speed / ay;
	}
	else
	{
		speed = speed / az;
	}
	ax = ffabs(dx * speed);
	ay = ffabs(dy * speed);
	az = ffabs(dz * speed);
	if(ax < dx)
	{
		dx = ax;
	}
	else if(dx < -ax)
	{
		dx = -ax;
	}
	if(ay < dy)
	{
		dy = ay;
	}
	else if(dy < -ay)
	{
		dy = -ay;
	}
	if(az < dz)
	{
		dz = az;
	}
	else if(dz < -az)
	{
		dz = -az;
	}
	vel[0] = dx;
	vel[1] = dy;
	vel[2] = dz;
}

/* 0x800fd614: the enemy's goal becomes a velocity of speed across the camera's line of
   sight to it (minus speed along the x axis turned by the yaw from the camera) */
void knock_sideways(enemy_t *e, float speed)
{
	float	v[3], angles[3], m[9];

	angles_to(camera_pos, &e->x, angles);
	angles[0] = angles[2] = 0.0f;
	rot_matrix_neg_zyx(m, angles);
	v[0] = -speed;
	v[2] = v[1] = 0.0f;
	mat_vec_mul(v, m, e->goal);
}

/* 0x800fd7b8: the velocity that lobs a thing from pos to target in frames frames under
   the 0.22 a frame gravity the thrown objects fall with (the fifth argument is unused) */
void lob_toward(float *pos, float *target, float *vel, float frames, int unused)
{
	float	goal[3], angles[3], m[9], d[3];
	float	dist, a, vy, vz;

	goal[0] = target[0];
	goal[2] = target[2];
	goal[1] = pos[1];
	dist = fsqrt((pos[0] - goal[0]) * (pos[0] - goal[0]) + (pos[1] - goal[1]) * (pos[1] - goal[1]) + (pos[2] - goal[2]) * (pos[2] - goal[2]));
	vy = (target[1] - pos[1]) / frames;
	vz = dist / frames;
	vy = vy + frames * 0.11f;
	vel[0] = 0.0f;
	vel[2] = vz;
	vel[1] = vy;
	d[0] = pos[0] - target[0];
	d[1] = pos[1] - target[1];
	d[2] = pos[2] - target[2];
	if(d[2] == 0.0f)
	{
		if(d[0] > 0.0f)
		{
			angles[1] = 1.5707964f;
		}
		else
		{
			angles[1] = 4.712389f;
		}
	}
	else
	{
		a = fatan(ffabs(d[0] / d[2]));
		if(d[0] > 0.0f)
		{
			if(d[2] > 0.0f)
			{
				angles[1] = 3.1415927f - a;
			}
			else
			{
				angles[1] = a;
			}
		}
		else
		{
			if(d[2] > 0.0f)
			{
				angles[1] = a + 3.1415927f;
			}
			else
			{
				angles[1] = 6.2831855f - a;
			}
		}
	}
	angles[0] = 0.0f;
	angles[2] = 0.0f;
	rot_matrix_neg_yxz(m, angles);
	mat_vec_mul(vel, m, vel);
}

/* 0x800fd764 */
float distance_between(float *a, float *b)
{
	return fsqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
}

/* 0x800fcd70: face_camera towards any point: turn the enemy towards to by at most rate radians;
   1 when it turned, 0 when it already faces it (within the rate) */
int func_800fcd70(enemy_t *e, float *to, float rate)
{
	float	v[3];
	float	d[3];
	float	a, diff;

	d[0] = e->x - to[0];
	d[1] = e->y - to[1];
	d[2] = e->z - to[2];
	if(d[2] == 0.0f)
	{
		if(0.0f < d[0])
		{
			v[1] = 1.570796f;
		}
		else
		{
			v[1] = 4.712388f;
		}
	}
	else
	{
		a = fatan(ffabs(d[0] / d[2]));
		if(0.0f < d[0])
		{
			if(0.0f < d[2])
			{
				v[1] = 3.1415927f - a;
			}
			else
			{
				v[1] = a;
			}
		}
		else
		{
			if(0.0f < d[2])
			{
				v[1] = a + 3.1415927f;
			}
			else
			{
				v[1] = 6.2831855f - a;
			}
		}
	}
	v[0] = 0;
	v[2] = 0;
	diff = v[1] - e->ry;
	if(diff < -3.1415927f)
	{
		diff += 6.2831855f;
		while(diff < -3.1415927f)
		{
			diff += 6.2831855f;
		}
	}
	if(3.1415927f < diff)
	{
		diff -= 6.2831855f;
		while(3.1415927f < diff)
		{
			diff -= 6.2831855f;
		}
	}
	if(diff == 0.0f)
	{
		return(0);
	}
	if(0.0f < diff ? diff < rate : rate < diff)
	{
		return(0);
	}
	if(diff < -3.1415927f)
	{
		e->ry = e->ry + rate;
	}
	else if(diff < 0.0f)
	{
		e->ry = e->ry - rate;
	}
	else if(diff < 3.1415927f)
	{
		e->ry = e->ry + rate;
	}
	else
	{
		e->ry = e->ry - rate;
	}
	return(1);
}

/* 0x800fd0a8: step straight at the goal on x and z, at most step on each axis (the larger
   one scaled to step, the other in proportion), without the collision test: 0 when the goal
   was within a step on both axes (it moves there), else 1 */
int func_800fd0a8(enemy_t *e, float step)
{
	float	dx, dz;
	float	ax, az;

	dx = e->goal[0] - e->hit_group->pos[0];
	dz = e->goal[2] - e->hit_group->pos[2];
	ax = ffabs(dx);
	az = ffabs(dz);
	if(ax < step && az < step)
	{
		func_800cde74(e, dx, 0.0f, dz);
		return(0);
	}
	if(az < ax)
	{
		step = step / ax;
	}
	else
	{
		step = step / az;
	}
	ax = ffabs(dx * step);
	az = ffabs(dz * step);
	if(ax < dx)
	{
		dx = ax;
	}
	else if(dx < -ax)
	{
		dx = -ax;
	}
	if(az < dz)
	{
		dz = az;
	}
	else if(dz < -az)
	{
		dz = -az;
	}
	func_800cde74(e, dx, 0.0f, dz);
	return(1);
}
