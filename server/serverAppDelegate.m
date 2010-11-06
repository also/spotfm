#import "serverAppDelegate.h"

#import <IOKit/hidsystem/ev_keymap.h>

@implementation serverAppDelegate

@synthesize window;
@synthesize statusMenu;
@synthesize webView;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	char *serviceName = "Spotify";
	SecKeychainItemRef keychainItem;
	OSStatus result = SecKeychainFindGenericPassword(NULL, strlen(serviceName), serviceName, 0, NULL, NULL, NULL, &keychainItem);
	if (result) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"Spotify Password Not Found" defaultButton:nil alternateButton:nil otherButton:nil informativeTextWithFormat:@"I am lazy. Please log in with Spotify and save your password."];
		[alert runModal];
		[NSApp terminate:self];
		return;
	}

	SecKeychainAttribute attributes[1];
	SecKeychainAttributeList list;
	char *outData;

	attributes[0].tag = kSecAccountItemAttr;

	list.count = 1;
	list.attr = attributes;
	UInt32 length;

	result = SecKeychainItemCopyContent(keychainItem, NULL, &list, &length, (void **)&outData);
	if (result || ! outData) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"Error" defaultButton:nil alternateButton:nil otherButton:nil informativeTextWithFormat:@"Failed getting password."];
		[alert runModal];
		[NSApp terminate:self];
		return;
	}
	
	char *username = malloc(attributes[0].length + 1);
	strncpy(username, attributes[0].data, attributes[0].length);
	username[attributes[0].length] = '\0';

	char *password = malloc(length + 1);
	strncpy(password, outData, length);
	password[length] = '\0';

	SecKeychainItemFreeContent(&list, outData);

	[webView setUIDelegate:self];
	[webView setEditingDelegate:self];
	
	NSString* controlsFile = [[NSBundle mainBundle] pathForResource:@"controls" ofType:@"html"];
	[[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:[NSURL fileURLWithPath:controlsFile]]];

	sxxxxxxx_init(&session, username, password);
	free(username);
	free(password);
	[self showStatusItem];
	sxxxxxxx_run(session, TRUE);
}

- (void) showStatusItem {
	NSStatusBar *bar = [NSStatusBar systemStatusBar];

    NSStatusItem *statusItem = [bar statusItemWithLength:NSVariableStatusItemLength];
    [statusItem retain];

    [statusItem setTitle: NSLocalizedString(@"•㎙",@"")];
    [statusItem setHighlightMode:YES];
	[statusItem setMenu:statusMenu];
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
			NSLog(@"keyboard play");
			sxxxxxxx_toggle_play(session);
			break;
			
		case NX_KEYTYPE_FAST:
			NSLog(@"keyboard advance");
			break;
			
		case NX_KEYTYPE_REWIND:
			sxxxxxxx_previous(session);
			NSLog(@"keyboard rewind");
			break;
	}
}

@end
