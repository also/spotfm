#import "SXXXXXXXApplication.h"

#import "serverAppDelegate.h"

@implementation SXXXXXXXApplication

- (void) sendEvent: (NSEvent*)event {
	// http://www.rogueamoeba.com/utm/2007/09/29/
	if ([event type] == NSSystemDefined && [event subtype] == 8) {
		int keyCode = (([event data1] & 0xFFFF0000) >> 16);
		int keyFlags = ([event data1] & 0x0000FFFF);
		int keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA;
		int keyRepeat = (keyFlags & 0x1);
		
		[((serverAppDelegate *)self.delegate) mediaKeyEvent:keyCode state:keyState repeat:keyRepeat];
	}
	
	[super sendEvent: event];
}

@end
