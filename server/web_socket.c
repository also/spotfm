#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include "md5.h"

#include "web_socket.h"

#define CRLF "\r\n"

#define WS_MESSAGE_START '\0'
#define WS_MESSAGE_END '\xff'

void ws_send(ws_client *c, const char *data, size_t len) {
	char b = '\0';
	send(c->fd, &b, 1, 0);
	send(c->fd, data, len, 0);
	b = '\xff';
	send(c->fd, &b, 1, 0);
}

static void send_client(ws_client *c, char *data) {
	send(c->fd, data, strlen(data), 0);
}

uint32_t ws_parse_key(const char *key) {
	size_t len = strlen(key);

	char nums[len + 1];
	int num_count = 0;
	int space_count = 0;

	for (int i = 0; i < len; i++) {
		char c = key[i];
		if (c >= '0' && c <= '9') {
			nums[num_count++] = c;
		}
		else if (c == ' ') {
			space_count++;
		}
	}

	nums[num_count] = '\0';
	uint32_t n = strtoul(nums, NULL, 10);
	uint32_t result = n / space_count;
	return result;
}

void ws_generate_signature(const char *key1, const char *key2, const char *key3, char *buf) {
	// oh god this is stupid
	uint32_t num1 = htonl(ws_parse_key(key1));
	uint32_t num2 = htonl(ws_parse_key(key2));

	MD5_CTX mdContext;
	MD5Init(&mdContext);
	MD5Update(&mdContext, &num1, 4);
	MD5Update(&mdContext, &num2, 4);
	MD5Update(&mdContext, key3, 8);
	MD5Final(&mdContext);

	memcpy(buf, mdContext.digest, 16);
}

void ws_send_handshake(ws_client *c) {
	char signature[16];
	ws_generate_signature(c->key1, c->key2, c->key3, signature);
	
	char *status = "HTTP/1.1 101 WebSocket Protocol Handshake" CRLF
	"Upgrade: WebSocket" CRLF
	"Connection: Upgrade" CRLF
	"Sec-WebSocket-Origin: ";
	send_client(c, status);
	send_client(c, c->origin);
	send_client(c, CRLF "Sec-WebSocket-Location: ws://");
	send_client(c, c->host);
	send_client(c, c->path);
	send_client(c, CRLF CRLF);
	send(c->fd, signature, 16, 0);
}

static int ws_parse_input(ws_client *c, const char *input, size_t input_len) {
	const char *current, *end;
	current = input;
	end = current + input_len;
	int current_len = 0;

	if (input_len == 0) {
		// called with empty string
		return 0;
	}

	while (current < end) {
		if (!c->in_message) {
			// looking for a message start
			if (current[0] != WS_MESSAGE_START) {
				// invalid message start
				return -1;
			}
			// got message start
			c->in_message = true;
			current++;
		}
		while (current + current_len < end) {
			if (current[current_len] == WS_MESSAGE_END) {
				c->message = realloc(c->message, c->message_len + current_len + 1);
				memcpy(c->message + c->message_len, current, current_len);
				c->message_len += current_len;
				c->message[c->message_len] = '\0';
				c->callback(c, c->message, c->message_len);
				free(c->message);
				c->message = NULL;
				c->message_len = 0;
				current += current_len + 1;
				current_len = 0;
				c->in_message = false;
				break;
			}
			current_len++;
		}
		if (current_len > 0) {
			c->message = realloc(c->message, c->message_len + current_len + 1);
			memcpy(c->message + c->message_len, current, current_len);
			c->message_len += current_len;
			c->message[c->message_len] = '\0';
			break;
		}
	}
	return 0;
}

void ws_run(ws_client *c, int fd, const char *data, size_t data_len) {
	size_t len = 80*1024;
	char buf[len];
	ssize_t recved;

	c->message = NULL;
	c->message_len = 0;
	c->in_message = false;

	if (data_len > 0) {
		ws_parse_input(c, data, data_len);
	}

	do {
		recved = recv(fd, buf, len, 0);
		if (recved < 0) {
			/*  TODO Handle error. */
			break;
		}

		ws_parse_input(c, buf, recved);
	} while (recved > 0);
}
