#import "serverAppDelegate.h"

#import <IOKit/hidsystem/ev_keymap.h>

#import "keychain.h"

@implementation serverAppDelegate

@synthesize window;
@synthesize statusMenu;
@synthesize webView;

static void _log(void *data, const char *message);

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	char *username, *password;
	
	OSStatus result = get_credentials(&username, &password);
	if (result) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"Spotify Password Not Found" defaultButton:nil alternateButton:nil otherButton:nil informativeTextWithFormat:@"I am lazy. Please log in with Spotify and save your password."];
		[alert runModal];
		[NSApp terminate:self];
		return;
	}

	[webView setUIDelegate:self];
	[webView setEditingDelegate:self];
	
	NSString* controlsFile = [[NSBundle mainBundle] pathForResource:@"controls" ofType:@"html"];
	[[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:[NSURL fileURLWithPath:controlsFile]]];

	sxxxxxxx_session_config config = {
		.log = _log
	};

	sxxxxxxx_init(&session, &config, username, password);
	free(username);
	free(password);
	[self showStatusItem];
	sxxxxxxx_run(session, TRUE);

	iconInDock = [[NSUserDefaults standardUserDefaults] boolForKey:@"ShowInDock"];
	if (iconInDock) {
		[[NSApplication sharedApplication] setActivationPolicy:NSApplicationActivationPolicyRegular];
		// http://www.cocoadev.com/index.pl?TransformProcessType
		// the menu bar for the app won't work until we switch to a different prcess, then back. or something.
		ProcessSerialNumber psnx = { 0, kNoProcess };
		GetNextProcess(&psnx);
		SetFrontProcess(&psnx);
	}
}

- (void) showStatusItem {
	NSStatusBar *bar = [NSStatusBar systemStatusBar];

    NSStatusItem *statusItem = [bar statusItemWithLength:NSVariableStatusItemLength];
    [statusItem retain];

	if ([[[NSUserDefaults standardUserDefaults] stringForKey:@"StatusItemType"] isEqualToString:@"text"]) {
		[statusItem setTitle: NSLocalizedString(@"•㎙",@"")];
	}
	else {
		NSImage *menuImage = [[NSImage alloc] initWithContentsOfFile: [[NSBundle mainBundle] pathForResource:@"status-icon" ofType:@"png"]];
		statusItem.image = menuImage;
	}
    [statusItem setHighlightMode:YES];
	[statusItem setMenu:statusMenu];
}

void _log(void *data, const char *message) {
	NSLog(@"%s", message);
}

- (IBAction) showControls:(id)sender {
	[window makeKeyAndOrderFront:sender];
	[NSApp activateIgnoringOtherApps:TRUE];
}

- (NSArray *)webView:(WebView *)sender contextMenuItemsForElement:(NSDictionary *)element defaultMenuItems:(NSArray *)defaultMenuItems {
	// disable right-click context menu
	return NULL;
}

- (BOOL)webView:(WebView *)webView shouldChangeSelectedDOMRange:(DOMRange *)currentRange toDOMRange:(DOMRange *)proposedRange affinity:(NSSelectionAffinity)selectionAffinity stillSelecting:(BOOL)flag {
	// disable text selection
	return NO;
}

- (IBAction) stop:(id)sender {
	sxxxxxxx_stop(session);
}

- (IBAction) resume:(id)sender {
	sxxxxxxx_resume(session);
}

- (IBAction) quit:(id)sender {
	[NSApp terminate:self];
}

- (void)mediaKeyEvent:(int)key state:(BOOL)state repeat:(BOOL)repeat {
	if (state) {
		return;
	}
	switch (key) {
		case NX_KEYTYPE_PLAY:
			sxxxxxxx_toggle_play(session);
			break;
			
		case NX_KEYTYPE_FAST:
			sxxxxxxx_next(session);
			break;
			
		case NX_KEYTYPE_REWIND:
			sxxxxxxx_previous(session);
			break;
	}
}

@end
