/* headless video: no window, draws go nowhere (used by --headless and on hosts without a backend) */
#include "video.h"
int  video_fullscreen, video_borderless;
int  video_open(int w, int h, const char *t) { (void)w; (void)h; (void)t; return 0; }
void video_present(void) { }
int  video_closed(void) { return 0; }
void video_size(int *w, int *h) { *w = 0; *h = 0; }
