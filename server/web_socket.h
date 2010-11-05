#ifndef web_socket_h
#define web_socket_h

#include <sys/types.h>
#include <stdint.h>

typedef struct ws_client ws_client;

typedef int (*ws_message_cb) (ws_client*, const char *at, size_t length);

struct ws_client {
	void *data;
	int fd;
	ws_message_cb callback;
	char *message;
	size_t message_len;
	bool in_message;
};

uint32_t ws_parse_key(const char *key);
void ws_generate_signature(const char *key1, const char *key2, const char *key3, char *buf);
void ws_run(ws_client *parser, int fd, const char *data, size_t data_len);
void ws_send(ws_client *c, const char *data, size_t len);

#endif