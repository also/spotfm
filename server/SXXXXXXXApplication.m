#import "SXXXXXXXApplication.h"

#import <IOKit/hidsystem/ev_keymap.h>


@implementation SXXXXXXXApplication

- (void)sendEvent: (NSEvent*)event {
	if( [event type] == NSSystemDefined && [event subtype] == 8 ) {
		int keyCode = (([event data1] & 0xFFFF0000) >> 16);
		int keyFlags = ([event data1] & 0x0000FFFF);
		int keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA;
		int keyRepeat = (keyFlags & 0x1);
		
		[self mediaKeyEvent: keyCode state: keyState repeat: keyRepeat];
	}
	
	[super sendEvent: event];
}

- (void)mediaKeyEvent:(int)key state:(BOOL)state repeat:(BOOL)repeat {
	if (state) {
		return;
	}
	switch (key) {
		case NX_KEYTYPE_PLAY:
			NSLog(@"keyboard play");
			break;
			
		case NX_KEYTYPE_FAST:
			NSLog(@"keyboard advance");
			break;
			
		case NX_KEYTYPE_REWIND:
			NSLog(@"keyboard rewind");
			break;
	}
}

@end
