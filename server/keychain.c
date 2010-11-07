#include "keychain.h"

OSStatus get_credentials(char **username, char **password) {
	char *serviceName = "Spotify";
	SecKeychainItemRef keychainItem;
	OSStatus result = SecKeychainFindGenericPassword(NULL, strlen(serviceName), serviceName, 0, NULL, NULL, NULL, &keychainItem);
	if (result) {
		return result;
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
		return result;
	}
	
	*username = malloc(attributes[0].length + 1);
	strncpy(*username, attributes[0].data, attributes[0].length);
	(*username)[attributes[0].length] = '\0';
	
	*password = malloc(length + 1);
	strncpy(*password, outData, length);
	(*password)[length] = '\0';
	
	SecKeychainItemFreeContent(&list, outData);
	
	return 0;
}
