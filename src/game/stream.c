/*
 * stream.c - the level music streamer (0x8013f508-0x8014012c and interrupt_ioasic_handler
 * 0x8013f678): raw stereo 16-bit PCM read from a disk partition into a 96 KB ring of 1 KB
 * chunks (32 KB per read, queued by the movie player's disk process) and pushed 512 words at
 * a time into the I/O ASIC's sound FIFO from the ASIC's FIFO interrupt, where the DCS2 board
 * picks it up (sound command 0x55D6 starts it). A wave selects the partition and its segment
 * table; the level scripts choose the loop region (stream_set_loop / stream_jump).
 */
#include <stdio.h>
#include <string.h>

extern int	_ioctl(int, int, int);
extern int	snd_send_data(unsigned int);
extern void	snd_clear_latch(void);
extern int	snd_query_buffers(int *);
extern int	snd_stream_volume(int);
extern void	*ide_get_partition_table(void);
extern short	_SecReads(unsigned int, void *, unsigned int);
extern void	graphical_printf(char *, char *, ...);
extern void	draw_string_color(int, char *, float, float, unsigned int);
extern char	text_buffer[];

#define FIOCGETSOUNDCTL		0x6907
#define FIOCSETSOUNDCTL		0x6906
#define FIOCGETMAINSTATUS	0x690e
#define FIOCSETMAINCTL		0x690f
#define FIOCGETMAINCTL		0x6910

#define RING_CHUNKS		0x60			/* 1 KB chunks: 96 KB */
#define CHUNK_FREE		1
#define CHUNK_READING		2
#define CHUNK_READY		4

int		stream_active;				/* 0x801e5bc0 */
int		stream_busy;				/* 0x801a25c8 */
volatile unsigned int *fifo_port = (volatile unsigned int *)0xb3000000;	/* 0x801a25cc: the ASIC sound FIFO */
unsigned short	chunk_state[RING_CHUNKS];		/* 0x801e6c98 */
unsigned char	stream_ring[RING_CHUNKS * 0x400];	/* 0x801fad70 */
int		fill_index;				/* 0x801e5ba4: next chunk the disk fills */
int		read_index;				/* 0x80237448: next chunk the FIFO takes */
int		stream_partition;			/* 0x801e5c04 */
int		stream_segments;			/* 0x801e0e74 */
int		*stream_part_table;			/* 0x801e5b14 */
int		partition_start;			/* 0x801e5c0c */
int		stream_sector;				/* 0x80212dac: next sector to read, within the partition */
int		loop_start, loop_end;			/* 0x801e7530, 0x801e7534 */
int		segment[10][2];				/* 0x802374b8: start and end sector of each segment */
int		stream_volume;				/* 0x80212de4 */

#ifdef PORT_HOST
extern void	dcs_fifo_write(unsigned int);
#define FIFO_WRITE(w)	dcs_fifo_write(w)
#else
#define FIFO_WRITE(w)	(*fifo_port = (w))
#endif

/* 0x8013f508: pulse the FIFO reset bits of the sound control register */
void sound_fifo_reset(void)
{
	int	ctl;

	_ioctl(4, FIOCGETSOUNDCTL, (int)(long)&ctl);
	ctl &= ~0xe;
	_ioctl(4, FIOCSETSOUNDCTL, ctl);
	_ioctl(4, FIOCGETSOUNDCTL, (int)(long)&ctl);
	ctl |= 0xe;
	_ioctl(4, FIOCSETSOUNDCTL, ctl);
}

/* 0x8013f574, 0x8013f5b4, 0x8013f5f4, 0x8013f634: main control bits (interrupt enables) */
void main_ctl_enable(void)
{
	int	ctl;
	_ioctl(4, FIOCGETMAINCTL, (int)(long)&ctl);
	ctl |= 1;
	_ioctl(4, FIOCSETMAINCTL, ctl);
}
void main_ctl_clear(void)
{
	int	ctl;
	_ioctl(4, FIOCGETMAINCTL, (int)(long)&ctl);
	ctl &= 0xc000;
	_ioctl(4, FIOCSETMAINCTL, ctl);
}
void main_ctl_fifo_int_on(void)
{
	int	ctl;
	_ioctl(4, FIOCGETMAINCTL, (int)(long)&ctl);
	ctl |= 8;
	_ioctl(4, FIOCSETMAINCTL, ctl);
}
void main_ctl_fifo_int_off(void)
{
	int	ctl;
	_ioctl(4, FIOCGETMAINCTL, (int)(long)&ctl);
	ctl &= ~8;
	_ioctl(4, FIOCSETMAINCTL, ctl);
}

void func_8013f5b4(void) { main_ctl_clear(); }		/* the name GOOSE main() (initg.c) calls it by */

/* 0x8013fae8, 0x8013fb20: restart the FIFO and its interrupt */
void fifo_restart(void)
{
	main_ctl_clear();
	sound_fifo_reset();
	main_ctl_fifo_int_on();
	main_ctl_enable();
}

/* 0x8013fbcc */
void stream_ring_reset(void)
{
	int	i;

	memset(stream_ring, 0, sizeof stream_ring);
	for(i = RING_CHUNKS - 1; i >= 0; i--)
	{
		chunk_state[i] = CHUNK_FREE;
	}
	read_index = 0;
	fill_index = 0;
}

/* 0x8013f6d8 */
void stream_reset(void)
{
	stream_active = 0;
	stream_ring_reset();
}

/* 0x8013fb58, 0x8013fb84 */
void stream_start(void)
{
	stream_active = 1;
	snd_send_data(0x55d6);
}
void stream_stop(void)
{
	stream_active = 0;
	fifo_restart();
	snd_send_data(0x55d7);
}

/* 0x8013fce0: queue the next 32 KB of the stream into the ring if 32 chunks are free;
   returns the number of disk reads issued (two when the loop end falls inside the read) */
int stream_fill(void)
{
	int	i;
	int	first;

	for(i = 0; i < 0x20; i++)
	{
		if(chunk_state[fill_index + i] != CHUNK_FREE)
		{
			return(0);
		}
	}
	for(i = 0; i < 0x20; i++)
	{
		chunk_state[fill_index + i] = CHUNK_READING;
	}
	if(loop_end < stream_sector + 0x40)
	{
		first = loop_end - stream_sector;
		if(first)
		{
			_SecReads(stream_sector + partition_start, stream_ring + (fill_index << 10), first);
			stream_sector = loop_start;
			_SecReads(loop_start + partition_start, stream_ring + ((fill_index + first / 2) << 10), 0x40 - first);
			stream_sector = stream_sector + 0x40 - first;
			return(2);
		}
		stream_sector = loop_start;
		_SecReads(loop_start + partition_start, stream_ring + (fill_index << 10), 0x40);
		stream_sector += 0x40;
		return(1);
	}
	_SecReads(stream_sector + partition_start, stream_ring + (fill_index << 10), 0x40);
	stream_sector += 0x40;
	return(1);
}

/* 0x8013fea4: the read landed: mark the chunks ready */
void stream_fill_done(void)
{
	int	i;

	for(i = 0; i < 0x20; i++)
	{
		chunk_state[fill_index + i] = CHUNK_READY;
	}
	fill_index = (fill_index + 0x20) % RING_CHUNKS;
}

/* 0x8013fc2c */
int stream_prefill(void)
{
	stream_fill();
	stream_fill_done();
	stream_fill();
	stream_fill_done();
	return(0);
}

/* 0x8013fc64: flush the board's stream buffers and restart the FIFO */
int stream_board_flush(void)
{
	int	n;

	if(snd_send_data(0x55d8))
	{
		return(1);
	}
	snd_clear_latch();
	fifo_restart();
	snd_query_buffers(&n);
	if(snd_send_data(0x55e7))
	{
		return(1);
	}
	do
	{
		snd_query_buffers(&n);
	} while(n > 0);
	return(0);
}

/* 0x8013ff24: called by the movie player's disk process after each frame read */
int stream_kick(void)
{
	int	r;

	stream_busy = 1;
	if(!stream_active)
	{
		stream_busy = 0;
		return(0);
	}
	r = stream_fill();
	stream_busy = 0;
	return(r);
}

/* 0x8013ff78: 512 words into the sound FIFO: the next chunk, or silence */
void stream_fifo_feed(void)
{
	int	i;
	unsigned short *p;

	if(!stream_active)
	{
		for(i = 0x1ff; i >= 0; i--)
		{
			FIFO_WRITE(0);
		}
		return;
	}
	if(chunk_state[read_index] != CHUNK_READY)
	{
		for(i = 0x1ff; i >= 0; i--)
		{
			FIFO_WRITE(0);
		}
		return;
	}
	p = (unsigned short *)(stream_ring + (read_index << 10));
	for(i = 0; i < 0x200; i++)
	{
		FIFO_WRITE(*p++);
	}
	chunk_state[read_index] = CHUNK_FREE;
	read_index = (read_index + 1) % RING_CHUNKS;
}

/* 0x8013f678: the I/O ASIC interrupt: FIFO wants data */
int interrupt_ioasic_handler(void)
{
	int	status;

	_ioctl(4, FIOCGETMAINSTATUS, (int)(long)&status);
	if((status & 1) && (status & 8))
	{
		stream_fifo_feed();
		_ioctl(4, FIOCGETMAINSTATUS, (int)(long)&status);
	}
	return(0);
}

/* 0x8013f700: select the wave's partition and segments, prime the ring and the board */
int stream_select_wave(int wave)
{
	int	*table;

	stream_sector = 0;
	if(wave == 0x1040000)
	{
		stream_partition = 7;
		stream_segments = 10;
		segment[0][0] = 0;       segment[0][1] = 0x8cc;
		segment[1][0] = 0x8cc;   segment[1][1] = 0x3a50;
		segment[2][0] = 0x3a50;  segment[2][1] = 0x3c5a;
		segment[3][0] = 0x3c5a;  segment[3][1] = 0x4edc;
		segment[4][0] = 0x4edc;  segment[4][1] = 0x5e00;
		segment[5][0] = 0x5e00;  segment[5][1] = 0xa4ae;
		segment[6][0] = 0xa4ae;  segment[6][1] = 0xa744;
		segment[7][0] = 0xa744;  segment[7][1] = 0xf54c;
		segment[8][0] = 0xf54c;  segment[8][1] = 0xfa80;
		segment[9][0] = 0xfa80;  segment[9][1] = 0x1241c;
		loop_start = 0x8cc;
		loop_end = 0x3a50;
	}
	else if(wave == 0x1060000)
	{
		stream_partition = 5;
		stream_segments = 4;
		segment[0][0] = 0;       segment[0][1] = 0x32e6;
		segment[1][0] = 0x32e6;  segment[1][1] = 0x4aae;
		segment[2][0] = 0x4aae;  segment[2][1] = 0x4ae4;
		segment[3][0] = 0x4ae4;  segment[3][1] = 0x73d2;
		segment[4][0] = 0x73d2;  segment[4][1] = 0x7ca2;
		segment[5][0] = 0x7ca2;  segment[5][1] = 0xbac0;
		loop_start = 0x32e6;
		loop_end = 0x4aae;
	}
	else if(wave == 0x1070000)
	{
		stream_partition = 9;
		stream_segments = 4;
		segment[0][0] = 0;       segment[0][1] = 0x396;
		segment[1][0] = 0x396;   segment[1][1] = 0x5242;
		segment[2][0] = 0x5242;  segment[2][1] = 0x5fb0;
		segment[3][0] = 0x5fb0;  segment[3][1] = 0x8490;
		segment[4][0] = 0x8490;  segment[4][1] = 0x8cd4;
		segment[5][0] = 0x8cd4;  segment[5][1] = 0xbf10;
		loop_start = 0x396;
		loop_end = 0x5242;
	}
	else if(wave == 0xd0000)
	{
		stream_partition = 9;
		stream_segments = 4;
		segment[0][0] = 0;       segment[0][1] = 0x396;
		segment[1][0] = 0x396;   segment[1][1] = 0x5242;
		segment[2][0] = 0x5242;  segment[2][1] = 0x5fb0;
		segment[3][0] = 0x5fb0;  segment[3][1] = 0x8490;
		segment[4][0] = 0x8490;  segment[4][1] = 0x8cd4;
		segment[5][0] = 0x8cd4;  segment[5][1] = 0xbf10;
		stream_sector = 0x8490;
		loop_start = 0x8cd4;
		loop_end = 0xbf10;
	}
	else
	{
		stream_partition = 3;
		stream_segments = 3;
		segment[0][0] = 0;       segment[0][1] = 0xab0;
		segment[1][0] = 0xab0;   segment[1][1] = 0x2976;
		segment[2][0] = 0x2976;  segment[2][1] = 0x3f42;
		loop_start = 0xab0;
		loop_end = 0x2976;
	}
	table = ide_get_partition_table();
	stream_part_table = table;
	partition_start = table[2 + stream_partition * 3];
	stream_ring_reset();
	if(stream_prefill())
	{
		return(1);
	}
	if(stream_board_flush())
	{
		return(1);
	}
	stream_volume = 0;
	return(0);
}

/* 0x80140074 */
void stream_set_volume(int volume)
{
	snd_stream_volume(volume);
	stream_volume = volume;
}

/* 0x801400a4: loop from segment a's start to segment b's end (after the current read) */
void stream_set_loop(int a, int b)
{
	loop_start = segment[a][0];
	loop_end = segment[b][1];
}

/* 0x801400dc: jump to segment a now and loop from b to c */
void stream_jump(int a, int b, int c)
{
	loop_start = segment[b][0];
	stream_sector = segment[a][0];
	loop_end = segment[c][1];
}

/* 0x8014012c: debug display of the ring: one number per chunk, coloured by state */
void stream_debug_display(void)
{
	int	i;
	unsigned int color;

	for(i = 0; i < RING_CHUNKS; i++)
	{
		graphical_printf(text_buffer, "%d", i);
		color = 0xffffff;
		if(chunk_state[i] == CHUNK_READING)
		{
			color = 0xff0000;
		}
		else if(chunk_state[i] == CHUNK_FREE)
		{
			color = 0xffffff;
		}
		else if(chunk_state[i] == CHUNK_READY)
		{
			color = 0xff00;
		}
		if(i == read_index)
		{
			color = 0xffff00;
		}
		if(i == fill_index)
		{
			color = 0xff;
		}
		draw_string_color(0, text_buffer, (float)(i % 8) * 20.0f, (float)(i / 8) * 20.0f, color);
	}
}
