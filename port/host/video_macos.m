/* Cocoa window with a legacy OpenGL 2.1 context. Apple's frameworks are universal, so this
   links for the x86_64 port binary without any third-party library. */
#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#include "video.h"

static NSWindow		*win;
static NSOpenGLContext	*ctx;
static NSOpenGLView	*view;
static int		closed;

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
		win = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, width, height)
			styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable)
			backing:NSBackingStoreBuffered defer:NO];
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
	}
	return 1;
}

void video_present(void)
{
	@autoreleasepool {
		NSEvent *ev;
		[ctx flushBuffer];
		while((ev = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES]))
			[NSApp sendEvent:ev];
		[ctx makeCurrentContext];
	}
}

int video_closed(void) { return closed; }

void video_size(int *w, int *h)
{
	NSRect r = [view convertRectToBacking:[view bounds]];
	*w = (int)r.size.width; *h = (int)r.size.height;
}
