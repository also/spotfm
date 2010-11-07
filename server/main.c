#include "keychain.h"
#include "sxxxxxxx.h"

int main() {
	char *username, *password;
	
	OSStatus result = get_credentials(&username, &password);
	if (result) {
		printf("Failed getting credentials from keychain.\n");
		return 1;
	}
	
    sxxxxxxx_session *session;
	sxxxxxxx_init(&session, username, password);
	sxxxxxxx_run(session, FALSE);
	return 0;
}
