/*
 * vecmat.c - 3x3 matrices, rotations and the series approximations the 3D code uses
 * (0x800cfb18-0x800d1750: the file also holds get_screen_pos and the projection helpers,
 * still in assembly). Matrices are nine floats, row major; a rotation about an axis takes
 * the angle from the matching element of a three-float array and negates it.
 */
extern float	fsin(float), fcos(float);		/* GOOSE sincos.c */

/* 0x800cfb18 */
void mat_copy(float *src, float *dst)
{
	dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
	dst[3] = src[3]; dst[4] = src[4]; dst[5] = src[5];
	dst[6] = src[6]; dst[7] = src[7]; dst[8] = src[8];
}

/* 0x800cfb64: out = a * b (out may be either input: the product is built in a temporary) */
void mat_mul(float *a, float *b, float *out)
{
	float	t[9];

	t[0] = a[0] * b[0] + a[1] * b[3] + a[2] * b[6];
	t[1] = a[0] * b[1] + a[1] * b[4] + a[2] * b[7];
	t[2] = a[0] * b[2] + a[1] * b[5] + a[2] * b[8];
	t[3] = a[3] * b[0] + a[4] * b[3] + a[5] * b[6];
	t[4] = a[3] * b[1] + a[4] * b[4] + a[5] * b[7];
	t[5] = a[3] * b[2] + a[4] * b[5] + a[5] * b[8];
	t[6] = a[6] * b[0] + a[7] * b[3] + a[8] * b[6];
	t[7] = a[6] * b[1] + a[7] * b[4] + a[8] * b[7];
	t[8] = a[6] * b[2] + a[7] * b[5] + a[8] * b[8];
	out[0] = t[0]; out[1] = t[1]; out[2] = t[2];
	out[3] = t[3]; out[4] = t[4]; out[5] = t[5];
	out[6] = t[6]; out[7] = t[7]; out[8] = t[8];
}

/* 0x800d0528: rotation about x by -angles[0] */
void rot_x(float *m, float *angles)
{
	float	c, s;

	c = fcos(-angles[0]);
	s = fsin(-angles[0]);
	m[0] = 1.0f; m[1] = 0; m[2] = 0;
	m[3] = 0; m[4] = c; m[5] = s;
	m[6] = 0; m[7] = -s; m[8] = c;
}

/* 0x800d05ac: rotation about y by -angles[1] */
void rot_y(float *m, float *angles)
{
	float	c, s;

	c = fcos(-angles[1]);
	s = fsin(-angles[1]);
	m[0] = c; m[1] = 0; m[2] = -s;
	m[3] = 0; m[4] = 1.0f; m[5] = 0;
	m[6] = s; m[7] = 0; m[8] = c;
}

/* 0x800d0630: rotation about z by -angles[2] */
void rot_z(float *m, float *angles)
{
	float	c, s;

	c = fcos(-angles[2]);
	s = fsin(-angles[2]);
	m[0] = c; m[1] = s; m[2] = 0;
	m[3] = -s; m[4] = c; m[5] = 0;
	m[6] = 0; m[7] = 0; m[8] = 1.0f;
}

/* 0x800cfda8: out = Rz * Ry * Rx for the three angles */
void rot_matrix_zyx(float *out, float *angles)
{
	float	mx[9], my[9], mz[9], t[9];

	rot_x(mx, angles);
	rot_y(my, angles);
	rot_z(mz, angles);
	mat_mul(mz, my, t);
	mat_mul(t, mx, out);
}

/* 0x800cfe34: out = Rz * Rx * Ry for the three angles */
void rot_matrix_zxy(float *out, float *angles)
{
	float	mx[9], my[9], mz[9], t[9];

	rot_x(mx, angles);
	rot_y(my, angles);
	rot_z(mz, angles);
	mat_mul(mz, mx, t);
	mat_mul(t, my, out);
}

/* 0x800cfec0: out = Rx * Ry * Rz for the three angles */
void rot_matrix_xyz(float *out, float *angles)
{
	float	mx[9], my[9], mz[9], t[9];

	rot_x(mx, angles);
	rot_y(my, angles);
	rot_z(mz, angles);
	mat_mul(mx, my, t);
	mat_mul(t, mz, out);
}

/* 0x800cff4c: out = Rx * Rz * Ry for the three angles */
void rot_matrix_xzy(float *out, float *angles)
{
	float	mx[9], my[9], mz[9], t[9];

	rot_x(mx, angles);
	rot_y(my, angles);
	rot_z(mz, angles);
	mat_mul(mx, mz, t);
	mat_mul(t, my, out);
}

/* 0x800cffd8: out = Ry * Rz * Rx for the three angles */
void rot_matrix_yzx(float *out, float *angles)
{
	float	mx[9], my[9], mz[9], t[9];

	rot_x(mx, angles);
	rot_y(my, angles);
	rot_z(mz, angles);
	mat_mul(my, mz, t);
	mat_mul(t, mx, out);
}

/* 0x800d0064: out = Ry * Rx * Rz for the three angles */
void rot_matrix_yxz(float *out, float *angles)
{
	float	mx[9], my[9], mz[9], t[9];

	rot_x(mx, angles);
	rot_y(my, angles);
	rot_z(mz, angles);
	mat_mul(my, mx, t);
	mat_mul(t, mz, out);
}

/* 0x800cfd68: the identity, as the rotation by no angles */
void rot_matrix_none(float *out)
{
	float	zero[3] = { 0.0f, 0.0f, 0.0f };

	rot_matrix_zyx(out, zero);
}

/* 0x800d00f0-0x800d0474: the same six orders for the negated angles (each rotation negates
   again, so these turn the world the opposite way: the camera's view matrix is the zxy one) */
/* 0x800d00f0 */
void rot_matrix_neg_zyx(float *out, float *angles)
{
	float	mx[9], my[9], mz[9], t[9], neg[3];

	neg[0] = -angles[0];
	neg[1] = -angles[1];
	neg[2] = -angles[2];
	rot_x(mx, neg);
	rot_y(my, neg);
	rot_z(mz, neg);
	mat_mul(mz, my, t);
	mat_mul(t, mx, out);
}

/* 0x800d01a4 */
void rot_matrix_neg_zxy(float *out, float *angles)
{
	float	mx[9], my[9], mz[9], t[9], neg[3];

	neg[0] = -angles[0];
	neg[1] = -angles[1];
	neg[2] = -angles[2];
	rot_x(mx, neg);
	rot_y(my, neg);
	rot_z(mz, neg);
	mat_mul(mz, mx, t);
	mat_mul(t, my, out);
}

/* 0x800d0258 */
void rot_matrix_neg_yzx(float *out, float *angles)
{
	float	mx[9], my[9], mz[9], t[9], neg[3];

	neg[0] = -angles[0];
	neg[1] = -angles[1];
	neg[2] = -angles[2];
	rot_x(mx, neg);
	rot_y(my, neg);
	rot_z(mz, neg);
	mat_mul(my, mz, t);
	mat_mul(t, mx, out);
}

/* 0x800d030c */
void rot_matrix_neg_yxz(float *out, float *angles)
{
	float	mx[9], my[9], mz[9], t[9], neg[3];

	neg[0] = -angles[0];
	neg[1] = -angles[1];
	neg[2] = -angles[2];
	rot_x(mx, neg);
	rot_y(my, neg);
	rot_z(mz, neg);
	mat_mul(my, mx, t);
	mat_mul(t, mz, out);
}

/* 0x800d03c0 */
void rot_matrix_neg_xyz(float *out, float *angles)
{
	float	mx[9], my[9], mz[9], t[9], neg[3];

	neg[0] = -angles[0];
	neg[1] = -angles[1];
	neg[2] = -angles[2];
	rot_x(mx, neg);
	rot_y(my, neg);
	rot_z(mz, neg);
	mat_mul(mx, my, t);
	mat_mul(t, mz, out);
}

/* 0x800d0474 */
void rot_matrix_neg_xzy(float *out, float *angles)
{
	float	mx[9], my[9], mz[9], t[9], neg[3];

	neg[0] = -angles[0];
	neg[1] = -angles[1];
	neg[2] = -angles[2];
	rot_x(mx, neg);
	rot_y(my, neg);
	rot_z(mz, neg);
	mat_mul(mx, mz, t);
	mat_mul(t, my, out);
}

/* 0x800d06b4: m = diag(scale) * m */
void mat_scale(float *m, float *scale)
{
	float	s[9];

	s[0] = scale[0]; s[1] = 0; s[2] = 0;
	s[3] = 0; s[4] = scale[1]; s[5] = 0;
	s[6] = 0; s[7] = 0; s[8] = scale[2];
	mat_mul(s, m, m);
}

/* 0x800d0710: m = diag(1 / scale) * m */
void mat_unscale(float *m, float *scale)
{
	float	s[9];

	s[0] = 1.0f / scale[0]; s[1] = 0; s[2] = 0;
	s[3] = 0; s[4] = 1.0f / scale[1]; s[5] = 0;
	s[6] = 0; s[7] = 0; s[8] = 1.0f / scale[2];
	mat_mul(s, m, m);
}

/* 0x800cf6dc: out = m * v (rows of m dotted with v; out may not alias v) */
void mat_vec_mul(float *v, float *m, float *out)
{
	out[0] = v[0] * m[0] + v[1] * m[1] + v[2] * m[2];
	out[1] = v[0] * m[3] + v[1] * m[4] + v[2] * m[5];
	out[2] = v[0] * m[6] + v[1] * m[7] + v[2] * m[8];
}

/* 0x800cf7ec */
void vec_add(float *a, float *b, float *out)
{
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
}

/* 0x800cf98c: out[i] = m * v[i] for n vectors (the normals of a submesh) */
void transform_vectors(int n, float *out, float *v, float *m)
{
	int	i;

	for(i = 0; i < n; i++)
	{
		out[0] = v[0] * m[0] + v[1] * m[1] + v[2] * m[2];
		out[1] = v[0] * m[3] + v[1] * m[4] + v[2] * m[5];
		out[2] = v[0] * m[6] + v[1] * m[7] + v[2] * m[8];
		v += 3;
		out += 3;
	}
}

/* 0x800cf304: n model vertices through a placement (matrix m at position pos) and the
   camera to the screen: out[i] = (x, y, 1/depth), or q = -1 when the vertex is too near, too
   far or outside the (wide) view. Each takes three triangles' worth of the RTP count. The
   2^21 add-and-subtract rounds the coordinates to the Voodoo's 1/8 pixel. */
extern float	view_matrix[9], camera_neg_pos[3], cull_x, cull_y, proj_x, proj_y;
extern int	half_height, RTP_val;
void project_vertices(int n, float *v, float *out, float *m, float *pos)
{
	float	full[9];
	float	p[3];
	float	x, y, z, q;
	int	i;

	mat_mul(view_matrix, m, full);
	p[0] = pos[0] + camera_neg_pos[0];
	p[1] = pos[1] + camera_neg_pos[1];
	p[2] = pos[2] + camera_neg_pos[2];
	mat_vec_mul(p, view_matrix, p);
	RTP_val += n;
	for(i = 0; i < n; i++)
	{
		x = v[0] * full[0] + v[1] * full[1] + v[2] * full[2] + p[0];
		y = v[0] * full[3] + v[1] * full[4] + v[2] * full[5] + p[1];
		z = v[0] * full[6] + v[1] * full[7] + v[2] * full[8] + p[2];
		if(z < 1.1f || z > 400.0f)
		{
			out[2] = -1.0f;
		}
		else
		{
			q = 1.0f / z;
			x = x * q;
			if(x > cull_x || x < -cull_x)
			{
				out[2] = -1.0f;
			}
			else
			{
				y = y * q;
				if(y > cull_y || y < -cull_y)
				{
					out[2] = -1.0f;
				}
				else
				{
					x = x * proj_x + 200.0f;
					y = y * proj_y + (float)half_height;
					x = x + 2097152.0f;
					y = y + 2097152.0f;
					x = x - 2097152.0f;
					y = y - 2097152.0f;
					out[0] = x;
					out[2] = q;
					out[1] = y;
				}
			}
		}
		v += 3;
		out += 3;
	}
}

/* 0x800d0780 */
void vec_negate(float *v, float *out)
{
	out[0] = -v[0];
	out[1] = -v[1];
	out[2] = -v[2];
}

/* 0x800d1108: arc tangent by its series, in radians: four terms, reduced to |x| <= 1 by
   atan(x) = pi/2 - atan(1/x). The 1/7 coefficient is the original's typo (0.142867), and the
   fifth-power term of the main branch is a double constant, so that branch runs in double. */
float fatan(float x)
{
	if(x > 1.0f)
	{
		x = 1.0f / x;
		return(1.570796f - x + 0.333333f * x * x * x - 0.2f * x * x * x * x * x + 0.142867f * x * x * x * x * x * x * x);
	}
	if(x < -1.0f)
	{
		x = 1.0f / x;
		return(-1.570796f - x + 0.333333f * x * x * x - 0.2f * x * x * x * x * x + 0.142867f * x * x * x * x * x * x * x);
	}
	if(x == 1.0f)
	{
		return(0.785398f);
	}
	if(x == -1.0f)
	{
		return(-0.785398f);
	}
	if(x < 0.000005f && x > -0.000005f)
	{
		return(0.0f);
	}
	return(x - 0.333333f * x * x * x + 0.2 * x * x * x * x * x - 0.142867f * x * x * x * x * x * x * x);
}

/* 0x800d1300: tangent by its series, in radians (the seventh-power term is a double) */
float ftan(float x)
{
	return(x + 0.333333f * x * x * x + 0.1333333f * x * x * x * x * x + 0.053968 * x * x * x * x * x * x * x);
}
