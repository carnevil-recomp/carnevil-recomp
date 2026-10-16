/* Cocoa window with a legacy OpenGL 2.1 context. Apple's frameworks are universal, so this
   links for the x86_64 port binary without any third-party library. */
#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <GameController/GameController.h>
#include "video.h"
#include "hal.h"

static const char *key_name(NSEvent *ev)
{
	NSString *s = [ev charactersIgnoringModifiers];
	unichar c = [s length] ? [s characterAtIndex:0] : 0;
	switch(c)
	{
	case '5': case 'c': case 'C': return "coin";
	case '1': return "start1";
	case '2': return "start2";
	case '9': return "service";
	case '-': return "voldown";
	case '=': return "volup";
	case ' ': return "pump";
	case NSF2FunctionKey: return "test";
	default: return 0;
	}
}

static NSWindow		*win;
static NSOpenGLContext	*ctx;
static NSOpenGLView	*view;
static int		closed;
int			video_fullscreen, video_borderless;

/* a borderless window has to be told it may take the keyboard */
@interface PortWindow : NSWindow
@end
@implementation PortWindow
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end

@interface PortDelegate : NSObject <NSWindowDelegate, NSApplicationDelegate>
@end
@implementation PortDelegate
- (BOOL)windowShouldClose:(id)sender { (void)sender; closed = 1; return YES; }
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)s { (void)s; return YES; }
@end

int video_open(int width, int height, const char *title)
{
	@autoreleasepool {
		NSOpenGLPixelFormatAttribute attrs[] = { NSOpenGLPFADoubleBuffer, NSOpenGLPFADepthSize, 24, NSOpenGLPFAColorSize, 24, 0 };
		NSOpenGLPixelFormat *pf;
		PortDelegate *del = [PortDelegate new];
		[NSApplication sharedApplication];
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
		[NSApp setDelegate:del];
		win = [[PortWindow alloc] initWithContentRect:NSMakeRect(100, 100, width, height)
			styleMask:(video_borderless ? NSWindowStyleMaskBorderless :
				NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
			backing:NSBackingStoreBuffered defer:NO];
		[win setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];	/* the green button, Ctrl+Cmd+F */
		[win center];
		[win setTitle:[NSString stringWithUTF8String:title]];
		[win setDelegate:del];
		pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
		if(!pf) return 0;
		view = [[NSOpenGLView alloc] initWithFrame:NSMakeRect(0, 0, width, height) pixelFormat:pf];
		[win setContentView:view];
		ctx = [view openGLContext];
		[ctx makeCurrentContext];
		[win makeKeyAndOrderFront:nil];
		[NSApp activateIgnoringOtherApps:YES];
		[NSApp finishLaunching];
		[win setAcceptsMouseMovedEvents:YES];
		if(video_fullscreen) [win toggleFullScreen:nil];
	}
	return 1;
}

extern int video_to_game(float wx, float wy, float *gx, float *gy);
static void mouse_to_gun(NSEvent *ev)
{
	NSPoint p = [view convertPoint:[ev locationInWindow] fromView:nil];
	NSPoint b = [view convertPointToBacking:p];
	float gx, gy;
	int on = video_to_game((float)b.x, (float)b.y, &gx, &gy);
	hal_mouse(gx, gy, on);
	/* the port draws the crosshair, so the system pointer goes while it is over the picture */
	{
		static int hidden;
		int want = on && port_crosshair;
		if(want != hidden) { if(want) [NSCursor hide]; else [NSCursor unhide]; hidden = want; }
	}
}

/* gamepads through Apple's GameController framework (the first two extended gamepads are players
   1 and 2; same buttons as the SDL backend: left stick or d-pad aim, A or right trigger fires,
   B or left trigger pumps, Y or right shoulder reloads, Menu starts, Options inserts a coin) */
static void pads_poll(void)
{
	NSArray<GCController *> *cs = [GCController controllers];
	static int had[2];
	int	i, n = 0;
	for(i = 0; i < (int)[cs count] && n < 2; i++)
	{
		GCExtendedGamepad *g = cs[i].extendedGamepad;
		float		lx, ly;
		unsigned int	b = 0;
		if(!g) continue;
		lx = g.leftThumbstick.xAxis.value; ly = g.leftThumbstick.yAxis.value;
		if(g.dpad.left.pressed) lx = -1.0f;
		if(g.dpad.right.pressed) lx = 1.0f;
		if(g.dpad.up.pressed) ly = 1.0f;
		if(g.dpad.down.pressed) ly = -1.0f;
		if(g.buttonA.pressed || g.rightTrigger.value > 0.5f) b |= PAD_TRIGGER;
		if(g.buttonB.pressed || g.leftTrigger.value > 0.5f) b |= PAD_PUMP;
		if(g.buttonY.pressed || g.rightShoulder.pressed) b |= PAD_RELOAD;
		if(g.buttonMenu.pressed) b |= PAD_START;
		if(g.buttonOptions && g.buttonOptions.pressed) b |= PAD_COIN;
		hal_pad(n, lx, ly, b);
		had[n++] = 1;
	}
	for(; n < 2; n++) if(had[n]) { hal_pad(n, 0, 0, 0); had[n] = 0; }	/* unplugged: let go */
}

void video_present(void)
{
	pads_poll();
	@autoreleasepool {
		NSEvent *ev;
		[ctx flushBuffer];
		while((ev = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES]))
		{
			NSEventType t = [ev type];
			if(t == NSEventTypeKeyDown && ![ev isARepeat] && [[ev charactersIgnoringModifiers] length] &&
			   (([ev modifierFlags] & NSEventModifierFlagOption && [[ev charactersIgnoringModifiers] characterAtIndex:0] == '\r') ||
			    (([ev modifierFlags] & (NSEventModifierFlagControl | NSEventModifierFlagCommand)) == (NSEventModifierFlagControl | NSEventModifierFlagCommand) &&
			     [[ev charactersIgnoringModifiers] characterAtIndex:0] == 'f')))
			{
				[win toggleFullScreen:nil];	/* Option+Return or Ctrl+Cmd+F */
				continue;
			}
			if((t == NSEventTypeKeyDown || t == NSEventTypeKeyUp) && ![ev isARepeat])
			{
				const char *k = key_name(ev);
				if(k) { hal_key(k, t == NSEventTypeKeyDown); continue; }
			}
			else if(t == NSEventTypeFlagsChanged) { hal_key("trigger", ([ev modifierFlags] & NSEventModifierFlagControl) != 0); continue; }
			else if(t == NSEventTypeLeftMouseDown || t == NSEventTypeLeftMouseUp) { mouse_to_gun(ev); hal_key("trigger", t == NSEventTypeLeftMouseDown); continue; }
			else if(t == NSEventTypeMouseMoved || t == NSEventTypeLeftMouseDragged) { mouse_to_gun(ev); continue; }
			[NSApp sendEvent:ev];
		}
		[ctx makeCurrentContext];
	}
}

int video_closed(void) { return closed; }

void video_size(int *w, int *h)
{
	NSRect r = [view convertRectToBacking:[view bounds]];
	*w = (int)r.size.width; *h = (int)r.size.height;
}
