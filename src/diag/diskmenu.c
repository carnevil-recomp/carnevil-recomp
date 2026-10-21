/*
 * diskmenu.c - the DISK TESTS menu (data only: 0x80161768-0x80161968, its names in rodata
 * 0x80152e10-). The tests are in ide.c, fscheck.c and integrity.c.
 */
#include "diag.h"

extern int	ide_disk_info(void), ide_sequential_read_test(void), ide_cache_read_test(void);
extern int	ide_random_read_test(void), ide_random_cache_read_test(void), ide_surface_scan(void);
extern int	disk_data_integrity(void), file_system_check(void);

char	help_disk_info[] = "INFORMATION ABOUT THE IDE DISK DRIVE";
char	help_seq_read[] = "SEQUENTIAL SECTOR READS W/O CACHE";
char	help_seq_cache_read[] = "SEQUENTIAL SECTOR READS WITH CACHE";
char	help_random_read[] = "RANDOM SECTOR READS W/O CACHE";
char	help_random_cache_read[] = "RANDOM SECTOR READS WITH CACHE";
char	help_fs_check[] = "CHECK INTEGRITY OF FILES ON DISK";
char	help_data_integrity[] = "DISK DATA INTEGRITY TEST";
char	help_surface_scan[] = "DISK SURFACE SCAN TEST";
char	help_disk_exit[] = "RETURN TO PREVIOUS MENU";

menu_item_t	disk_items[] = {
	{ "DISK INFORMATION", MENU_FUNC, ide_disk_info, help_disk_info, 0 },
	{ "SEQUENTIAL READ", MENU_FUNC, ide_sequential_read_test, help_seq_read, 0 },
	{ "SEQUENTIAL CACHE READ", MENU_FUNC, ide_cache_read_test, help_seq_cache_read, 0 },
	{ "RANDOM READ", MENU_FUNC, ide_random_read_test, help_random_read, 0 },
	{ "RANDOM CACHE READ", MENU_FUNC, ide_random_cache_read_test, help_random_cache_read, 0 },
	{ "DATA INTEGRITY TEST", MENU_FUNC, disk_data_integrity, help_data_integrity, 0 },
	{ "FILE SYSTEM CHECK", MENU_FUNC, file_system_check, help_fs_check, 0 },
	{ "SURFACE SCAN", MENU_FUNC, ide_surface_scan, help_surface_scan, 0 },
	{ "EXIT", MENU_EXIT, 0, help_disk_exit, 0 },
	{ 0, -1, 0, 0, 0 }
};

menu_t	disk_menu = { "DISK TESTS", -1.0F, -1.0F, 0, 0, disk_items };
