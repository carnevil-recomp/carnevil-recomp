/*
 * dcs_board.cpp - CarnEvil's DCS2 sound board as the game's sound library sees it.
 *
 * Protocol (from src/sound/sound.c): after a reset the boot monitor puts 0x0A on the
 * sound-to-host port. Monitor commands: LOAD (0x1A start end type data...) answered with the
 * 16-bit checksum of the data words then 0x0A; RUN (0x2A) starts the uploaded operating
 * system, which answers 0x0C. OS commands are 0x55xx words with their arguments; anything
 * else is a sound call: call number, (volume << 8 | pan), priority. LOAD_DRAM (0x55D0/D1)
 * writes 16-bit words to DRAM word addresses and answers a checksum. The sound-call table
 * lives in DRAM words 0..0x3FFF (two words per call = the DRAM byte address of the call's
 * track program, 0xFFFFFFFF = unused); banks follow it. A track program is the byte code
 * described in DCSExplorer's format reference: type, channel, then {u16 wait, u8 opcode,
 * args}. The DRAM image keeps the file bytes in file order, so stream pointers (24-bit, big
 * endian, relative to their own position) index straight into it.
 */
#include <cmath>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <vector>
#include <deque>
#include <mutex>
#include "../third_party/dcsdecoder/DCSDecoder.h"
#include "../third_party/dcsdecoder/DCSDecoderNative.h"
#include "dcs_board.h"

#define SND_MONITOR_READY	0x0A
#define SND_OPSYS_READY		0x0C
#define STH_DATA_READY		0x0040
#define HTS_DATA_EMPTY		0x0080
#define SND_STATUS_REALTIME	0x8000
#define DRAM_WORDS		0x100000	/* 2 MB */
#define SAMPLES_PER_FRAME	240
#define OS_VERSION		0x0223

static int verbose;

class Board : public DCSDecoderNative
{
public:
	DCSDecoder::MinHost host;
	Board() : DCSDecoderNative(&host) { }

	std::mutex		lock;
	std::deque<uint16_t>	sth;		/* board -> host words */
	std::vector<uint8_t>	dram;		/* byte image, word w at dram[2w] (hi), dram[2w+1] (lo) */
	enum { MONITOR, OS } mode = MONITOR;
	bool			booted = false;

	/* host -> board parser */
	enum { IDLE, ARGS, DATA } st = IDLE;
	uint16_t	cmd = 0;
	uint16_t	args[8];
	int		nargs = 0, wantargs = 0;
	uint32_t	data_start = 0, data_end = 0, data_count = 0, data_pos = 0;
	uint32_t	checksum = 0;
	int		load_type = 0;
	uint16_t	callnum = 0;

	/* track programs */
	struct Track
	{
		bool		active = false;
		int		channel = 0;		/* the track number the host chose; the program's channels are relative to it */
		int		priority = 0;		/* low 7 bits of the host priority word */
		const uint8_t	*pc = 0;
		int		counter = 0;
		bool		op_next = false;	/* a wait was consumed: the next item is the opcode */
		struct Loop { int count; const uint8_t *pos; };
		std::vector<Loop> loops;
	};
	Track		tracks[MAX_CHANNELS];
	int		sample_in_frame = 0;
	/* the I/O ASIC's sound FIFO (512 words on the board; a few frames deep here because the
	   game's interrupt handler only runs between frames) carrying stereo 16-bit PCM */
	enum { FIFO_WORDS = 8192 };
	uint16_t	fifo[FIFO_WORDS];
	unsigned	fifo_head = 0, fifo_tail = 0, fifo_count = 0;
	bool		stream_on = false;
	int		stream_volume = 255;
	int		master_volume = 255;
	double		master_mult = 1.0;	/* the DCS master volume curve, kept with master_volume */
	unsigned long	stream_underruns = 0;
	void fifo_reset() { fifo_head = fifo_tail = fifo_count = 0; }
	unsigned fifo_space() { return FIFO_WORDS - fifo_count; }
	void fifo_write(uint16_t w) { if(fifo_count < FIFO_WORDS) { fifo[fifo_tail] = w; fifo_tail = (fifo_tail + 1) % FIFO_WORDS; fifo_count++; } }
	int16_t fifo_read() { int16_t w = (int16_t)fifo[fifo_head]; fifo_head = (fifo_head + 1) % FIFO_WORDS; fifo_count--; return w; }
	int		host_level[MAX_CHANNELS];	/* 0..255 from the sound-call volume byte */

	void reset()
	{
		std::lock_guard<std::mutex> g(lock);
		sth.clear();
		mode = MONITOR; st = IDLE;
		dram.assign(DRAM_WORDS * 2, 0);
		for(auto &t : tracks) t = Track();
		for(auto &l : host_level) l = 255;
		if(!booted) { SoftBoot(); SetMasterVolume(255); SetDefaultVolume(255); booted = true; }
		stop_all();
		sth.push_back(SND_MONITOR_READY);
		if(verbose) fprintf(stderr, "[dcs] reset: monitor ready\n");
	}

	void stop_all()
	{
		for(int ch = 0; ch < MAX_CHANNELS; ch++) stop_channel(ch);
		for(auto &t : tracks) t = Track();
	}
	void stop_track(int t)
	{
		if(t < 0 || t >= MAX_CHANNELS) return;
		tracks[t] = Track();
		for(int ch = 0; ch < MAX_CHANNELS; ch++) if(channel[ch].sourceChannel == t) stop_channel(ch);
		if(verbose) fprintf(stderr, "[dcs] stop track %d\n", t);
	}
	void stop_channel(int ch)
	{
		if(ch < 0 || ch >= MAX_CHANNELS) return;
		channel[ch].audioStream.Clear();
		channel[ch].audioStream.numFrames = 0;
		for(auto &m : channel[ch].mixer) m.Reset();
		channel[ch].sourceChannel = -1;
	}
	void set_level(int ch, int src, int level)
	{
		if(ch < 0 || ch >= MAX_CHANNELS || src < 0 || src >= MAX_CHANNELS) return;
		auto &m = channel[ch].mixer[src];
		m.curLevel = m.fadeTargetLevel = level << 6;
		m.fadeDelta = 0; m.fadeSteps = 0;
	}

	uint32_t dram32(uint32_t byteaddr)
	{
		if(byteaddr + 4 > dram.size()) return 0xffffffff;
		return ((uint32_t)dram[byteaddr] << 24) | ((uint32_t)dram[byteaddr + 1] << 16) | ((uint32_t)dram[byteaddr + 2] << 8) | dram[byteaddr + 3];
	}

	void write_dram_word(uint32_t waddr, uint16_t w)
	{
		if(waddr < DRAM_WORDS) { dram[2 * waddr] = w >> 8; dram[2 * waddr + 1] = w & 0xff; }
	}

	/* ---- host -> board ---- */
	void write(uint16_t w)
	{
		std::lock_guard<std::mutex> g(lock);
		switch(st)
		{
		case IDLE:	begin(w); break;
		case ARGS:
			args[nargs++] = w;
			if(nargs == wantargs) { st = IDLE; finish_args(); }
			break;
		case DATA:
			checksum += w;
			if(mode == OS) write_dram_word(data_start + data_pos, w);
			else if(cmd == 0x55DA) { /* stream buffer word: streaming not wired yet */ }
			data_pos++;
			if(data_pos >= data_count) { st = IDLE; finish_data(); }
			break;
		}
	}
	void want(int n) { st = ARGS; nargs = 0; wantargs = n; }
	/* the next word is bank data of a download made while sound plays (SND_CMD_LOAD_PLAY) */
	bool loading_while_playing() { return st == DATA && cmd == 0x55D1; }

	void begin(uint16_t w)
	{
		cmd = w;
		if(mode == MONITOR)
		{
			switch(w)
			{
			case 0x1A: want(3); return;				/* LOAD start end type */
			case 0x2A: mode = OS; sth.push_back(SND_OPSYS_READY);	/* RUN */
				if(verbose) fprintf(stderr, "[dcs] operating system running\n"); return;
			case 0x6A: sth.push_back(0x0100); return;		/* ROM version */
			case 0x8A: want(1); return;				/* echo */
			case 0x3A: sth.push_back(0xCC01); return;		/* memory tests */
			case 0x4A: sth.push_back(0xCC02); return;
			case 0x5A: sth.push_back(0xCC03); return;
			case 0xAA: sth.push_back(0xCC04); return;		/* bong */
			default: if(verbose) fprintf(stderr, "[dcs] monitor: unknown word %04x\n", w); return;
			}
		}
		if((w & 0xff00) == 0x5500)
		{
			switch(w)
			{
			case 0x55D0: case 0x55D1: case 0x55D5: want(4); return;	/* load / checksum DRAM: start hi lo, end hi lo */
			case 0x55D2: return;						/* flush pending calls */
			case 0x55D3: sth.clear(); return;				/* flush queues */
			case 0x55D4: sth.push_back(OS_VERSION); return;
			case 0x55AA: case 0x55AB: case 0x55AC: case 0x55AD: case 0x55AE: case 0x55AF:
			case 0x55B1: case 0x55B2: case 0x55B3: want(1); return;
			case 0x55B0: sth.push_back(0x0400); return;			/* sync */
			case 0x55E0: case 0x55E3: case 0x55E4: want(2); return;	/* engine/drone params */
			case 0x55D6: stream_on = true; if(verbose) fprintf(stderr, "[dcs] stream start\n"); return;
			case 0x55D7: stream_on = false; if(verbose) fprintf(stderr, "[dcs] stream stop (%lu underruns)\n", stream_underruns); return;
			case 0x55D8: fifo_reset(); if(verbose) fprintf(stderr, "[dcs] stream flush\n"); return;
			case 0x55E7: return;						/* FIFO streaming mode: what the game uses */
			case 0x55DB: case 0x55DC: return;
			case 0x55D9: sth.push_back(0); return;				/* free stream buffers: none, the FIFO is the buffer */
			case 0x55DA: if(verbose > 1) fprintf(stderr, "[dcs] stream buffer (dropped)\n"); sth.push_back(0xA1A1); st = DATA; data_count = SAMPLES_PER_FRAME * 2; data_pos = 0; checksum = 0; return;
			default: if(verbose) fprintf(stderr, "[dcs] unknown command %04x\n", w); return;
			}
		}
		callnum = w;			/* a sound call: volume/pan and priority follow */
		want(2);
	}

	void finish_args()
	{
		switch(cmd)
		{
		case 0x1A:
			load_type = args[2];
			data_count = (args[1] - args[0] + 1) * (load_type == 0 ? 2 : 1);
			data_pos = 0; checksum = 0; st = DATA;
			if(verbose) fprintf(stderr, "[dcs] monitor load %s %04x-%04x (%u words)\n", load_type ? "DM" : "PM", args[0], args[1], data_count);
			return;
		case 0x8A: sth.push_back(args[0]); return;
		case 0x55D0: case 0x55D1:
			data_start = ((uint32_t)args[0] << 16) | args[1];
			data_end = ((uint32_t)args[2] << 16) | args[3];
			data_count = data_end - data_start + 1; data_pos = 0; checksum = 0; st = DATA;
			if(verbose) fprintf(stderr, "[dcs] load DRAM words %06x-%06x\n", data_start, data_end);
			return;
		case 0x55D5:
		{
			uint32_t s = ((uint32_t)args[0] << 16) | args[1], e = ((uint32_t)args[2] << 16) | args[3], sum = 0;
			for(uint32_t a = s; a <= e && a < DRAM_WORDS; a++) sum += ((uint32_t)dram[2 * a] << 8) | dram[2 * a + 1];
			sth.push_back(sum & 0xffff);
			return;
		}
		case 0x55AA: master_volume = args[0] >> 8; master_mult = master_volume ? pow(0.981201, 255 - master_volume) : 0.0; SetMasterVolume(master_volume); if(verbose) fprintf(stderr, "[dcs] master volume %d\n", master_volume); return;
		case 0x55B3: stream_volume = args[0] & 0xff; if(verbose) fprintf(stderr, "[dcs] stream volume %d\n", stream_volume); return;
		case 0x55AB:			/* track volume: mask << 8 | volume */
			for(int ch = 0; ch < MAX_CHANNELS; ch++) if((args[0] >> 8) & (1 << ch)) channel[ch].channelVolume = args[0] & 0xff;
			return;
		case 0x55AE:			/* stop the track programs in the mask and the streams they started */
			for(int t = 0; t < MAX_CHANNELS; t++) if((args[0] >> 8) & (1 << t)) stop_track(t);
			return;
		case 0x55AF: reserved_mask = (args[0] >> 8) & 0x3f; return;		/* reserved track mask (not used by the allocator) */
		case 0x55AD:			/* track priority: mask << 8 | priority */
			for(int t = 0; t < MAX_CHANNELS; t++) if((args[0] >> 8) & (1 << t)) tracks[t].priority = args[0] & 0x7f;
			return;
		case 0x55AC: case 0x55B1: case 0x55B2:
		case 0x55E0: case 0x55E3: case 0x55E4:
			return;
		default:
			sound_call(callnum, args[0] >> 8, args[0] & 0xff, args[1]);
			return;
		}
	}

	void finish_data()
	{
		if(verbose) fprintf(stderr, "[dcs] data done: %u words, checksum %04x\n", data_count, checksum & 0xffff);
		sth.push_back(checksum & 0xffff);
		if(mode == MONITOR) sth.push_back(SND_MONITOR_READY);
	}

	/* ---- sound calls and track programs ---- */
	/* The host priority word (SOUND.H): 0x8000 = force the track in bits 7-9, else allocate one;
	   0x2000 = don't interrupt a track of equal or higher priority; low 7 bits = priority. */
	int pick_track(unsigned priority)
	{
		int level = priority & 0x7f;
		if(priority & 0x8000) return (priority >> 7) & 7;
		for(int i = 0; i < MAX_CHANNELS; i++)
			if(!(reserved_mask & (1 << i)) && !tracks[i].active && channel[i].sourceChannel != i) return i;
		int best = -1;
		for(int i = 0; i < MAX_CHANNELS; i++)
			if(!(reserved_mask & (1 << i)) && tracks[i].priority <= level && (best < 0 || tracks[i].priority < tracks[best].priority)) best = i;
		return best;
	}
	int reserved_mask = 0;

	void sound_call(unsigned call, int volume, int pan, unsigned priority)
	{
		(void)pan;
		if(call == 0) { stop_all(); reserved_mask = 0; if(verbose) fprintf(stderr, "[dcs] stop all\n"); return; }
		uint32_t entry = dram32(call * 4);
		int track = pick_track(priority);
		if(verbose) fprintf(stderr, "[dcs] sound call %u vol %d pan %d pri %04x -> %08x track %d\n", call, volume, pan, priority, entry, track);
		if(entry == 0xffffffff || entry + 2 >= dram.size() || track < 0 || track >= MAX_CHANNELS) return;
		if(verbose > 1) { fprintf(stderr, "[dcs]   program:"); for(unsigned i = 0; i < 40 && entry + i < dram.size(); i++) fprintf(stderr, " %02x", dram[entry + i]); fprintf(stderr, "\n"); }
		if((priority & 0x2000) && tracks[track].active && tracks[track].priority >= (int)(priority & 0x7f)) return;
		start_track(track, priority & 0x7f, dram.data() + entry, volume);
	}

	/* a bank program: type byte (1 = normal), number of channels it uses, then {u16 wait, op, args}...
	   with every channel number relative to the track it runs on */
	void start_track(int track, int priority, const uint8_t *p, int volume)
	{
		int type = p[0], nch = p[1];
		if(type != 1)
		{
			if(verbose) fprintf(stderr, "[dcs] track type %d (%d channels) not handled\n", type, nch);
			return;
		}
		stop_track(track);
		Track &t = tracks[track];
		t.active = true; t.channel = track; t.priority = priority; t.pc = p + 2;
		host_level[track] = volume;
		for(int k = 0; k < nch && k < MAX_CHANNELS; k++) channel[(track + k) % MAX_CHANNELS].channelVolume = volume;
		run(t);
	}

	uint16_t rd16(const uint8_t *&p) { uint16_t v = (p[0] << 8) | p[1]; p += 2; return v; }

	/* advance one program: executes ops until one asks for a wait */
	void run(Track &t)
	{
		for(int guard = 0; t.active && guard < 10000; guard++)
		{
			const uint8_t *&p = t.pc;
			if(p < dram.data() || p + 3 > dram.data() + dram.size()) { t.active = false; return; }
			if(!t.op_next)
			{
				uint16_t wait = rd16(p);
				if(wait) { t.counter = wait; t.op_next = true; return; }
			}
			t.op_next = false;
			uint8_t op = *p++;
			switch(op)
			{
			case 0x00: t.active = false; return;
			case 0x01:
			{
				int sch = (t.channel + *p++) % MAX_CHANNELS;
				const uint8_t *at = p;
				uint32_t rel = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2]; p += 3;
				int repeats = *p++;
				const uint8_t *stream = at + rel;
				if(stream >= dram.data() && stream + 16 < dram.data() + dram.size() && sch >= 0 && sch < MAX_CHANNELS)
				{
					DCSDecoder::ROMPointer rp(0, stream);
					channel[sch].trackPtr.Clear();
					LoadAudioStream(sch, t.channel, repeats == 0 ? 1 : repeats, rp);
					if(verbose) fprintf(stderr, "[dcs]   play stream +%06x on channel %d x%d\n", (unsigned)(stream - dram.data()), sch, repeats);
				}
				break;
			}
			case 0x02: stop_channel((t.channel + *p++) % MAX_CHANNELS); break;
			case 0x03: { uint16_t call = rd16(p); sound_call(call, host_level[t.channel], 0x7f, 0); break; }
			case 0x04: p += 3; break;			/* host event timer */
			case 0x05: p += 1; break;			/* start deferred track */
			case 0x06: p += 2; break;			/* set variable */
			case 0x07: { int ch = (t.channel + *p++) % MAX_CHANNELS; int lvl = (int8_t)*p++; set_level(ch, t.channel, lvl); break; }
			case 0x08: { int ch = (t.channel + *p++) % MAX_CHANNELS; int d = (int8_t)*p++; if(ch < MAX_CHANNELS) set_level(ch, t.channel, (channel[ch].mixer[t.channel].curLevel >> 6) + d); break; }
			case 0x09: { int ch = (t.channel + *p++) % MAX_CHANNELS; int d = (int8_t)*p++; if(ch < MAX_CHANNELS) set_level(ch, t.channel, (channel[ch].mixer[t.channel].curLevel >> 6) - d); break; }
			case 0x0a: { int ch = (t.channel + *p++) % MAX_CHANNELS; int lvl = (int8_t)*p++; p += 2; set_level(ch, t.channel, lvl); break; }
			case 0x0b: { int ch = (t.channel + *p++) % MAX_CHANNELS; int d = (int8_t)*p++; p += 2; if(ch < MAX_CHANNELS) set_level(ch, t.channel, (channel[ch].mixer[t.channel].curLevel >> 6) + d); break; }
			case 0x0c: { int ch = (t.channel + *p++) % MAX_CHANNELS; int d = (int8_t)*p++; p += 2; if(ch < MAX_CHANNELS) set_level(ch, t.channel, (channel[ch].mixer[t.channel].curLevel >> 6) - d); break; }
			case 0x0d: break;
			case 0x0e: { int n = *p++; t.loops.push_back({ n, p }); break; }
			case 0x0f:
				if(!t.loops.empty())
				{
					auto &l = t.loops.back();
					if(l.count > 1) { l.count--; p = l.pos; }
					else t.loops.pop_back();
				}
				break;
			case 0x10: p += 2; break;
			case 0x11: p += 4; break;
			default:
				if(verbose) fprintf(stderr, "[dcs] unknown track opcode %02x\n", op);
				t.active = false; return;
			}
		}
	}

	void frame_tick()
	{
		for(auto &t : tracks)
		{
			if(!t.active) continue;
			if(t.counter > 0 && --t.counter > 0) continue;
			run(t);
		}
	}

	void render(short *out, int n)
	{
		std::lock_guard<std::mutex> g(lock);
		if(mode != OS) { memset(out, 0, n * sizeof *out); return; }
		for(int i = 0; i < n; i++)
		{
			if(sample_in_frame == 0) frame_tick();
			int s = GetNextSample();
			if(stream_on)
			{
				if(fifo_count >= 2)
				{
					int l = fifo_read(), r = fifo_read();
					/* the stream goes through the DSP's output stage like every other sound, so the
					   master volume scales it with the DCS curve (0.981201 per step below 255,
					   as the decoder does), not linearly; the stream volume command is linear */
					s += (int)(((l + r) / 2) * stream_volume / 255 * master_mult);
				}
				else stream_underruns++;
			}
			out[i] = s > 32767 ? 32767 : s < -32768 ? -32768 : s;
			if(++sample_in_frame == SAMPLES_PER_FRAME) sample_in_frame = 0;
		}
	}
};

static Board *board;
static Board *get(void) { if(!board) board = new Board(); return board; }

extern "C" {
void dcs_reset(void) { get()->reset(); }
void dcs_write(unsigned int w) { get()->write(w & 0xffff); }
int dcs_loading_while_playing(void) { Board *b = get(); std::lock_guard<std::mutex> g(b->lock); return b->loading_while_playing(); }
unsigned dcs_status(void)
{
	Board *b = get(); std::lock_guard<std::mutex> g(b->lock);
	return HTS_DATA_EMPTY | SND_STATUS_REALTIME | (b->sth.empty() ? 0 : STH_DATA_READY);
}
unsigned dcs_read(void)
{
	Board *b = get(); std::lock_guard<std::mutex> g(b->lock);
	if(verbose && !b->sth.empty()) fprintf(stderr, "[dcs] host reads %04x\n", b->sth.front());
	return b->sth.empty() ? 0 : b->sth.front();
}
void dcs_clear_sth(void)
{
	Board *b = get(); std::lock_guard<std::mutex> g(b->lock);
	if(!b->sth.empty()) b->sth.pop_front();
}
void dcs_render(short *out, int n) { get()->render(out, n); }
void dcs_fifo_write(unsigned int w) { Board *b = get(); std::lock_guard<std::mutex> g(b->lock); b->fifo_write((uint16_t)w); }
unsigned dcs_fifo_space(void) { Board *b = get(); std::lock_guard<std::mutex> g(b->lock); return b->fifo_space(); }
void dcs_fifo_reset(void) { Board *b = get(); std::lock_guard<std::mutex> g(b->lock); b->fifo_reset(); }
void dcs_set_verbose(int on) { verbose = on; }
}
