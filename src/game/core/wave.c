/*
 * wave.c - loading and unloading a wave (0x800f56d0-0x800f70b8): a level, the attract loop,
 * the level-select gallery or the high-score entry screen. preload_wave() shows the jester
 * while a "loadwave" process loads the sound banks, the enemies' texmaps and models, the
 * lights, the camera file and the spawn table, then starts the level's music stream.
 * unload_wave() takes it all down again. The big top swaps its act-one assets for the finale's
 * in the middle of the level (btopload_proc). The rest of the file (0x800f5244-0x800f5658,
 * 0x800f70b8-0x800f7420: the jester's own object code) is still assembly.
 */
#include <stdio.h>
#include <goose/object.h>
#include "objects.h"
#include "credits.h"			/* debug_mode, player_stats */
#include "waves.h"
#include "levelsnd.h"
#include "movie.h"

extern void	sleep(int);
extern void	*qcreate(char *, int, void (*)(), int, int, int, int);
extern void	*cur_proc;
extern void	suspend_self(void), resume_process(void);
extern void	install_disk_info(void *, void *, void *);
extern void	ide_set_partition(int);
extern void	install_disk_callback(void *);
extern int	get_free_mem_left(void);
extern void	func_80162a10(int *size, int *avail, int *used, int *frag);	/* texture memory statistics */
extern void	func_80163eec(int);
extern int	snd_bank_load_playing(char *), snd_bank_delete(char *), snd_stop_all(void), snd_delay(int);
extern int	snd_scall_bank(char *, int, int, int, int);
extern void	check_sound_os(void);
extern void	remove_all_objects(void), disable_spawning(void);
#include "sprites.h"
extern void	free_level_camera(void), update_view_matrix(void), load_level_camera(int);
extern void	enable_guns(void), disable_guns(void), func_800d4978(void);
extern float	camera_x, camera_y, camera_z, camera_rx, camera_ry, camera_rz;
extern void	load_level_lights(int);
extern int	load_level_spawn(int);
extern int	stream_select_wave(int);
extern void	stream_start(void);
extern void	reset_fog(void), free_lights(void), free_chunks(void);
extern void	set_jester_state(struct enemy *, int);
extern void	draw_jester(game_object_t *), jester_think(game_object_t *);	/* the loading jester (object 0x2014) */
extern int	init_jester(game_object_t *);
int	available_free_mem;					/* 0x801de8a8 */
int	sound_init_pending;					/* 0x801e6ec0 */
extern int	D_801a25c8, D_80212da4, D_801e5cd4, D_801e0f10;
extern int	hhouse_texmap_set;

/* the enemies' texmaps, models and animations, one file per enemy (all still assembly) */
extern void	load_hhouse_jester_anim(void), load_rickity_jester_anim(void), load_bigtop_jester_anim(void), load_freakshow_jester_anim(void);
extern void	remove_hhouse_jester_anim(void), remove_rickity_jester_anim(void), remove_bigtop_jester_anim(void), remove_freakshow_jester_anim(void);
extern void	jester_cue_hhouse(void), jester_cue_freakshow(void), jester_cue_rickity(void), jester_cue_bigtop(void);
extern void	load_ehstd_texmap(void), remove_ehstd_texmap(void), load_hstd_texmap(void), remove_hstd_texmap(void);
extern void	load_attract_texmap(void), remove_attract_texmap(void), load_gallery_objects(void), remove_gallery_objects(void);
extern void	remove_flash_texmap(void), remove_hhouse_enemy_weaps(void), remove_bat_assets(void), remove_gool(void), func_80106504(void);
extern void	func_80106de0(void), func_8010584c(void), func_800f75f8(void), remove_spider_assets(void), func_800f8798(void);
extern void	remove_mari(void), remove_mike_assets(void), remove_hhouse_texmap(void), load_hhouse_texmap(void);
extern void	load_mike_texmap(void), load_mari_texmap(void), load_zombie_texmap(void), load_spider_texmap(void), load_hand_texmap(void);
extern void	load_spdl_texmap(void), load_vict_texmap(void), load_bat_texmap(void), load_hhouse_enemy_weap_texmaps(void);
extern void	load_gool_texmap(void), load_flash_texmap(void), load_zomb(void), load_mari(void), load_mike(void), load_gool(void);
extern void	load_spider(void), load_hand(void), load_inng(void), load_spdl(void), load_vict(void), load_bat(void);
extern void	load_hhouse_enemy_weap_models(void), func_8011536c(void);
extern void	remove_freakshow_weaps(void), remove_taur_assets(void), remove_rodz_assets(void), remove_tort_assets(void);
extern void	remove_junior_texmap(void), remove_grub_assets(void), remove_monk_asets(void), remove_flie_assets(void);
extern void	remove_tops_assets(void), remove_finn_assts(void), remove_freakshow_texmap(void);
extern void	load_freakshow_texmap(void), load_tops_texmap(void), load_flie_texmap(void), load_monk_texmap(void), load_grub_texmap(void);
extern void	load_junoir_texmap(void), load_tort_texmap(void), load_rodz_texmap(void), load_taur_texmap(void);
extern void	load_freakshow_enemy_weap_texmaps(void), load_tops(void), load_tort(void), load_monk(void), load_flie(void);
extern void	load_junior(void), load_taur(void), load_grub(void), load_finn(void), load_rodz(void), load_freakshow_enemy_weap_models(void);
extern void	remove_btop_weaps(void), func_8013c2a4(void), remove_litl_assets(void), remove_clown_assets(void), remove_batb_texmap(void);
extern void	remove_mame_assets(void), func_80131330(void), remove_binn_assets(void), remove_skeleton_assets(void);
extern void	remove_jester_path(void), func_80139444(void), remove_btop_gore_texmaps(void);
extern void	load_toke_texmap(void), load_toke(void), load_jester_path(void);
extern void	load_btop_gore_texmaps(void), load_pood_texmap(void), load_mame_texmap(void), load_batb_texmap(void), load_clown_texmap(void);
extern void	load_litl_texmap(void), load_doctorklot_texmap(void), load_skeleton_texmap(void), load_btop_weap_texmaps(void);
extern void	load_pood(void), load_mame(void), load_batb(void), load_clown(void), load_litl(void), load_doctor_klot(void);
extern void	load_binn(void), load_skeleton(void), load_btop_weap_models(void);
extern void	remove_junk(void), remove_rinn_assets(void), remove_term_assets(void), remove_dino_assets(void), remove_teen_assets(void);
extern void	remove_krampus_assets(void), remove_krinkle_assets(void), remove_smilingbob_assets(void), remove_rickity_texmap(void);
extern void	load_rickity_texmap(void), load_smilingbob_texmap(void), load_krinkle_texmap(void), load_krampus_texmap(void);
extern void	load_teen_texmap(void), load_dino_texmap(void), load_term_texmap(void), load_rickity_object_texmaps(void), load_smilingbob(void);
extern void	load_krinkle(void), load_krampus(void), load_teen(void), load_dino(void), load_term(void), load_rinn(void), load_junk(void);

int	loaded_wave;			/* 0x80212e30 (wave_id_g2): the wave loaded, 0 for none */
int	wave_busy;			/* 0x80212e60: 1 while a wave loads or unloads (and the frame clock stops) */
int	free_mem_after_load;		/* 0x801e5be0 */
int	bigtop_act2;			/* 0x801e0f4c: the big top's finale assets are in */

void	load_wave_proc(void), btopload_proc(void);
void	load_level_sounds(int), remove_level_sounds(int);
void	remove_hhouse_assets(void), load_hhouse_assets(void), remove_freakshow_assets(void), load_freakshow_assets(void);
void	remove_bigtop_assets(void), load_bigtop_assets(void), remove_rickitytown_assets(void), load_rickity_town_assets(void);
void	remove_attract_assets(void), load_attract_assets(void), remove_gallery_assets(void), load_gallery_objects_wrapper(void);

#ifdef PORT_HOST
extern int	port_trace;
extern unsigned long hal_frame_count;
#define WAVE_TRACE(what)	if(port_trace) fprintf(stderr, "[wave] vblank %lu %s\n", hal_frame_count, what)
/* every sound bank load reports its result (0 = OK) */
static int traced_bank_load(char *name)
{
	int	r = snd_bank_load_playing(name);

	if(port_trace) fprintf(stderr, "[wave] bank %s -> %d\n", name, r);
	return r;
}
#define snd_bank_load_playing(n)	traced_bank_load(n)
#else
#define WAVE_TRACE(what)
#endif

#define TEXMEM_REPORT(when) \
	if(debug_mode & DEBUG_TEXMAP_LOG) \
	{ \
		func_80162a10(&size, &avail, &used, &frag); \
		printf(when "\nTexMem size = %dK, avail = %dK, used = %dK, frag = %d bytes\n", size / 1024, avail / 1024, used / 1024, frag); \
	}

/* 0x800f56d0 */
void unload_wave(void)
{
	int	size, avail, used, frag;

	while(wave_busy == 1)
	{
		sleep(1);
	}
	wave_busy = 1;
	TEXMEM_REPORT("before unload wave...");
	if(debug_mode & DEBUG_HEAP)
	{
		printf("Pre unload wave heap %d\n", get_free_mem_left());
	}
	clean_up_level_sounds();
	remove_all_objects();
	disable_spawning();
	free_level_camera();
	free_lights();
	snd_stop_all();
	snd_delay(20);
	switch(loaded_wave)
	{
	case WAVE_HISCORE_ENTRY:
		remove_ehstd_texmap();
		break;
	case WAVE_ATTRACT:
		remove_attract_assets();
		break;
	case WAVE_LEVEL_SELECT:
		remove_gallery_assets();
		break;
	case WAVE_HHOUSE:
		remove_hhouse_assets();
		break;
	case WAVE_FREAKSHOW:
		remove_freakshow_assets();
		break;
	case WAVE_RICKITY:
		remove_rickitytown_assets();
		break;
	case WAVE_BIGTOP:
		remove_bigtop_assets();
		break;
	}
	func_80163eec(D_801e0f10 + 1);
	TEXMEM_REPORT("after unload wave...");
	if(debug_mode & DEBUG_HEAP)
	{
		available_free_mem -= get_free_mem_left();
		printf("Post unload wave heap %d\n", get_free_mem_left());
		printf("Heap delta %d\n\n", available_free_mem);
	}
	remove_level_sounds(loaded_wave);
	loaded_wave = 0;
	wave_busy = 0;
}

/* 0x800f59dc: load a wave: the jester dances (object 0x2014) while the "loadwave" process
   does the work, then the objects and the jester's animation go */
void preload_wave(int wave)
{
	short	obj;

	while(wave_busy == 1)
	{
		sleep(1);
	}
	wave_busy = 1;
	loaded_wave = wave;
	if(wave == WAVE_ATTRACT)
	{
		check_sound_os();
	}
	reset_fog();
	func_800d4978();
	camera_x = 0;
	camera_y = 0;
	camera_rx = 0;
	camera_ry = 0;
	camera_rz = 0;
	camera_z = -9.0f;
	update_view_matrix();
	if(wave & WAVE_LEVEL)
	{
		switch(wave)
		{
		case WAVE_HHOUSE:
			load_hhouse_jester_anim();
			break;
		case WAVE_RICKITY:
			load_rickity_jester_anim();
			break;
		case WAVE_BIGTOP:
			load_bigtop_jester_anim();
			break;
		case WAVE_FREAKSHOW:
		default:
			load_freakshow_jester_anim();
			break;
		}
		obj = add_object1_wrapper(0x2014, draw_jester, init_jester, jester_think);
		if(obj)
		{
			set_jester_state(get_object_data(obj), 0x100);
		}
	}
	clean_up_level_sounds();
	while(D_801a25c8)
	{
		sleep(1);
	}
	WAVE_TRACE("preload_wave: loadwave process");
	qcreate("loadwave", 0x800, load_wave_proc, 0, 0, 0, 0);
	while(wave_busy == 1)
	{
		sleep(1);
	}
	WAVE_TRACE("preload_wave: loaded");
	if(D_80212da4 == 5)
	{
		D_801e5cd4 = 1;
	}
	else
	{
		D_801e5cd4 = 0;
	}
	if(wave & WAVE_LEVEL)
	{
		remove_all_objects();
		free_world_sprites();
		switch(wave)
		{
		case WAVE_HHOUSE:
			remove_hhouse_jester_anim();
			break;
		case WAVE_RICKITY:
			remove_rickity_jester_anim();
			break;
		case WAVE_BIGTOP:
			remove_bigtop_jester_anim();
			break;
		case WAVE_FREAKSHOW:
		default:
			remove_freakshow_jester_anim();
			break;
		}
	}
}

/* 0x800f5cbc: the "loadwave" process: the wave's sound banks, assets, lights, camera and
   spawn table, then its music stream */
void load_wave_proc(void)
{
	int	size, avail, used, frag;

	if(loaded_wave & WAVE_LEVEL)
	{
		play_level_cue(3, "wavinmus");
	}
	TEXMEM_REPORT("before load wave...");
	if(debug_mode & DEBUG_HEAP)
	{
		available_free_mem = get_free_mem_left();
		printf("Pre load wave heap %d  wave 0x%08x\n", available_free_mem, loaded_wave);
	}
	WAVE_TRACE("load_wave_proc: banks");
	install_disk_info(&cur_proc, suspend_self, resume_process);
	ide_set_partition(1);
	switch(loaded_wave)
	{
	case WAVE_HHOUSE:
		snd_bank_load_playing("jesthhou");
		jester_cue_hhouse();
		break;
	case WAVE_FREAKSHOW:
		snd_bank_load_playing("jestfrek");
		jester_cue_freakshow();
		break;
	case WAVE_RICKITY:
		snd_bank_load_playing("jesricky");
		jester_cue_rickity();
		break;
	case WAVE_BIGTOP:
		snd_bank_load_playing("jesbig");
		jester_cue_bigtop();
		break;
	}
	load_level_sounds(loaded_wave);
	switch(loaded_wave)
	{
	case WAVE_HHOUSE:
		load_hhouse_assets();
		break;
	case WAVE_FREAKSHOW:
		load_freakshow_assets();
		break;
	case WAVE_RICKITY:
		load_rickity_town_assets();
		break;
	case WAVE_BIGTOP:
		bigtop_act2 = 0;
		load_bigtop_assets();
		free_world_sprites();
		break;
	case WAVE_ATTRACT:
		load_attract_assets();
		break;
	case WAVE_LEVEL_SELECT:
		load_gallery_objects_wrapper();
		break;
	case WAVE_HISCORE_ENTRY:
		load_ehstd_texmap();
		break;
	default:
		printf("Unknown wave 0x%08x in load_wave_proc!\n", loaded_wave);
		break;
	}
	WAVE_TRACE("load_wave_proc: lights, camera, spawn");
	load_level_lights(loaded_wave);
	load_level_camera(loaded_wave);
	WAVE_TRACE("load_wave_proc: spawn");
	load_level_spawn(loaded_wave);
	TEXMEM_REPORT("after load...");
	if(debug_mode & DEBUG_HEAP)
	{
		printf("Post load wave heap %d\n", get_free_mem_left());
	}
	free_mem_after_load = get_free_mem_left();
	if(loaded_wave & WAVE_LEVEL)		/* only the levels stream music from the disk */
	{
		stop_level_cue();
		install_disk_info(0, 0, 0);
		reset_stream_volume();
		WAVE_TRACE("load_wave_proc: stream");
		stream_select_wave(loaded_wave);
		sleep(3);
		stream_start();
	}
	WAVE_TRACE("load_wave_proc: done");
	wave_busy = 0;
}

/* 0x800f60ec, 0x800f616c */
void remove_hhouse_assets(void)
{
	remove_flash_texmap();
	remove_hhouse_enemy_weaps();
	remove_bat_assets();
	remove_gool();
	func_80106504();
	func_80106de0();
	func_8010584c();
	func_800f75f8();
	remove_spider_assets();
	func_800f8798();
	remove_mari();
	remove_mike_assets();
	remove_hhouse_texmap();
}

void load_hhouse_assets(void)
{
	install_disk_callback(0);
	load_hhouse_texmap();
	load_mike_texmap();
	load_mari_texmap();
	load_zombie_texmap();
	load_spider_texmap();
	load_hand_texmap();
	load_spdl_texmap();
	load_vict_texmap();
	load_bat_texmap();
	load_hhouse_enemy_weap_texmaps();
	load_gool_texmap();
	load_flash_texmap();
	load_zomb();
	load_mari();
	load_mike();
	load_gool();
	load_spider();
	load_hand();
	load_inng();
	load_spdl();
	load_vict();
	load_bat();
	load_hhouse_enemy_weap_models();
	func_8011536c();
}

/* 0x800f624c: the wave's sound banks (the levels also get the jester's bank first, above) */
void load_level_sounds(int wave)
{
	ide_set_partition(1);
	sound_init_pending = 1;
	switch(wave)
	{
	case WAVE_LEVEL_SELECT:
		snd_bank_load_playing("select1");
		break;
	case WAVE_ATTRACT:
		snd_bank_load_playing("select1");
		snd_bank_load_playing("midway");
		snd_bank_load_playing("intro4");
		snd_bank_load_playing("attract");
		break;
	case WAVE_HISCORE_ENTRY:
	case WAVE_HHOUSE_SOUNDS:
		break;
	case WAVE_HHOUSE:
		snd_bank_load_playing("chastity");
		snd_bank_load_playing("goul");
		snd_bank_load_playing("hhouse1");
		snd_bank_load_playing("zombie");
		snd_bank_load_playing("jester");
		snd_bank_load_playing("hambone");
		break;
	case WAVE_FREAKSHOW:
		snd_bank_load_playing("freaksho");
		snd_bank_load_playing("flapjack");
		snd_bank_load_playing("rodz");
		snd_bank_load_playing("tort");
		snd_bank_load_playing("taur");
		snd_bank_load_playing("baby");
		snd_bank_load_playing("monkey");
		snd_bank_load_playing("fly");
		break;
	case WAVE_RICKITY:
		snd_bank_load_playing("bob");
		snd_bank_load_playing("krinkle");
		snd_bank_load_playing("teena");
		snd_bank_load_playing("carny");
		snd_bank_load_playing("krampus");
		snd_bank_load_playing("teenb");
		snd_bank_load_playing("rickity");
		break;
	case WAVE_BIGTOP:
		snd_bank_load_playing("skeleton");
		snd_bank_load_playing("tokken");
		snd_bank_load_playing("bigtop");
		snd_bank_load_playing("mortito");
		snd_bank_load_playing("smeek");
		snd_bank_load_playing("klott");
		snd_bank_load_playing("pood");
		snd_bank_load_playing("ozob");
		break;
	default:
		printf("Bad wave passed to load level sounds - 0x%08x\n", wave);
		break;
	}
	sound_init_pending = 0;
}

/* 0x800f6574 */
void remove_level_sounds(int wave)
{
	snd_stop_all();
	snd_delay(20);
	switch(wave)
	{
	case WAVE_ATTRACT:
		snd_bank_delete("attract");
		snd_bank_delete("intro4");
		snd_bank_delete("midway");
	case WAVE_LEVEL_SELECT:
		snd_bank_delete("select1");
		break;
	case WAVE_HHOUSE_SOUNDS:
		snd_bank_delete("hhouse1");
		snd_bank_delete("zombie");
		snd_bank_delete("hambone");
		break;
	case WAVE_HHOUSE:
		snd_bank_delete("hambone");
		snd_bank_delete("jester");
		snd_bank_delete("zombie");
		snd_bank_delete("hhouse1");
		snd_bank_delete("goul");
		snd_bank_delete("chastity");
		snd_bank_delete("jesthhou");
		break;
	case WAVE_FREAKSHOW:
		snd_bank_delete("fly");
		snd_bank_delete("monkey");
		snd_bank_delete("baby");
		snd_bank_delete("taur");
		snd_bank_delete("tort");
		snd_bank_delete("rodz");
		snd_bank_delete("flapjack");
		snd_bank_delete("freaksho");
		snd_bank_delete("jestfrek");
		break;
	case WAVE_RICKITY:
		snd_bank_delete("rickity");
		snd_bank_delete("teenb");
		snd_bank_delete("krampus");
		snd_bank_delete("carny");
		snd_bank_delete("teena");
		snd_bank_delete("krinkle");
		snd_bank_delete("bob");
		snd_bank_delete("jesricky");
		break;
	case WAVE_BIGTOP:
		if(!bigtop_act2)
		{
			snd_bank_delete("ozob");
			snd_bank_delete("pood");
			snd_bank_delete("klott");
			snd_bank_delete("smeek");
			snd_bank_delete("mortito");
			snd_bank_delete("bigtop");
		}
		else
		{
			snd_bank_delete("ending");
			snd_bank_delete("bigtop2");
			snd_bank_delete("jesbig2");
			snd_bank_delete("tokken2");
		}
		snd_bank_delete("tokken");
		snd_bank_delete("skeleton");
		snd_bank_delete("jesbig");
		break;
	case WAVE_HISCORE_ENTRY:
		break;
	default:
		printf("Bad wave passed to load level sounds - 0x%08x\n", wave);
		break;
	}
}

/* 0x800f693c, 0x800f69ac */
void remove_freakshow_assets(void)
{
	remove_freakshow_weaps();
	remove_taur_assets();
	remove_rodz_assets();
	remove_tort_assets();
	remove_junior_texmap();
	remove_grub_assets();
	remove_monk_asets();
	remove_flie_assets();
	remove_tops_assets();
	remove_finn_assts();
	remove_freakshow_texmap();
}

void load_freakshow_assets(void)
{
	install_disk_callback(0);
	load_freakshow_texmap();
	load_tops_texmap();
	load_flie_texmap();
	load_monk_texmap();
	load_grub_texmap();
	load_junoir_texmap();
	load_tort_texmap();
	load_rodz_texmap();
	load_taur_texmap();
	load_freakshow_enemy_weap_texmaps();
	load_tops();
	load_tort();
	load_monk();
	load_flie();
	load_junior();
	load_taur();
	load_grub();
	load_finn();
	load_rodz();
	load_freakshow_enemy_weap_models();
}

/* 0x800f6a6c: the big top's assets, whichever act is loaded */
void remove_bigtop_assets(void)
{
	remove_btop_weaps();
	if(!bigtop_act2)
	{
		func_8013c2a4();
		remove_litl_assets();
		remove_clown_assets();
		remove_batb_texmap();
		remove_mame_assets();
		func_80131330();
		remove_binn_assets();
		remove_skeleton_assets();
	}
	else
	{
		remove_binn_assets();
		remove_skeleton_assets();
		remove_jester_path();
		func_80139444();
	}
	remove_btop_gore_texmaps();
}

/* 0x800f6b0c, 0x800f6b54: act one out, the finale in (btopload_proc does the same inline) */
void remove_bigtop_act1(void)
{
	func_8013c2a4();
	remove_litl_assets();
	remove_clown_assets();
	remove_batb_texmap();
	remove_mame_assets();
	func_80131330();
}

void load_bigtop_act2(void)
{
	bigtop_act2 = 1;
	snd_bank_delete("ozob");
	snd_bank_delete("pood");
	snd_bank_delete("klott");
	snd_bank_delete("smeek");
	snd_bank_delete("mortito");
	snd_bank_delete("bigtop");
	snd_bank_load_playing("tokken2");
	snd_bank_load_playing("bigtop2");
	snd_bank_load_playing("jesbig2");
	snd_bank_load_playing("ending");
	load_toke_texmap();
	load_toke();
	load_jester_path();
}

/* 0x800f6c30 */
void load_bigtop_assets(void)
{
	load_btop_gore_texmaps();
	load_pood_texmap();
	load_mame_texmap();
	load_batb_texmap();
	load_clown_texmap();
	load_litl_texmap();
	load_doctorklot_texmap();
	load_skeleton_texmap();
	load_btop_weap_texmaps();
	load_pood();
	load_mame();
	load_batb();
	load_clown();
	load_litl();
	load_doctor_klot();
	load_binn();
	load_skeleton();
	load_btop_weap_models();
}

/* 0x800f6cd8, 0x800f6d38 */
void remove_rickitytown_assets(void)
{
	remove_junk();
	remove_rinn_assets();
	remove_term_assets();
	remove_dino_assets();
	remove_teen_assets();
	remove_krampus_assets();
	remove_krinkle_assets();
	remove_smilingbob_assets();
	remove_rickity_texmap();
}

void load_rickity_town_assets(void)
{
	load_rickity_texmap();
	load_smilingbob_texmap();
	load_krinkle_texmap();
	load_krampus_texmap();
	load_teen_texmap();
	load_dino_texmap();
	load_term_texmap();
	load_rickity_object_texmaps();
	load_smilingbob();
	load_krinkle();
	load_krampus();
	load_teen();
	load_dino();
	load_term();
	load_rinn();
	load_junk();
}

/* 0x800f6dd0-0x800f6e50: the attract loop's and the gallery's assets */
void remove_attract_assets(void)
{
	remove_attract_texmap();
	remove_hstd_texmap();
	remove_gallery_objects();
}

void load_attract_assets(void)
{
	load_gallery_objects();
	load_hstd_texmap();
	load_attract_texmap();
}

void remove_gallery_assets(void)
{
	remove_gallery_objects();
}

void load_gallery_objects_wrapper(void)
{
	load_gallery_objects();
}

/* 0x800f6e70: start the big top's mid-level reload */
void start_btopload(void)
{
	qcreate("btopload", 0x800, btopload_proc, 0, 0, 0, 0);
}

/* 0x800f6eb0: the "btopload" process: with the movie paused, swap act one's enemies and
   banks for the finale's and switch the music stream */
void btopload_proc(void)
{
	disable_guns();
	free_chunks();
	free_screen_sprites();
	free_world_sprites();
	movie_pause();
	while(movie.flags & MOVIE_READING)
	{
		sleep(1);
	}
	wave_busy = 1;
	clean_up_level_sounds();
	snd_delay(20);
	snd_scall_bank("tokken", 0x24, 0xff, 0x80, 0x8181);
	install_disk_info(&cur_proc, suspend_self, resume_process);
	ide_set_partition(1);
	func_8013c2a4();
	remove_litl_assets();
	remove_clown_assets();
	remove_batb_texmap();
	remove_mame_assets();
	func_80131330();
	bigtop_act2 = 1;
	snd_bank_delete("ozob");
	snd_bank_delete("pood");
	snd_bank_delete("klott");
	snd_bank_delete("smeek");
	snd_bank_delete("mortito");
	snd_bank_delete("bigtop");
	snd_bank_load_playing("tokken2");
	snd_bank_load_playing("bigtop2");
	snd_bank_load_playing("jesbig2");
	snd_bank_load_playing("ending");
	load_toke_texmap();
	load_toke();
	load_jester_path();
	sleep(3);
	wave_busy = 0;
	install_disk_info(0, 0, 0);
	movie_resume();
	enable_guns();
	stream_select_wave(WAVE_BIGTOP_LOAD);
	sleep(3);
	stream_start();
}
