/* --mice for the SDL backend: each mouse or light gun read on its own (SDL2 merges them).
   Windows: Raw Input (WM_INPUT through SDL's message hook), one RAWINPUT per device report.
   Linux: XInput2 raw events on a connection of our own; libXi is opened at run time, so the build
   needs no X headers and a Wayland-only session simply reports that --mice is unavailable.
   The device reports go to port/runtime/mice.c, which gives devices to players. */
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include "mice.h"

extern SDL_Window *video_sdl_window(void);

/* window points -> drawable pixels with the origin at the bottom left (as mouse_to_gun does) */
static void window_to_drawable(float x, float y, float *bx, float *by)
{
	SDL_Window *w = video_sdl_window();
	int	ww, wh, dw, dh;
	SDL_GetWindowSize(w, &ww, &wh);
	SDL_GL_GetDrawableSize(w, &dw, &dh);
	*bx = ww ? x * (float)dw / ww : 0;
	*by = wh ? (wh - y) * (float)dh / wh : 0;
}

#ifdef _WIN32
#include <windows.h>

static HWND	hwnd;

static void SDLCALL raw_hook(void *user, void *h, unsigned int msg, Uint64 wp, Sint64 lp)
{
	RAWINPUT	ri;
	UINT		size = sizeof ri;
	RAWMOUSE	*m;
	uintptr_t	id;
	USHORT		f;
	(void)user; (void)h; (void)wp;
	if(msg != WM_INPUT) return;
	if(GetRawInputData((HRAWINPUT)(intptr_t)lp, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1) return;
	if(ri.header.dwType != RIM_TYPEMOUSE) return;
	m = &ri.data.mouse; id = (uintptr_t)ri.header.hDevice;
	if(m->usFlags & MOUSE_MOVE_ABSOLUTE)
	{
		/* a light gun, a DolphinBar or a tablet: 0..65535 across the (virtual) desktop */
		int	vd = (m->usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
		int	x0 = vd ? GetSystemMetrics(SM_XVIRTUALSCREEN) : 0, y0 = vd ? GetSystemMetrics(SM_YVIRTUALSCREEN) : 0;
		int	w = GetSystemMetrics(vd ? SM_CXVIRTUALSCREEN : SM_CXSCREEN), hh = GetSystemMetrics(vd ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
		POINT	p = { x0 + MulDiv(m->lLastX, w, 65535), y0 + MulDiv(m->lLastY, hh, 65535) };
		float	bx, by;
		ScreenToClient(hwnd, &p);
		window_to_drawable((float)p.x, (float)p.y, &bx, &by);
		mice_abs(id, bx, by);
	}
	else if(m->lLastX || m->lLastY) mice_rel(id, (float)m->lLastX, (float)m->lLastY);
	f = m->usButtonFlags;
	if(f & RI_MOUSE_LEFT_BUTTON_DOWN) mice_button(id, 0, 1);
	if(f & RI_MOUSE_LEFT_BUTTON_UP) mice_button(id, 0, 0);
	if(f & RI_MOUSE_RIGHT_BUTTON_DOWN) mice_button(id, 1, 1);
	if(f & RI_MOUSE_RIGHT_BUTTON_UP) mice_button(id, 1, 0);
	if(f & RI_MOUSE_MIDDLE_BUTTON_DOWN) mice_button(id, 2, 1);
	if(f & RI_MOUSE_MIDDLE_BUTTON_UP) mice_button(id, 2, 0);
}

int mice_open(void)
{
	SDL_SysWMinfo	info;
	RAWINPUTDEVICE	rid;
	SDL_VERSION(&info.version);
	if(!SDL_GetWindowWMInfo(video_sdl_window(), &info)) return 0;
	hwnd = info.info.win.window;
	rid.usUsagePage = 0x01; rid.usUsage = 0x02;	/* generic desktop: mouse */
	rid.dwFlags = 0; rid.hwndTarget = hwnd;
	if(!RegisterRawInputDevices(&rid, 1, sizeof rid)) return 0;
	SDL_SetWindowsMessageHook(raw_hook, NULL);
	return 1;
}
void mice_poll(void) { }		/* the hook runs inside SDL_PollEvent */

#else	/* X11 */
#include <dlfcn.h>

/* the parts of Xlib and XInput2 used here (a stable ABI) */
typedef unsigned long XID_t;
typedef struct { int type; unsigned long serial; int send_event; void *display; int extension; int evtype; unsigned int cookie; void *data; } XGenericEventCookie_t;
typedef union { int type; XGenericEventCookie_t xcookie; long pad[24]; } XEvent_t;
typedef struct { int deviceid; int mask_len; unsigned char *mask; } XIEventMask_t;
typedef struct { int mask_len; unsigned char *mask; double *values; } XIValuatorState_t;
typedef struct { int type; unsigned long serial; int send_event; void *display; int extension; int evtype; unsigned long time;
		 int deviceid; int sourceid; int detail; int flags; XIValuatorState_t valuators; double *raw_values; } XIRawEvent_t;
typedef struct { int type; int sourceid; } XIAnyClassInfo_t;
typedef struct { int type; int sourceid; int number; XID_t label; double min, max, value; int resolution; int mode; } XIValuatorClassInfo_t;
typedef struct { int deviceid; char *name; int use; int attachment; int enabled; int num_classes; XIAnyClassInfo_t **classes; } XIDeviceInfo_t;
#define GenericEvent		35
#define XI_RawButtonPress	15
#define XI_RawButtonRelease	16
#define XI_RawMotion		17
#define XIAllMasterDevices	1
#define XIValuatorClass		2
#define XIModeAbsolute		1

static void	*dpy, *xlib, *xi;
static int	xi_opcode;
static void	*(*pXOpenDisplay)(const char *);
static XID_t	(*pXDefaultRootWindow)(void *);
static int	(*pXPending)(void *), (*pXNextEvent)(void *, XEvent_t *), (*pXGetEventData)(void *, XGenericEventCookie_t *);
static void	(*pXFreeEventData)(void *, XGenericEventCookie_t *);
static int	(*pXQueryExtension)(void *, const char *, int *, int *, int *), (*pXFlush)(void *);
static int	(*pXDisplayWidth)(void *, int), (*pXDisplayHeight)(void *, int);
static int	(*pXIQueryVersion)(void *, int *, int *), (*pXISelectEvents)(void *, XID_t, XIEventMask_t *, int);
static XIDeviceInfo_t *(*pXIQueryDevice)(void *, int, int *);
static void	(*pXIFreeDeviceInfo)(XIDeviceInfo_t *);

/* what each source device is: absolute (a gun, a tablet) with its axis ranges, or relative */
#define MAX_SRC	64
static struct { int id, known, absolute; double min[2], max[2], last[2]; } src[MAX_SRC];

static int source(int id)
{
	int	i, n, k;
	XIDeviceInfo_t *d;
	for(i = 0; i < MAX_SRC && src[i].known; i++)
		if(src[i].id == id) return i;
	if(i == MAX_SRC) return -1;
	src[i].id = id; src[i].known = 1; src[i].absolute = 0;
	if((d = pXIQueryDevice(dpy, id, &n)) != NULL && n > 0)
	{
		for(k = 0; k < d->num_classes; k++)
		{
			XIValuatorClassInfo_t *v = (XIValuatorClassInfo_t *)d->classes[k];
			if(v->type != XIValuatorClass || v->number > 1) continue;
			if(v->mode == XIModeAbsolute) src[i].absolute = 1;
			src[i].min[v->number] = v->min; src[i].max[v->number] = v->max;
		}
		pXIFreeDeviceInfo(d);
	}
	return i;
}

int mice_open(void)
{
	XIEventMask_t	em;
	unsigned char	mask[3] = { 0, 0, 0 };
	int		ev, err, major = 2, minor = 0;
	if(!(xlib = dlopen("libX11.so.6", RTLD_NOW)) || !(xi = dlopen("libXi.so.6", RTLD_NOW))) return 0;
#define SYM(lib, p, n) if(!(*(void **)&p = dlsym(lib, n))) return 0
	SYM(xlib, pXOpenDisplay, "XOpenDisplay"); SYM(xlib, pXDefaultRootWindow, "XDefaultRootWindow");
	SYM(xlib, pXPending, "XPending"); SYM(xlib, pXNextEvent, "XNextEvent"); SYM(xlib, pXGetEventData, "XGetEventData");
	SYM(xlib, pXFreeEventData, "XFreeEventData"); SYM(xlib, pXQueryExtension, "XQueryExtension"); SYM(xlib, pXFlush, "XFlush");
	SYM(xlib, pXDisplayWidth, "XDisplayWidth"); SYM(xlib, pXDisplayHeight, "XDisplayHeight");
	SYM(xi, pXIQueryVersion, "XIQueryVersion"); SYM(xi, pXISelectEvents, "XISelectEvents");
	SYM(xi, pXIQueryDevice, "XIQueryDevice"); SYM(xi, pXIFreeDeviceInfo, "XIFreeDeviceInfo");
#undef SYM
	if(!(dpy = pXOpenDisplay(NULL))) return 0;
	if(!pXQueryExtension(dpy, "XInputExtension", &xi_opcode, &ev, &err) || pXIQueryVersion(dpy, &major, &minor) != 0) return 0;
	mask[XI_RawButtonPress >> 3] |= 1 << (XI_RawButtonPress & 7);
	mask[XI_RawButtonRelease >> 3] |= 1 << (XI_RawButtonRelease & 7);
	mask[XI_RawMotion >> 3] |= 1 << (XI_RawMotion & 7);
	em.deviceid = XIAllMasterDevices; em.mask_len = sizeof mask; em.mask = mask;
	pXISelectEvents(dpy, pXDefaultRootWindow(dpy), &em, 1);
	pXFlush(dpy);
	return 1;
}

void mice_poll(void)
{
	XEvent_t	e;
	if(!dpy) return;
	while(pXPending(dpy))
	{
		XIRawEvent_t	*r;
		int		s;
		pXNextEvent(dpy, &e);
		if(e.type != GenericEvent || e.xcookie.extension != xi_opcode || !pXGetEventData(dpy, &e.xcookie)) continue;
		r = (XIRawEvent_t *)e.xcookie.data;
		if((s = source(r->sourceid)) >= 0)
		{
			uintptr_t id = (uintptr_t)r->sourceid;
			if(e.xcookie.evtype == XI_RawMotion)
			{
				double	v[2] = { 0, 0 };
				int	have[2] = { 0, 0 }, k, n = 0;
				for(k = 0; k < 2 && k < r->valuators.mask_len * 8; k++)
					if(r->valuators.mask[k >> 3] & (1 << (k & 7))) { v[k] = r->raw_values[n++]; have[k] = 1; }
				if(src[s].absolute)
				{
					int	wx, wy;
					float	bx, by, sx, sy;
					for(k = 0; k < 2; k++) if(have[k]) src[s].last[k] = v[k];
					/* the device's range covers the screen; the window's position puts it in the window */
					sx = (float)((src[s].last[0] - src[s].min[0]) / (src[s].max[0] - src[s].min[0] + 1e-9) * pXDisplayWidth(dpy, 0));
					sy = (float)((src[s].last[1] - src[s].min[1]) / (src[s].max[1] - src[s].min[1] + 1e-9) * pXDisplayHeight(dpy, 0));
					SDL_GetWindowPosition(video_sdl_window(), &wx, &wy);
					window_to_drawable(sx - wx, sy - wy, &bx, &by);
					mice_abs(id, bx, by);
				}
				else if(have[0] || have[1]) mice_rel(id, (float)v[0], (float)v[1]);
			}
			else if(r->detail >= 1 && r->detail <= 3)	/* X buttons: 1 left, 2 middle, 3 right */
				mice_button(id, r->detail == 1 ? 0 : r->detail == 3 ? 1 : 2, e.xcookie.evtype == XI_RawButtonPress);
		}
		pXFreeEventData(dpy, &e.xcookie);
	}
}
#endif
