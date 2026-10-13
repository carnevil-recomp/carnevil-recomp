/*
 * movie.h - CarnEvil's disk-streamed movie player (0x800e74f0-0x800e8f88 plus the script
 * interpreter at 0x80110270-0x80110ad8). "Movies" are the pre-rendered backgrounds of
 * every level and the attract/intro/ending sequences: raw partitions of fixed-size frames,
 * read one per tick by the "loadfram" process, expanded by a decoder into a 400x256
 * colour+depth buffer, and DMA'd into the Voodoo frame buffer before the 3D draws.
 */
#ifndef MOVIE_H
#define MOVIE_H

#define MOVIE_WIDTH		400
#define MOVIE_HEIGHT		256
#define MOVIE_END		0x2b67		/* do_script() result: the script finished */

/* flags in movie.flags */
#define MOVIE_PLAYING		1
#define MOVIE_READING		2
#define MOVIE_DONE		4

/* a frame buffer: three disk buffers (mdb1..3) and two screen buffers (msb1, msb2) */
typedef struct movie_buf
{
	int		state;			/* 0 free, 1 being read, 2 holds a frame */
	int		frame;
	unsigned char	*buffer;
	int		pad;
} movie_buf_t;

typedef struct movie
{
	int		flags;			/* 0x00 */
	movie_buf_t	*disp;			/* 0x04 screen buffer being shown */
	movie_buf_t	*disp_back;		/* 0x08 screen buffer being decoded into */
	movie_buf_t	*disk;			/* 0x0c disk buffer to decode next */
	movie_buf_t	*load;			/* 0x10 disk buffer being read */
	int		frame_number;		/* 0x14 next frame to read */
	int		unk18;
	int		unk1c;
	int		unk20;
	int		scroll;			/* 0x24 vertical offset (screen shake) */
	int		scan_plane;		/* 0x28 */
	int		partition;		/* 0x2c TRAP partition index of the frames */
	int		enable_flag;		/* 0x30 1 = show the previous frame (double buffer) */
	int		unk34;			/* 0x34 per-movie constant (100/700/1800/62) */
	int		sectors_per_frame;	/* 0x38 */
	int		disk_error;		/* 0x3c last status from the disk callback */
	int		width;			/* 0x40 */
	int		height;			/* 0x44 */
	void		(*decode)(unsigned char *, unsigned int *);	/* 0x48 */
	int		unk4c;
	int		unk50;
	int		unk54;
} movie_t;

/* script words hold ints, function pointers and data pointers; the host build needs them
   pointer-sized */
#ifdef PORT_HOST
typedef long	script_word_t;
#else
typedef int	script_word_t;
#endif
#define SCRIPT_FN(f)	((script_word_t)(f))
#define SCRIPT_VAR(v)	((script_word_t)(v))
#define SCRIPT_LABEL(i)	((script_word_t)&movie_scripts[i])

/* entry points into movie_scripts[] (movie_scripts.c) */
#define INTRO_SCRIPT_OFFSET	0
#define ATTRACT2_SCRIPT_OFFSET	29
#define END_SCRIPT_OFFSET	50
#define MIDWAY_SCRIPT_OFFSET	74
#define ATTRACT_SCRIPT_OFFSET	77
#define ATTRACT3_SCRIPT_OFFSET	80
#define GALLERY_SCRIPT_OFFSET	83

extern movie_t		movie;
extern script_word_t	movie_scripts[];
extern int		movie_paused;

int	movie_load(int id);
int	movie_start(void);
int	movie_pause(void);
int	movie_resume(void);
int	movie_stop(void);
void	movie_init(void);
void	loadfram_proc(void);
int	movie_draw_frame(void);
float	movie_depth_at(float x, float y);
void	movie_script_init(script_word_t *script);
int	do_script(int frame);
void	decode_movie_8pal(unsigned char *src, unsigned int *dst);
void	decode_movie_8pal_depth(unsigned char *src, unsigned int *dst);
void	decode_movie_1pal(unsigned char *src, unsigned int *dst);
void	decode_movie_1pal_depth(unsigned char *src, unsigned int *dst);

#endif
