#ifndef server_h
#define server_h

#include "web_socket.h"

#define MAX_HEADERS 20

typedef struct header {
	char *name;
	size_t name_len;
	char *value;
	size_t value_len;
} header;

typedef struct sx_client {
	struct sx_session *session;
	void *data;
	int fd;
	bool async;
	ws_client *ws_client;
	bool headers_complete;
	char *path;
	header headers[MAX_HEADERS];
	int header_count;
} sx_client;

void* sx_server_loop(void *sp);

void sx_server_free_client(sx_client *c);

#endif
