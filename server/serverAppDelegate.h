#import <Cocoa/Cocoa.h>

#import "sxxxxxxx.h"

@interface serverAppDelegate : NSObject <NSApplicationDelegate> {
    NSWindow *window;
	NSMenu *statusMenu;
	sxxxxxxx_session *session;
}

@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet NSMenu *statusMenu;

- (void) showStatusItem;

- (IBAction) stop:(id)sender;
- (IBAction) resume:(id)sender;
- (IBAction) quit:(id)sender;

@end
