#import "serverAppDelegate.h"

#import "sxxxxxxx.h"

@implementation serverAppDelegate

@synthesize window;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	sxxxxxxx_session *session;
	sxxxxxxx_init(&session, USERNAME, PASSWORD);
	sxxxxxxx_run(session, TRUE);}

@end
