/* SDL2 window with a legacy (compatibility) OpenGL context, for Linux and other non-Apple hosts.
   The keys and the mouse-as-gun mapping are the same as the macOS backend (video_macos.m). */
#include <stdio.h>
#define SDL_MAIN_HANDLED	/* the port's own main(), not SDL's WinMain wrapper */
#include <SDL.h>
#include "video.h"
#include "hal.h"
#include "mice.h"

static SDL_Window	*win;
SDL_Window *video_sdl_window(void) { return win; }
static SDL_GLContext	ctx;
static int		closed;
int			video_fullscreen, video_borderless;
static SDL_GameController *pad[2];	/* the first two controllers: players 1 and 2 */

static void pads_open(void)
{
	int	i, n = SDL_NumJoysticks(), slot;
	for(i = 0; i < n; i++)
	{
		SDL_JoystickID id;
		if(!SDL_IsGameController(i)) continue;
		id = SDL_JoystickGetDeviceInstanceID(i);
		if((pad[0] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad[0])) == id) ||
		   (pad[1] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad[1])) == id)) continue;
		slot = !pad[0] ? 0 : !pad[1] ? 1 : -1;
		if(slot < 0) return;
		pad[slot] = SDL_GameControllerOpen(i);
		if(pad[slot]) fprintf(stderr, "[video] gamepad %d: %s (player %d)\n", slot + 1, SDL_GameControllerName(pad[slot]), slot + 1);
	}
}
static void pads_close_removed(SDL_JoystickID id)
{
	int	i;
	for(i = 0; i < 2; i++)
		if(pad[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad[i])) == id)
		{
			SDL_GameControllerClose(pad[i]); pad[i] = 0;
			hal_pad(i, 0, 0, 0);
			fprintf(stderr, "[video] gamepad %d disconnected\n", i + 1);
		}
}
static void pads_poll(void)
{
	int	i;
	for(i = 0; i < 2; i++)
	{
		SDL_GameController *c = pad[i];
		float		lx, ly;
		unsigned int	b = 0;
		if(!c) continue;
		lx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
		ly = -SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;	/* up is positive */
		if(SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) lx = -1.0f;
		if(SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) lx = 1.0f;
		if(SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP)) ly = 1.0f;
		if(SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) ly = -1.0f;
		if(SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A) || SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) b |= PAD_TRIGGER;
		if(SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B) || SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) b |= PAD_PUMP;
		if(SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_Y) || SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) b |= PAD_RELOAD;
		if(SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START)) b |= PAD_START;
		if(SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_BACK)) b |= PAD_COIN;
		hal_pad(i, lx, ly, b);
	}
}

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
	SDL_SetMainReady();
	if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) fprintf(stderr, "[video] no gamepad support: %s\n", SDL_GetError());
	if(SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "[video] SDL: %s\n", SDL_GetError()); return 0; }
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
		SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE |
		(video_borderless ? SDL_WINDOW_BORDERLESS : 0) | (video_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
	if(!win) { fprintf(stderr, "[video] SDL: %s\n", SDL_GetError()); return 0; }
	ctx = SDL_GL_CreateContext(win);
	if(!ctx) { fprintf(stderr, "[video] SDL: %s\n", SDL_GetError()); SDL_DestroyWindow(win); win = 0; return 0; }
	SDL_GL_MakeCurrent(win, ctx);
	SDL_GL_SetSwapInterval(0);		/* the HAL paces frames itself */
	if(port_crosshair) SDL_ShowCursor(SDL_DISABLE);	/* the port draws the crosshair instead */
	if(port_mice)
	{
		if(mice_open()) fprintf(stderr, "[mice] reading each mouse and light gun on its own: the first to click is player 1, the next player 2\n");
		else { fprintf(stderr, "[mice] per-device input is not available here (X11 with XInput2, or Windows); --mice is off\n"); port_mice = 0; }
	}
#ifdef _WIN32
	{
		extern int port_gl_load(void *(*)(const char *));
		if(port_gl_load(SDL_GL_GetProcAddress)) fprintf(stderr, "[video] the OpenGL driver lacks calls the renderer needs\n");
	}
#endif
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
		case SDL_CONTROLLERDEVICEADDED: pads_open(); break;
		case SDL_CONTROLLERDEVICEREMOVED: pads_close_removed(ev.cdevice.which); break;
		case SDL_KEYDOWN: case SDL_KEYUP:
			/* F11 or Alt+Enter: fullscreen on the desktop's mode, or back to the window */
			if(ev.type == SDL_KEYDOWN && !ev.key.repeat && (ev.key.keysym.sym == SDLK_F11 ||
			   ((ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_KP_ENTER) && (ev.key.keysym.mod & KMOD_ALT))))
			{
				video_fullscreen = !(SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP);
				SDL_SetWindowFullscreen(win, video_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
				break;
			}
			if(!ev.key.repeat)
			{
				const char *k = key_name(ev.key.keysym.sym);
				if(k) hal_key(k, ev.type == SDL_KEYDOWN);
			}
			break;
		case SDL_MOUSEMOTION: mouse_to_gun(ev.motion.x, ev.motion.y); break;
		case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
			if(ev.button.button == SDL_BUTTON_LEFT && !port_mice)	/* --mice: each device is read on its own */
			{
				mouse_to_gun(ev.button.x, ev.button.y);
				hal_key("trigger", ev.type == SDL_MOUSEBUTTONDOWN);
			}
			break;
		}
	}
	pads_poll();
	if(port_mice) mice_poll();
}

int video_closed(void) { return closed; }

void video_size(int *w, int *h)
{
	if(!win) { *w = *h = 0; return; }
	SDL_GL_GetDrawableSize(win, w, h);
}
