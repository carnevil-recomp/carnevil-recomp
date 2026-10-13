/*
 * movie.c - CarnEvil's disk-streamed movie player (0x800e74f0-0x800e8f88, scroll update at
 * 0x800f3580, script interpreter at 0x80110270-0x80110ad8). See movie.h for the design.
 *
 * Frame formats (one frame = sectors_per_frame x 512 bytes in a raw TRAP partition):
 *   decode_movie_8pal     8 palettes of 256 RGB565 (0x1000 bytes) then 400x256 indices in
 *                        32 strips of 8 rows; row r of a strip uses palette 7-r. (attract,
 *                        intro, ending: 0xd0 sectors)
 *   decode_movie_8pal_depth       as above plus a 256-entry depth table and a 400x128 depth index
 *                        plane, each depth row serving two image rows (levels: 0x135 sectors)
 *   decode_movie_1pal    one palette (0x200 bytes) then 400x256 indices (midway: 0xc9)
 *   decode_movie_1pal_depth  one palette, indices, depth table, depth plane (0x12e)
 * Every output pixel is a 32-bit LFB word: RGB565 in the low half, depth in the high half
 * (0xffff = farthest for the flat formats), written with grLfbWriteMode(13).
 */
#include <stdio.h>
#include <stdlib.h>
#include <glide/glide.h>
#include <goose/process.h>
#include "movie.h"

extern void	sleep(int);
extern void	fade(float, int, void (*)(void));
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string(int, char *, float, float);
extern void	set_frame_factor(int);
extern void	remove_all_objects(void);
extern void	install_disk_callback(void (*)(int));
extern void	install_disk_info(void *, void *, void *);
extern void	*ide_get_partition_table(void);
extern short	_SecReads(unsigned int sector, void *buf, unsigned int num);
extern int	stream_kick(void);		/* 0x8013ff24: queue the next music stream read, returns reads issued */
extern void	stream_fill_done(void);		/* 0x8013fea4 */
extern int	end_of_wave(void);
extern void	increment_audit(int);
extern float	func_800c9158(unsigned int *);	/* depth word -> distance */
extern void	specific_gamma_load(float, float);
extern void	stop_level_cue_and_stream(void), play_level_cue(int, char *);
extern void	play_level_music(int, char *), play_level_speech(int, char *), start_level_music(int, char *);
extern void	stop_level_music(void), fade_out_level_music(void), fade_out_level_stream(void), func_800d4968(void);
extern void	play_bigtop_cue(int, int), play_bigtop2_cue(int);
extern void	stream_set_loop(int, int), stream_jump(int, int, int);	/* 0x801400a4, 0x801400dc */
extern void	*func_800d6758(int);
extern short	add_object1_wrapper(int, void (*)(), int, int);
extern void	level_end_stats_wrapper();
extern unsigned int	background_color;
extern int	do_draw_background;
extern int	debug_mode;			/* 0x80212e20 */
#include "credits.h"			/* game_mode (0x80212e14), player_stats (0x801e0f64) */
extern int	D_801e6e70, D_8018db90;
extern char	text_buffer[];

extern script_word_t	hhouse_script[], freakshow_script[], rickity_town_script[], bigtop_script[];
extern int	script_stop_frame;		/* script.c */

/* ---- state ---- */
movie_t		movie;				/* 0x801de900 */
unsigned int	movie_palette[8][256];		/* 0x801de958 */
unsigned int	movie_depth_table[256];		/* 0x801e0a08 */
unsigned int	movie_flat_palette[256];	/* 0x801e11a0 */
movie_buf_t	mdb1, mdb2, mdb3;		/* 0x801fac88, 0x801fac98, 0x801faca8 */
movie_buf_t	msb1, msb2;			/* 0x801fad28, 0x801fad38 */
int		movie_paused;			/* 0x801e0ef4 */
unsigned int	*blank_row;			/* 0x801e09e4: a zeroed scan line for the scroll gaps */
int		disk_pending;			/* 0x8023741c: outstanding sector reads */
int		grab_request;			/* 0x8017dfdc */
int		grab_count;			/* 0x8017dfe0 */
char		grab_name[256];			/* 0x801e7058 */

/* 0x800e7534, 0x800e7548, 0x800e7558, 0x800e7560 */
int movie_pause(void) { movie_paused = 1; return(0); }
int movie_resume(void) { movie_paused = 0; return(0); }
int movie_nop1(void) { return(0); }
int movie_nop2(void) { return(0); }

/* 0x800e74f0: start streaming the movie selected by movie_load() */
int movie_start(void)
{
	install_disk_info(0, 0, 0);
	movie_resume();
	movie.flags |= MOVIE_PLAYING;
	return(0);
}

/* 0x800e7568 */
int movie_stop(void)
{
	movie_paused = 0;
	movie.flags = (movie.flags & ~MOVIE_PLAYING) | MOVIE_DONE;
	install_disk_callback(0);
	return(0);
}

/* 0x800e7188: select a movie: its script, partition, frame size and decoder */
int movie_load(int id)
{
	switch(id)
	{
	case 1:
		movie_script_init(hhouse_script);
		movie.partition = 4;
		movie.enable_flag = 1;
		movie.unk34 = 100;
		movie.sectors_per_frame = 0x135;
		movie.decode = decode_movie_8pal_depth;
		break;
	case 2:
		movie_script_init(freakshow_script);
		movie.partition = 6;
		movie.enable_flag = 1;
		movie.unk34 = 100;
		movie.sectors_per_frame = 0x135;
		movie.decode = decode_movie_8pal_depth;
		break;
	case 5:
		movie_script_init(rickity_town_script);
		movie.partition = 8;
		movie.enable_flag = 1;
		movie.unk34 = 1800;
		movie.sectors_per_frame = 0x135;
		movie.decode = decode_movie_8pal_depth;
		break;
	case 7:
		movie_script_init(bigtop_script);
		movie.partition = 10;
		movie.enable_flag = 1;
		movie.unk34 = 1800;
		movie.sectors_per_frame = 0x135;
		movie.decode = decode_movie_8pal_depth;
		break;
	case 3:
		movie_script_init(&movie_scripts[INTRO_SCRIPT_OFFSET]);
		movie.partition = 13;
		movie.unk34 = 100;
		movie.sectors_per_frame = 0xd0;
		movie.enable_flag = 0;
		movie.decode = decode_movie_8pal;
		break;
	case 9:
		movie_script_init(&movie_scripts[ATTRACT2_SCRIPT_OFFSET]);
		movie.partition = 12;
		movie.unk34 = 700;
		movie.sectors_per_frame = 0xd0;
		movie.enable_flag = 0;
		movie.decode = decode_movie_8pal;
		break;
	case 4:
		movie_script_init(&movie_scripts[MIDWAY_SCRIPT_OFFSET]);
		movie.partition = 2;
		movie.unk34 = 100;
		movie.sectors_per_frame = 0xc9;
		movie.enable_flag = 0;
		movie.decode = decode_movie_1pal;
		break;
	case 8:
		movie_script_init(&movie_scripts[GALLERY_SCRIPT_OFFSET]);
		movie.partition = 11;
		movie.enable_flag = 1;
		movie.unk34 = 62;
		movie.sectors_per_frame = 0x135;
		movie.decode = decode_movie_8pal_depth;
		break;
	case 10:
		movie_script_init(&movie_scripts[END_SCRIPT_OFFSET]);
		movie.partition = 14;
		movie.unk34 = 700;
		movie.sectors_per_frame = 0xd0;
		movie.enable_flag = 0;
		movie.decode = decode_movie_8pal;
		break;
	case 6:
		movie_script_init(&movie_scripts[ATTRACT_SCRIPT_OFFSET]);
		movie.partition = 15;
		movie.enable_flag = 1;
		movie.unk34 = 100;
		movie.sectors_per_frame = 0xd0;
		movie.decode = decode_movie_8pal;
		break;
	case 11:
		movie_script_init(&movie_scripts[ATTRACT3_SCRIPT_OFFSET]);
		movie.partition = 16;
		movie.enable_flag = 1;
		movie.unk34 = 100;
		movie.sectors_per_frame = 0xd0;
		movie.decode = decode_movie_8pal;
		break;
	default:
		printf("Movie_load: don't know movie %d\n", id);
		break;
	}
	msb1.state = 0;
	msb2.state = 0;
	msb1.frame = 0;
	msb2.frame = 0;
	mdb1.state = 0;
	movie.disk = &mdb1;
	movie.load = &mdb1;
	movie.width = MOVIE_WIDTH;
	mdb2.state = 0;
	mdb3.state = 0;
	mdb1.frame = 0;
	mdb2.frame = 0;
	mdb3.frame = 0;
	movie.frame_number = 0;
	movie.flags = 0;
	movie.disp = &msb2;
	movie.disp_back = &msb1;
	movie_paused = 0;
	movie.scan_plane = 0;
	movie.disk_error = 0;
	movie.height = MOVIE_HEIGHT;
	return(0);
}

/* 0x800e8168: the ending movie, started from the game rather than the attract loop */
void movie_load_ending(void)
{
	stop_level_cue_and_stream();
	play_level_cue(1, "ending");
	remove_all_objects();
	set_frame_factor(2);
	movie.partition = 14;
	movie.unk34 = 700;
	movie.sectors_per_frame = 0xd0;
	movie.enable_flag = 0;
	movie.decode = decode_movie_8pal;
}

/* 0x800e81e4-0x800e82a0: gamma presets used by the scripts */
void movie_gamma_0(void) { specific_gamma_load(0.0f, 1.0f); }
void movie_gamma_25(void) { specific_gamma_load(0.25f, 1.0f); }
void movie_gamma_50(void) { specific_gamma_load(0.5f, 1.0f); }
void movie_gamma_75(void) { specific_gamma_load(0.75f, 1.0f); }
void movie_gamma_100(void) { specific_gamma_load(1.0f, 1.0f); }

/* 0x800e75ac: allocate the buffers and start the loader process (called once at game init) */
void movie_init(void)
{
	unsigned int	*p;
	int		i;

	movie_paused = 0;
	do_draw_background = 0;
	blank_row = p = malloc(MOVIE_WIDTH * 4);
	for(i = MOVIE_WIDTH - 1; i >= 0; i--)
	{
		*p++ = 0;
	}
	mdb1.buffer = malloc(0x26a00);
	if(!mdb1.buffer)
	{
		printf("Error, could not malloc memory for mdb1.buffer\n");
	}
	mdb2.buffer = malloc(0x26a00);
	if(!mdb2.buffer)
	{
		printf("Error, could not malloc memory for mdb2.buffer\n");
	}
	mdb3.buffer = malloc(0x26a00);
	if(!mdb3.buffer)
	{
		printf("Error, could not malloc memory for mdb3.buffer\n");
	}
	msb1.buffer = malloc(0x64040);
	if(!msb1.buffer)
	{
		printf("Error, could not malloc memory for msb1.buffer\n");
	}
	msb2.buffer = malloc(0x64040);
	if(!msb2.buffer)
	{
		printf("Error, could not malloc memory for msb2.buffer\n");
	}
	msb1.state = 0;
	msb2.state = 0;
	msb1.frame = 0;
	msb2.frame = 0;
	mdb1.state = 0;
	movie.disp = &msb2;
	movie.disp_back = &msb1;
	movie.disk = &mdb1;
	movie.load = &mdb1;
	movie.partition = 6;
	movie.enable_flag = 1;
	movie.unk34 = 100;
	movie.sectors_per_frame = 0x12e;
	movie.width = MOVIE_WIDTH;
	mdb2.state = 0;
	mdb3.state = 0;
	mdb1.frame = 0;
	mdb2.frame = 0;
	mdb3.frame = 0;
	movie.frame_number = 0;
	movie.flags = 0;
	movie.scan_plane = 0;
	movie.disk_error = 0;
	movie.height = MOVIE_HEIGHT;
	qcreate("loadfram", 0x400, loadfram_proc, 0, 0, 0, 0);
}

/* 0x800e7cf8: the BIOS calls this when a sector read completes */
void movie_disk_callback(int err)
{
	movie.disk_error = err;
	if(err)
	{
		printf("Movie: Disk error %d reading frame %d\n", err, movie.frame_number);
	}
	disk_pending--;
}

/* 0x800e7d50: the "loadfram" process: read one frame per tick into the next free disk
   buffer, then let the script decide which frame comes next */
void loadfram_proc(void)
{
	int	*table;
	int	n;			/* the frame's first sector, then the music stream reads issued */
	int	i;

	disk_pending = 0;
	for(;;)
	{
		if(!movie_paused && !(movie.flags & MOVIE_READING) && movie.load->state == 0
			&& (movie.flags & MOVIE_PLAYING))
		{
			movie.flags |= MOVIE_READING;
			movie.load->state = 1;
			install_disk_callback(movie_disk_callback);
			n = movie.frame_number * movie.sectors_per_frame;
			i = 0;
			table = ide_get_partition_table();
			disk_pending = 1;
			table += movie.partition * 3;
			_SecReads(n + table[2], movie.load->buffer, movie.sectors_per_frame);
			n = stream_kick();
			disk_pending += n;
			do
			{
				sleep(1);
				i++;
				if(i >= 201)
				{
					disk_pending = 0;
				}
			} while(disk_pending > 0 && (movie.flags & MOVIE_PLAYING));
			movie.flags &= ~MOVIE_READING;
			if(n)
			{
				stream_fill_done();
			}
			movie.load->state = 2;
			movie.load->frame = movie.frame_number;
#ifdef PORT_HOST
			{ extern int port_trace; if(port_trace && (movie.frame_number < 12 || !(movie.frame_number % 300))) { extern unsigned long hal_frame_count; fprintf(stderr, "[movie] vblank %lu partition %d frame %d stop %d\n", hal_frame_count, movie.partition, movie.frame_number, script_stop_frame); } }	/* PORT_TRACE_FRAME */
#endif
			movie.frame_number = do_script(movie.frame_number);
			if(movie.frame_number == MOVIE_END)
			{
				movie.flags = (movie.flags & ~MOVIE_PLAYING) | MOVIE_DONE;
			}
			if(movie.load == &mdb1)
			{
				movie.load = &mdb2;
			}
			else if(movie.load == &mdb2)
			{
				movie.load = &mdb3;
			}
			else
			{
				movie.load = &mdb1;
			}
		}
		else
		{
			sleep(1);
		}
	}
}

/* 0x800e7f64: ask the draw pass to save the next frame as GRABnnnn.RAW */
void grab_request_screen(void)
{
	graphical_printf(grab_name, "GRAB%04d.RAW", grab_count);
	grab_request = 1;
	grab_count++;
}

/* 0x800e7fbc: read the front buffer back through the LFB and write it as raw RGB */
void grab_save_screen(char *name)
{
	FILE		*f;
	unsigned char	b, g, r;
	unsigned short	*fb;
	unsigned int	v;
	int		x, y, row;

	printf("grab_save_screen(): begin\n");
	printf("grab_save_screen(): saving %s...\n", name);
	f = fopen(name, "wb");
	if(!f)
	{
		printf("grab_write_file(): ERROR cannot open file %s for writing.\n", name);
		fclose(0);
		return;
	}
#ifndef PORT_HOST
	fb = (unsigned short *)movie.disp_back->buffer;
	grLfbBegin();
	grLfbWriteMode(0);
	grLfbOrigin(1);
	grLfbBypassMode(0);
	grLfbReadRegion(0, 0, MOVIE_WIDTH, MOVIE_HEIGHT, fb, 800);
	grLfbEnd();
	printf("read region\n");
	for(y = 255, row = 0x18e70; y >= 0; y--)
	{
		for(x = 0; x < MOVIE_WIDTH; x++)
		{
			v = fb[row + x];
			r = (v >> 11) << 3;
			g = (v >> 3) & 0xfc;
			b = (v & 0x1f) << 3;
			fwrite(&r, 1, 1, f);
			fwrite(&g, 1, 1, f);
			fwrite(&b, 1, 1, f);
		}
		row -= MOVIE_WIDTH;
	}
#else
	(void)fb; (void)v; (void)r; (void)g; (void)b; (void)x; (void)y; (void)row;
#endif
	fclose(f);
	printf("grab_save_screen(): end\n");
}

/* 0x800e7c6c: distance of the movie pixel under (x, y), 300 when no movie is up */
float movie_depth_at(float x, float y)
{
	int	ix, iy;

	if(movie.disp->state != 2 || !(movie.flags & MOVIE_PLAYING))
	{
		return(300.0f);
	}
	ix = (int)x;
	iy = (int)y;
	return(func_800c9158(&((unsigned int *)movie.disp->buffer)[iy * MOVIE_WIDTH + ix]));
}

#ifdef PORT_HOST
extern void	port_movie_blit(const unsigned int *buf, int scroll);
#else
/* the DMA descriptor list is built through the global pointer register (the game is
   compiled with -G 0, so $gp is otherwise unused) */
register unsigned int	*dma asm("gp");
#endif

/* 0x800e77ec: the draw pass' first job: put the current frame on screen (or clear), then
   decode the next frame and rotate the buffers. Returns the frame number shown. */
int movie_draw_frame(void)
{
	int		frame = 0;
	int		state;

	if(grab_request)
	{
		grab_save_screen(grab_name);
		grab_request = 0;
	}
	if(movie.disp->state == 2 && (movie.flags & MOVIE_PLAYING))
	{
		frame = movie.disp->frame;
		update_movie_scroll();
#ifdef PORT_HOST
		port_movie_blit((unsigned int *)movie.disp->buffer, movie.scroll);
#else
		{
			unsigned int	*src;
			unsigned int	lfb;
			int		rows, top, bottom, i;

			grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);
			grDisableAllEffects();
			grDitherMode(GR_DITHER_DISABLE);
			grLfbWriteMode(13);
			grLfbConstantAlpha(0xff);
			grConstantColorValue(0xff000000);
			guColorCombineFunction(GR_COLORCOMBINE_DECAL_TEXTURE);
			grLfbBypassMode(1);
			grLfbOrigin(1);
			grChromakeyMode(GR_CHROMAKEY_DISABLE);
			grLfbBegin();
			lfb = (unsigned int)grLfbGetWritePtr(1);
			src = (unsigned int *)movie.disp->buffer;
			if(movie.scroll >= 0)
			{
				rows = MOVIE_HEIGHT - movie.scroll;
				top = movie.scroll;
				bottom = 0;
			}
			else
			{
				bottom = -movie.scroll;
				src += movie.width * bottom;
				rows = movie.scroll + MOVIE_HEIGHT;
				top = 0;
			}
			/* one descriptor per scan line: byte count, source, LFB destination, next */
			for(i = 0; i < top; i++)
			{
				dma[0] = movie.width << 2;
				dma[1] = (unsigned int)blank_row & 0x1fffffff;
				dma[2] = lfb & 0x1fffffff;
				dma[3] = (unsigned int)(dma + 4);
				lfb += 0x1000;
				dma += 4;
			}
			for(i = 0; i < rows; i++)
			{
				dma[0] = movie.width << 2;
				dma[1] = (unsigned int)src & 0x1fffffff;
				dma[2] = lfb & 0x1fffffff;
				dma[3] = (unsigned int)(dma + 4);
				lfb += 0x1000;
				src += movie.width;
				dma += 4;
			}
			for(i = 0; i < bottom; i++)
			{
				dma[0] = movie.width << 2;
				dma[1] = (unsigned int)blank_row & 0x1fffffff;
				dma[2] = lfb & 0x1fffffff;
				dma[3] = (unsigned int)(dma + 4);
				lfb += 0x1000;
				dma += 4;
			}
			grLfbEnd();
			grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
		}
#endif
	}
	else
	{
		grBufferClear(background_color, 0, 0xffff);
	}
	state = movie.disk->state;
	if(state == 2)
	{
		movie.decode(movie.disk->buffer, (unsigned int *)movie.disp_back->buffer);
		movie.disp_back->state = state;
		movie.disp_back->frame = movie.disk->frame;
		movie.disk->state = 0;
		if(movie.enable_flag == 1)
		{
			if(movie.disp_back == &msb1)
			{
				movie.disp_back = &msb2;
				movie.disp = &msb1;
			}
			else
			{
				movie.disp_back = &msb1;
				movie.disp = &msb2;
			}
		}
		else
		{
			if(movie.disp_back == &msb1)
			{
				movie.disp_back = &msb2;
				movie.disp = &msb2;
			}
			else
			{
				movie.disp_back = &msb1;
				movie.disp = &msb1;
			}
		}
		if(movie.disk == &mdb1)
		{
			movie.disk = &mdb2;
		}
		else if(movie.disk == &mdb2)
		{
			movie.disk = &mdb3;
		}
		else
		{
			movie.disk = &mdb1;
		}
	}
	if(debug_mode & 0x2000)
	{
		grDepthBufferFunction(GR_CMP_ALWAYS);
		grDepthMask(FXFALSE);
		if(!D_801e6e70)
		{
			graphical_printf(text_buffer, "FRAME %d   %d %d", frame, movie.frame_number, 0);
		}
		else
		{
			graphical_printf(text_buffer, "FRAME %d   %d LOCK %d %d", frame, movie.frame_number, D_8018db90, D_801e6e70);
		}
		draw_string(0, text_buffer, 0.0f, 220.0f);
		grDepthMask(FXTRUE);
		grDepthBufferFunction(GR_CMP_LEQUAL);
	}
	return(frame);
}

/* ---- the decoders: src is the raw frame, dst the 400x256 buffer of LFB words ----
   (one index variable and one table pointer serve every loop of a decoder, and the depth
   plane base is reused as the first row's pointer: that is how the original was written) */

#define PIX(tab, p)	*dst++ = tab[*p++]
#define PIX16(tab, p)	PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); \
			PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p)
#define DPIX(pal, cp, dp)	*dst++ = pal[*cp++] | tab[*dp++]
#define DPIX8(pal, cp, dp)	DPIX(pal, cp, dp); DPIX(pal, cp, dp); DPIX(pal, cp, dp); DPIX(pal, cp, dp); \
				DPIX(pal, cp, dp); DPIX(pal, cp, dp); DPIX(pal, cp, dp); DPIX(pal, cp, dp)
#define RP(p)		*dma++ = *p++ | 0xffff0000
#define RP8(p)		RP(p); RP(p); RP(p); RP(p); RP(p); RP(p); RP(p); RP(p)

/* 0x800e8510 */
void decode_movie_8pal(unsigned char *src, unsigned int *dst)
{
	unsigned short	*pal = (unsigned short *)src;
	unsigned char	*p = src + 0x1000;
	unsigned int	*tab;
	int		cols = movie.width / 16;
	int		strip, t, i;

	for(t = 0; t < 8; t++)
	{
		tab = movie_palette[t];
		for(i = 0; i < 256; i++)
		{
			*tab++ = *pal++ | 0xffff0000;
		}
	}
	for(strip = 0; strip < 32; strip++)
	{
		for(t = 7; t >= 0; t--)
		{
			tab = movie_palette[t];
			for(i = 0; i < cols; i++)
			{
				PIX16(tab, p);
			}
		}
	}
}

/* 0x800e8b78: eight palettes plus a depth table and a half-height depth plane */
void decode_movie_8pal_depth(unsigned char *src, unsigned int *dst)
{
	unsigned char	*cp = src + 0x1000;
	unsigned char	*dp = cp + movie.width * 256;
	unsigned short	*dtab = (unsigned short *)dp;
	unsigned char	*dpb = dp + 0x200;
	unsigned short	*pal;
	unsigned int	*tab;
	int		cols;
	int		strip, t, i;

	dp = dpb;
	cols = movie.width / 8;
	tab = movie_depth_table;
	for(i = 0; i < 256; i++)
	{
		*tab++ = (unsigned int)*dtab++ << 16;
	}
	tab = movie_depth_table;
	for(strip = 0; strip < 32; strip++)
	{
		for(t = 7; t >= 0; t--)
		{
			pal = (unsigned short *)(src + t * 0x200);
			for(i = 0; i < cols; i++)
			{
				DPIX8(pal, cp, dp);
			}
			t--;
			pal = (unsigned short *)(src + t * 0x200);
			for(i = 0; i < cols; i++)
			{
				DPIX8(pal, cp, dpb);
			}
		}
	}
}

/* 0x800e82c8: one palette, no depth */
void decode_movie_1pal(unsigned char *src, unsigned int *dst)
{
	unsigned short	*pal = (unsigned short *)src;
	unsigned char	*p = src + 0x200;
	unsigned int	*tab;
	int		cols = movie.width / 8;
	int		r, i;

	tab = movie_flat_palette;
	for(i = 0; i < 256; i++)
	{
		*tab++ = *pal++ | 0xffff0000;
	}
	tab = movie_flat_palette;
	for(r = 0; r < 128; r++)
	{
		for(i = 0; i < cols; i++)
		{
			PIX16(tab, p);
		}
	}
}

/* 0x800e878c: one palette plus the depth table and plane */
void decode_movie_1pal_depth(unsigned char *src, unsigned int *dst)
{
	unsigned short	*pal = (unsigned short *)src;
	unsigned char	*cp = src + 0x200;
	unsigned char	*dp = cp + movie.width * 256;
	unsigned short	*dtab = (unsigned short *)dp;
	unsigned char	*dpb = dp + 0x200;
	unsigned int	*tab;
	int		cols;
	int		r, i;

	dp = dpb;
	cols = movie.width / 8;
	tab = movie_depth_table;
	for(i = 0; i < 256; i++)
	{
		*tab++ = (unsigned int)*dtab++ << 16;
	}
	tab = movie_depth_table;
	for(r = 0; r < 128; r++)
	{
		for(i = 0; i < cols; i++)
		{
			DPIX8(pal, cp, dp);
		}
		for(i = 0; i < cols; i++)
		{
			DPIX8(pal, cp, dpb);
		}
	}
}

/* 0x800e8f88: raw RGB565 frames (no palette), each row shown twice; not selected by
   movie_load(), kept because it is part of the file (and not yet exact: 56%) */
void decode_movie_rgb16(unsigned char *src, unsigned int *dst)
{
	unsigned short	*p, *q;
	int		cols;
	int		r, i;

#ifndef PORT_HOST
	dma = dst;
#else
	unsigned int	*dma = dst;
#endif
	src += 0x200;
	p = (unsigned short *)(src + movie.width * 256);
	q = p;
	cols = movie.width / 8;
	for(r = 0; r < 128; r++)
	{
		for(i = 0; i < cols; i++)
		{
			RP8(q);
		}
		for(i = 0; i < cols; i++)
		{
			RP8(p);
		}
	}
}
