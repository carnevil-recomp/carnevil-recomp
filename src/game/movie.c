/*
 * movie.c - CarnEvil's disk-streamed movie player (0x800e74f0-0x800e8f88, scroll update at
 * 0x800f3580, script interpreter at 0x80110270-0x80110ad8). See movie.h for the design.
 *
 * Frame formats (one frame = sectors_per_frame x 512 bytes in a raw TRAP partition):
 *   decompress_movie     8 palettes of 256 RGB565 (0x1000 bytes) then 400x256 indices in
 *                        32 strips of 8 rows; row r of a strip uses palette 7-r. (attract,
 *                        intro, ending: 0xd0 sectors)
 *   decoode_movie2       as above plus a 256-entry depth table and a 400x128 depth index
 *                        plane, each depth row serving two image rows (levels: 0x135 sectors)
 *   decode_movie_flat    one palette (0x200 bytes) then 400x256 indices (midway: 0xc9)
 *   decode_movie_flat_depth  one palette, indices, depth table, depth plane (0x12e)
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
extern int	func_8013ff24(void);		/* disk queue kick: returns extra requests issued */
extern void	func_8013fea4(void);		/* disk queue cleanup */
extern int	end_of_wave(void);
extern void	increment_audit(int);
extern float	func_800c9158(unsigned int *);	/* depth word -> distance */
extern void	specific_gamma_load(float, float);
extern void	stop_level_cue_and_stream(void), play_level_cue(int, char *);
extern void	func_800ebdbc(int, char *), func_800ebe34(int, char *), func_800ebd70(int, char *);
extern void	func_800ebe6c(void), func_800ebe8c(void), func_800ebb44(void), func_800d4968(void);
extern void	func_800ec370(int, int), func_800ec504(int);
extern void	func_801400a4(int, int), func_801400dc(int, int, int);
extern void	*func_800d6758(int);
extern short	add_object1_wrapper(int, void (*)(), int, int);
extern void	level_end_stats_wrapper();
extern unsigned int	background_color;
extern int	do_draw_background;
extern int	debug_mode;			/* 0x80212e20 */
extern int	D_80212e14;			/* attract mode flag */
extern int	player_stats[], D_801e1084;	/* 0x801e0f64, 0x801e1084: player state words */
extern int	D_801e6e70, D_8018db90;
extern char	text_buffer[];

extern script_word_t	hhouse_script[], freakshow_script[], rickity_town_script[], bigtop_script[];

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

/* screen shake / scroll (0x800f3580) */
int		shake_count;			/* 0x801e0e6c */
float		shake_amp;			/* 0x801e09dc */
float		shake_decay;			/* 0x80212dc4 */
int		scroll_request;			/* 0x8018f130 */
int		scroll_value = 4;		/* 0x8018f134 */
int		movie_scroll_center = 128;	/* 0x8017dfd8 */

/* script interpreter */
script_word_t	*movie_script_g1;		/* 0x801e09d8: start of the current script */
script_word_t	*movie_script_g2;		/* 0x801e177c: next word to execute */
int		script_stop_frame;		/* 0x8023740c: play until this frame */
int		script_delay;			/* 0x80212e94: ticks to hold the current frame */
int		script_abort;			/* 0x801e5bdc: end the movie (end_of_wave) */
int		script_skip;			/* 0x801e5b24: jump to the stop frame */

/* 0x80110270 */
void movie_script_init(script_word_t *script)
{
	movie_script_g1 = script;
	movie_script_g2 = script;
	script_stop_frame = 0;
	script_delay = 0;
	script_abort = 0;
	script_skip = 0;
}

/* 0x801102a8, 0x801102bc */
void movie_script_abort(void) { script_abort = 1; }
void movie_script_skip(void) { script_skip = 1; }

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
		movie.decode = decoode_movie2;
		break;
	case 2:
		movie_script_init(freakshow_script);
		movie.partition = 6;
		movie.enable_flag = 1;
		movie.unk34 = 100;
		movie.sectors_per_frame = 0x135;
		movie.decode = decoode_movie2;
		break;
	case 5:
		movie_script_init(rickity_town_script);
		movie.partition = 8;
		movie.enable_flag = 1;
		movie.unk34 = 1800;
		movie.sectors_per_frame = 0x135;
		movie.decode = decoode_movie2;
		break;
	case 7:
		movie_script_init(bigtop_script);
		movie.partition = 10;
		movie.enable_flag = 1;
		movie.unk34 = 1800;
		movie.sectors_per_frame = 0x135;
		movie.decode = decoode_movie2;
		break;
	case 3:
		movie_script_init(&movie_scripts[INTRO_SCRIPT_OFFSET]);
		movie.partition = 13;
		movie.unk34 = 100;
		movie.sectors_per_frame = 0xd0;
		movie.decode = decompress_movie;
		movie.enable_flag = 0;
		break;
	case 9:
		movie_script_init(&movie_scripts[ATTRACT2_SCRIPT_OFFSET]);
		movie.partition = 12;
		movie.unk34 = 700;
		movie.sectors_per_frame = 0xd0;
		movie.decode = decompress_movie;
		movie.enable_flag = 0;
		break;
	case 4:
		movie_script_init(&movie_scripts[MIDWAY_SCRIPT_OFFSET]);
		movie.partition = 2;
		movie.unk34 = 100;
		movie.sectors_per_frame = 0xc9;
		movie.decode = decode_movie_flat;
		movie.enable_flag = 0;
		break;
	case 8:
		movie_script_init(&movie_scripts[GALLERY_SCRIPT_OFFSET]);
		movie.partition = 11;
		movie.enable_flag = 1;
		movie.unk34 = 62;
		movie.sectors_per_frame = 0x135;
		movie.decode = decoode_movie2;
		break;
	case 10:
		movie_script_init(&movie_scripts[END_SCRIPT_OFFSET]);
		movie.partition = 14;
		movie.unk34 = 700;
		movie.sectors_per_frame = 0xd0;
		movie.decode = decompress_movie;
		movie.enable_flag = 0;
		break;
	case 6:
		movie_script_init(&movie_scripts[ATTRACT_SCRIPT_OFFSET]);
		movie.partition = 15;
		movie.enable_flag = 1;
		movie.unk34 = 100;
		movie.sectors_per_frame = 0xd0;
		movie.decode = decompress_movie;
		break;
	case 11:
		movie_script_init(&movie_scripts[ATTRACT3_SCRIPT_OFFSET]);
		movie.partition = 16;
		movie.enable_flag = 1;
		movie.unk34 = 100;
		movie.sectors_per_frame = 0xd0;
		movie.decode = decompress_movie;
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
	movie.decode = decompress_movie;
}

/* 0x800e81e4-0x800e82a0: gamma presets used by the scripts */
void movie_gamma_0(void) { specific_gamma_load(0.0f, 1.0f); }
void movie_gamma_25(void) { specific_gamma_load(0.25f, 1.0f); }
void movie_gamma_50(void) { specific_gamma_load(0.5f, 1.0f); }
void movie_gamma_75(void) { specific_gamma_load(0.75f, 1.0f); }
void movie_gamma_100(void) { specific_gamma_load(1.0f, 1.0f); }

/* 0x800e75ac: allocate the buffers and start the loader process (called once at game init) */
void load_fram_wrapper(void)
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
	movie.height = MOVIE_HEIGHT;
	mdb2.state = 0;
	mdb3.state = 0;
	mdb1.frame = 0;
	mdb2.frame = 0;
	mdb3.frame = 0;
	movie.frame_number = 0;
	movie.flags = 0;
	movie.scan_plane = 0;
	movie.disk_error = 0;
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
	movie_buf_t	*buf;
	int		*table;
	int		sector;
	int		extra;
	int		i;

	disk_pending = 0;
	for(;;)
	{
		if(movie_paused || (movie.flags & MOVIE_READING) || movie.load->state != 0
			|| !(movie.flags & MOVIE_PLAYING))
		{
			sleep(1);
			continue;
		}
		buf = movie.load;
		movie.flags |= MOVIE_READING;
		buf->state = 1;
		install_disk_callback(movie_disk_callback);
		sector = movie.frame_number * movie.sectors_per_frame;
		i = 0;
		table = ide_get_partition_table();
		disk_pending = 1;
		_SecReads(sector + table[2 + movie.partition * 3], buf->buffer, movie.sectors_per_frame);
		extra = func_8013ff24();
		disk_pending += extra;
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
		if(extra)
		{
			func_8013fea4();
		}
		buf->state = 2;
		buf->frame = movie.frame_number;
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
}

/* 0x800f3580: vertical offset of the movie for the screen shake and one-shot scroll */
void update_movie_scroll(void)
{
	int	a;

	if(shake_count)
	{
		a = (int)shake_amp;
		movie.scroll = a;
		if(shake_count & 1)
		{
			movie.scroll = -a;
		}
		shake_amp = shake_amp - shake_decay;
		shake_count--;
	}
	else if(scroll_request)
	{
		movie.scroll = scroll_value;
		scroll_request = 0;
		scroll_value = -scroll_value;
	}
	else
	{
		movie.scroll = 0;
	}
	movie_scroll_center = (int)((float)movie.scroll + 128.0f);
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
	unsigned char	rgb[3];
	unsigned short	*fb;
	unsigned short	v;
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
	grLfbReadRegion(0, 0, MOVIE_WIDTH, MOVIE_HEIGHT, 800, fb);
	grLfbEnd();
	printf("read region\n");
	row = 0x18e70;
	for(y = 255; y >= 0; y--)
	{
		for(x = 0; x < MOVIE_WIDTH; x++)
		{
			v = fb[row + x];
			rgb[0] = (v >> 11) << 3;
			rgb[1] = (v >> 3) & 0xfc;
			rgb[2] = (v & 0x1f) << 3;
			fwrite(&rgb[0], 1, 1, f);
			fwrite(&rgb[1], 1, 1, f);
			fwrite(&rgb[2], 1, 1, f);
		}
		row -= MOVIE_WIDTH;
	}
#else
	(void)fb; (void)v; (void)x; (void)y; (void)row; (void)rgb;
#endif
	fclose(f);
	printf("grab_save_screen(): end\n");
}

/* 0x800e7c6c: distance of the movie pixel under (x, y), 300 when no movie is up */
float movie_depth_at(float x, float y)
{
	if(movie.disp->state == 2 && (movie.flags & MOVIE_PLAYING))
	{
		return(func_800c9158((unsigned int *)movie.disp->buffer + (int)y * MOVIE_WIDTH + (int)x));
	}
	return(300.0f);
}

#ifdef PORT_HOST
extern void	port_movie_blit(const unsigned int *buf, int scroll);
#endif

/* 0x800e77ec: the draw pass' first job: put the current frame on screen (or clear), then
   decode the next frame and rotate the buffers. Returns the frame number shown. */
int show_debug_frame_data(void)
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
			register unsigned int	*dma asm("$gp");
			unsigned int	*src;
			unsigned int	lfb;
			int		rows, top, bottom, i;

			grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);
			grDisableAllEffects();
			grTexCombineFunction(0, 0);
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
			for(i = 0; i < top; i++)
			{
				dma[2] = lfb & 0x1fffffff;
				lfb += 0x1000;
				dma[0] = movie.width << 2;
				dma[3] = (unsigned int)(dma + 4);
				dma[1] = (unsigned int)blank_row & 0x1fffffff;
				dma += 4;
			}
			for(i = 0; i < rows; i++)
			{
				dma[2] = lfb & 0x1fffffff;
				lfb += 0x1000;
				dma[1] = (unsigned int)src & 0x1fffffff;
				dma[3] = (unsigned int)(dma + 4);
				dma[0] = movie.width << 2;
				src += movie.width;
				dma += 4;
			}
			for(i = 0; i < bottom; i++)
			{
				dma[2] = lfb & 0x1fffffff;
				lfb += 0x1000;
				dma[0] = movie.width << 2;
				dma[3] = (unsigned int)(dma + 4);
				dma[1] = (unsigned int)blank_row & 0x1fffffff;
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
			graphical_printf(text_buffer, "FRAME %d   %d %d", movie.frame_number, frame, 0);
		}
		else
		{
			graphical_printf(text_buffer, "FRAME %d   %d LOCK %d %d", movie.frame_number, frame, D_8018db90, D_801e6e70);
		}
		draw_string(0, text_buffer, 0.0f, 220.0f);
		grDepthMask(FXTRUE);
		grDepthBufferFunction(GR_CMP_LEQUAL);
	}
	return(frame);
}

/* ---- the decoders: src is the raw frame, dst the 400x256 buffer of LFB words ---- */

#define PIX(tab, p)	*dst++ = tab[*p++]
#define PIX16(tab, p)	PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); \
			PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p); PIX(tab, p)
#define DPIX(pal, cp, dp)	*dst++ = pal[*cp++] | movie_depth_table[*dp++]
#define DPIX8(pal, cp, dp)	DPIX(pal, cp, dp); DPIX(pal, cp, dp); DPIX(pal, cp, dp); DPIX(pal, cp, dp); \
				DPIX(pal, cp, dp); DPIX(pal, cp, dp); DPIX(pal, cp, dp); DPIX(pal, cp, dp)

/* 0x800e8510 */
void decompress_movie(unsigned char *src, unsigned int *dst)
{
	unsigned short	*pal = (unsigned short *)src;
	unsigned char	*p = src + 0x1000;
	unsigned int	*tab;
	int		cols = movie.width / 16;
	int		strip, t, i, c;

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
			for(c = 0; c < cols; c++)
			{
				PIX16(tab, p);
			}
		}
	}
}

/* 0x800e8b78: eight palettes plus a depth table and a half-height depth plane */
void decoode_movie2(unsigned char *src, unsigned int *dst)
{
	unsigned char	*cp = src + 0x1000;
	unsigned short	*dtab = (unsigned short *)(cp + movie.width * 256);
	unsigned char	*dpa = (unsigned char *)dtab + 0x200;
	unsigned char	*dpb = dpa;
	unsigned short	*pal;
	int		cols = movie.width / 8;
	int		strip, t, i, c;

	for(i = 0; i < 256; i++)
	{
		movie_depth_table[i] = (unsigned int)dtab[i] << 16;
	}
	for(strip = 0; strip < 32; strip++)
	{
		for(t = 7; t >= 0; t--)
		{
			pal = (unsigned short *)(src + t * 0x200);
			for(c = 0; c < cols; c++)
			{
				DPIX8(pal, cp, dpa);
			}
			t--;
			pal = (unsigned short *)(src + t * 0x200);
			for(c = 0; c < cols; c++)
			{
				DPIX8(pal, cp, dpb);
			}
		}
	}
}

/* 0x800e82c8: one palette, no depth */
void decode_movie_flat(unsigned char *src, unsigned int *dst)
{
	unsigned short	*pal = (unsigned short *)src;
	unsigned char	*p = src + 0x200;
	int		cols = movie.width / 8;
	int		i, r, c;

	for(i = 0; i < 256; i++)
	{
		movie_flat_palette[i] = *pal++ | 0xffff0000;
	}
	for(r = 0; r < 128; r++)
	{
		for(c = 0; c < cols; c++)
		{
			PIX16(movie_flat_palette, p);
		}
	}
}

/* 0x800e878c: one palette plus the depth table and plane */
void decode_movie_flat_depth(unsigned char *src, unsigned int *dst)
{
	unsigned short	*pal = (unsigned short *)src;
	unsigned char	*cp = src + 0x200;
	unsigned short	*dtab = (unsigned short *)(cp + movie.width * 256);
	unsigned char	*dpa = (unsigned char *)dtab + 0x200;
	unsigned char	*dpb = dpa;
	int		cols = movie.width / 8;
	int		i, r, c;

	for(i = 0; i < 256; i++)
	{
		movie_depth_table[i] = (unsigned int)dtab[i] << 16;
	}
	for(r = 0; r < 128; r++)
	{
		for(c = 0; c < cols; c++)
		{
			DPIX8(pal, cp, dpa);
		}
		for(c = 0; c < cols; c++)
		{
			DPIX8(pal, cp, dpb);
		}
	}
}

/* ---- the script interpreter (0x801102d0) ---- */

/* script helpers called through opcode 5 (0x801109c0-0x80110ab0) */
void script_fade_out_108(void) { fade(0.0f, 108, 0); }
void script_fade_out_40(void) { fade(0.0f, 40, 0); }
void script_fade_out_40b(void) { fade(0.0f, 40, 0); }
void script_level_end_stats(void)
{
	func_800d6758(0x800);
	add_object1_wrapper(0x800, level_end_stats_wrapper, 0, 0);
}
void script_end_cleanup(void)
{
	func_800ebb44();
	func_800d4968();
	func_800ebe8c();
	fade(0.0f, 80, 0);
}
void script_fade_out_90(void) { fade(0.0f, 90, 0); }

#define NEXT()		(*movie_script_g2++)

/* returns the frame to read next, MOVIE_END when the movie is over */
int do_script(int frame)
{
	script_word_t	target;
	script_word_t	arg;
	int		(*fn)();
	int		next;

	if(script_abort)
	{
		end_of_wave();
		return(MOVIE_END);
	}
	if(debug_mode & 0x10000)
	{
		return(frame);
	}
	if(D_80212e14 != 1 && !(player_stats[0] & 0x44) && !(D_801e1084 & 0x44))
	{
		return(frame);
	}
	if(script_delay > 0)
	{
		script_delay--;
		return(frame);
	}
	if(script_skip)
	{
		frame = script_stop_frame;
		script_skip = 0;
	}
	next = frame + 1;
	for(;;)
	{
		if(frame < script_stop_frame)
		{
			return(next);
		}
		switch((int)NEXT())
		{
		case 1:		/* play until frame N */
			script_stop_frame = NEXT();
			return(next);
		case 2:		/* goto */
			movie_script_g2 = (script_word_t *)NEXT();
			break;
		case 3:		/* seek */
			next = NEXT();
			break;
		case 4:		/* if(*var) goto */
			arg = NEXT();
			target = NEXT();
			if(*(int *)arg)
			{
				movie_script_g2 = (script_word_t *)target;
			}
			break;
		case 5:		/* call */
			fn = (int (*)())NEXT();
			fn();
			break;
		case 6:
			func_800ebdbc(NEXT(), "generic1");
			break;
		case 7:		/* *var = 0 */
			*(int *)NEXT() = 0;
			break;
		case 8:		/* (*var)++ */
			(*(int *)NEXT())++;
			break;
		case 9:		/* end */
			return(MOVIE_END);
		case 10:	/* hold N ticks with the loader paused */
			script_delay = NEXT();
			movie_pause();
			return(next);
		case 11:	/* if(fn()) goto */
			fn = (int (*)())NEXT();
			target = NEXT();
			if(fn())
			{
				movie_script_g2 = (script_word_t *)target;
			}
			break;
		case 12:
			func_800ebdbc(NEXT(), "hhouse1");
			break;
		case 13:
			func_800ebdbc(NEXT(), "chastity");
			break;
		case 14:
			func_800ebdbc(NEXT(), "freaksho");
			break;
		case 15:
			func_800ebe6c();
			break;
		case 16:
			func_800ebe34(NEXT(), "freaksho");
			break;
		case 17:
			func_800ebd70(NEXT(), "freaksho");
			break;
		case 18:
			func_800ebe8c();
			break;
		case 19:
			arg = NEXT();
			target = NEXT();
			func_801400dc(arg, target, NEXT());
			break;
		case 20:	/* audits */
			switch((int)NEXT())
			{
			case 0x14: increment_audit(0x3b); break;
			case 0x15: increment_audit(0x41); break;
			case 0x16: increment_audit(0x42); break;
			case 0x17: increment_audit(0x43); break;
			case 0x18: increment_audit(0x44); break;
			case 0x19: increment_audit(0x48); break;
			case 0x1a: increment_audit(0x49); break;
			case 0x1b: increment_audit(0x4a); break;
			case 0x1c: increment_audit(0x4b); break;
			default: break;
			}
			break;
		case 21:
			func_800ebdbc(NEXT(), "rickity");
			break;
		case 22:
			func_800ebe34(NEXT(), "rickity");
			break;
		case 23:
			func_800ebd70(NEXT(), "rickity");
			break;
		case 24:	/* hold N ticks */
			script_delay = NEXT();
			return(next);
		case 25:
			arg = NEXT();
			func_801400a4(arg, NEXT());
			break;
		case 26:
			func_800ebdbc(NEXT(), "bigtop");
			break;
		case 27:
			func_800ebd70(NEXT(), "bigtop");
			break;
		case 28:
			func_800ec370(NEXT(), 0);
			break;
		case 29:
			arg = NEXT();
			if((unsigned)(arg - 0x11) < 2)
			{
				func_800ec504(arg);
			}
			else
			{
				func_800ebdbc(arg, "bigtop2");
			}
			break;
		default:
			printf("Unknown keyword %d in movie script\n", (int)movie_script_g2[-1]);
			break;
		}
	}
}
