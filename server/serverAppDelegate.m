#import "serverAppDelegate.h"

@implementation serverAppDelegate

@synthesize window;
@synthesize statusMenu;

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

	sxxxxxxx_init(&session, username, password);
	[self showStatusItem];
	sxxxxxxx_run(session, TRUE);

	SecKeychainItemFreeContent(&list, outData);
}

- (void) showStatusItem {
	NSStatusBar *bar = [NSStatusBar systemStatusBar];

    NSStatusItem *statusItem = [bar statusItemWithLength:NSVariableStatusItemLength];
    [statusItem retain];

    [statusItem setTitle: NSLocalizedString(@"•㎙",@"")];
    [statusItem setHighlightMode:YES];
	[statusItem setMenu:statusMenu];
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

@end
