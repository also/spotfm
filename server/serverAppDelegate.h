#import <Cocoa/Cocoa.h>

#import <WebKit/WebKit.h>

#import "sxxxxxxx.h"

@interface serverAppDelegate : NSObject <NSApplicationDelegate> {
    NSWindow *window;
	NSMenu *statusMenu;
	WebView *webView;
	sxxxxxxx_session *session;
}

@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet NSMenu *statusMenu;
@property (assign) IBOutlet WebView *webView;

- (void) showStatusItem;

- (IBAction) showControls:(id)sender;
- (IBAction) stop:(id)sender;
- (IBAction) resume:(id)sender;
- (IBAction) quit:(id)sender;
- (void) mediaKeyEvent:(int)key state:(BOOL)state repeat:(BOOL)repeat;

@end
