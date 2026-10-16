/* window + GL context for the port (video_macos.m / video_null.c) */
#ifndef PORT_VIDEO_H
#define PORT_VIDEO_H
int  video_open(int width, int height, const char *title);	/* 0 = no display available */
void video_present(void);					/* swap buffers, pump events */
int  video_closed(void);					/* user closed the window */
void video_size(int *width, int *height);			/* drawable size in pixels */
/* window style, set from the command line before video_open: --fullscreen fills the desktop
   (no display mode change), --borderless drops the title bar; the window is resizable and the
   picture letterboxes into it either way */
extern int video_fullscreen, video_borderless;
#endif
