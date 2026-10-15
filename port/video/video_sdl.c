/* SDL2 window with a legacy (compatibility) OpenGL context, for Linux and other non-Apple hosts.
   The keys and the mouse-as-gun mapping are the same as the macOS backend (video_macos.m). */
#include <stdio.h>
#include <SDL.h>
#include "video.h"
#include "hal.h"

static SDL_Window	*win;
static SDL_GLContext	ctx;
static int		closed;

static const char *key_name(SDL_Keycode k)
{
	switch(k)
	{
	case SDLK_5: case SDLK_c: return "coin";
	case SDLK_1: return "start1";
	case SDLK_2: return "start2";
	case SDLK_9: return "service";
	case SDLK_MINUS: return "voldown";
	case SDLK_EQUALS: return "volup";
	case SDLK_SPACE: return "pump";
	case SDLK_F2: return "test";
	case SDLK_LCTRL: case SDLK_RCTRL: return "trigger";
	default: return 0;
	}
}

int video_open(int width, int height, const char *title)
{
	if(SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "[video] SDL: %s\n", SDL_GetError()); return 0; }
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
		SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
	if(!win) { fprintf(stderr, "[video] SDL: %s\n", SDL_GetError()); return 0; }
	ctx = SDL_GL_CreateContext(win);
	if(!ctx) { fprintf(stderr, "[video] SDL: %s\n", SDL_GetError()); SDL_DestroyWindow(win); win = 0; return 0; }
	SDL_GL_MakeCurrent(win, ctx);
	SDL_GL_SetSwapInterval(0);		/* the HAL paces frames itself */
	return 1;
}

extern int video_to_game(float wx, float wy, float *gx, float *gy);
static void mouse_to_gun(int x, int y)
{
	int	ww, wh, dw, dh;
	float	gx, gy, bx, by;
	SDL_GetWindowSize(win, &ww, &wh);
	SDL_GL_GetDrawableSize(win, &dw, &dh);
	/* window points -> drawable pixels, origin at the bottom left like the GL viewport */
	bx = ww ? x * (float)dw / ww : 0;
	by = wh ? (wh - y) * (float)dh / wh : 0;
	{
		int on = video_to_game(bx, by, &gx, &gy);
		hal_mouse(gx, gy, on);
	}
}

void video_present(void)
{
	SDL_Event	ev;
	SDL_GL_SwapWindow(win);
	while(SDL_PollEvent(&ev))
	{
		switch(ev.type)
		{
		case SDL_QUIT: closed = 1; break;
		case SDL_KEYDOWN: case SDL_KEYUP:
			if(!ev.key.repeat)
			{
				const char *k = key_name(ev.key.keysym.sym);
				if(k) hal_key(k, ev.type == SDL_KEYDOWN);
			}
			break;
		case SDL_MOUSEMOTION: mouse_to_gun(ev.motion.x, ev.motion.y); break;
		case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
			if(ev.button.button == SDL_BUTTON_LEFT)
			{
				mouse_to_gun(ev.button.x, ev.button.y);
				hal_key("trigger", ev.type == SDL_MOUSEBUTTONDOWN);
			}
			break;
		}
	}
}

int video_closed(void) { return closed; }

void video_size(int *w, int *h)
{
	if(!win) { *w = *h = 0; return; }
	SDL_GL_GetDrawableSize(win, w, h);
}
