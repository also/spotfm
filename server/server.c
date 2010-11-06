#include "sxxxxxxx_private.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "md5.h"

#include "http_parser.h"

#define CURRENT_HEADER (&c->headers[c->header_count - 1])

void* run_client(void *sp);

int on_client_path (http_parser *parser, const char *p, size_t len);
int on_client_header_field(http_parser *parser, const char *p, size_t len);
int on_client_header_value(http_parser *parser, const char *p, size_t len);
int on_client_headers_complete(http_parser *parser);

static void handle_client_request(client *c, char *body, size_t body_len);
static char *get_header(client *c, char *name);

static char *get_header(client *c, char *name) {
	for (int i = 0; i < c->header_count; i++) {
		if (!strcmp(name, c->headers[i].name)) {
			return c->headers[i].value;
		}
	}
	return NULL;
}

static void send_client(client *c, char *data) {
	send(c->fd, data, strlen(data), 0);
}

void* server_loop(void *s) {
	sxxxxxxx_session *session = (sxxxxxxx_session *) s;
	int sockfd, newsockfd, err;
	socklen_t clilen;
	struct sockaddr_in cli_addr, serv_addr;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "server: can't open stream socket\n");
		return NULL;
	}
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family      = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port        = htons(9999);
	
	int on = 1;
	setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));
	
	err = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (err < 0) {
		fprintf(stderr, "server: can't bind local address %d\n", err);
		return NULL;
	}
	
	listen(sockfd, 5);
	for ( ; ; ) {
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0) {
			fprintf(stderr, "server: accept error\n");
			close(sockfd);
			return NULL;
		}

		client *c = (client *) malloc(sizeof(client));
		bzero(c, sizeof(client));
		c->session = session;
		c->headers_complete = false;
		c->fd = newsockfd;

		pthread_t client_thread;
		pthread_create(&client_thread, NULL, run_client, c);
	}
	
	close(sockfd);
}

void* run_client(void *x) {
	client *c = (client *) x;
	
	http_parser *parser = malloc(sizeof(http_parser));
	http_parser_init(parser);
	parser->data = c;
	parser->on_path = on_client_path;
	parser->on_header_field = on_client_header_field;
	parser->on_header_value = on_client_header_value;
	parser->on_headers_complete = on_client_headers_complete;
	size_t len = 80*1024, nparsed;
	char buf[len];
	ssize_t recved;
	
	do {
		recved = recv(c->fd, buf, len, 0);
		if (recved < 0) {
			/*  TODO Handle error. */
			break;
		}
		
		/* Start up / continue the parser.
		 * Note we pass the recved==0 to http_parse_requests to signal
		 * that EOF has been recieved.
		 */
		nparsed = http_parse_requests(parser, buf, recved);
		
		if (c->headers_complete) {
			handle_client_request(c, buf + nparsed, recved - nparsed);
			break;
		}
		else if (nparsed != recved) {
			break;
		}
	} while (recved > 0);

	if (c->fd > 0) {
		close(c->fd);
	}
	
	if (c->path) {
		free(c->path);
	}
	for (int i = 0; i < c->header_count; i++) {
		if (c->headers[i].name) {
			free(c->headers[i].name);
		}

		if (c->headers[i].value) {
			free(c->headers[i].value);
		}
	}
	free(c);
	free(parser);
	
	return NULL;
}

int on_client_path(http_parser *parser, const char *p, size_t len) {
	client *c = (client *) parser->data;

	c->path = malloc(len);
	c->path[len - 1] = 0;
	memcpy(c->path, p + 1, len - 1);
	fprintf(stderr, "client: \"%s\"\n", c->path);
	return 0;
}

int on_client_header_field(http_parser *parser, const char *p, size_t len) {
	client *c = (client *) parser->data;

	if (c->header_count == 0 || CURRENT_HEADER->value_len > 0) {
		// this is the first chunk of the name
		c->header_count++;
		if (c->header_count > MAX_HEADERS) {
			c->header_count = MAX_HEADERS;
			fprintf(stderr, "too many headers\n");
			return 1;
		}
	}

	size_t current_len = CURRENT_HEADER->name_len;
	CURRENT_HEADER->name_len += len;
	CURRENT_HEADER->name = realloc(CURRENT_HEADER->name, CURRENT_HEADER->name_len + 1);
	memcpy(CURRENT_HEADER->name + current_len, p, len);
	CURRENT_HEADER->name[CURRENT_HEADER->name_len] = '\0';

	return 0;
}

int on_client_header_value(http_parser *parser, const char *p, size_t len) {
	client *c = (client *) parser->data;

	size_t current_len = CURRENT_HEADER->value_len;
	CURRENT_HEADER->value_len += len;
	CURRENT_HEADER->value = realloc(CURRENT_HEADER->value, CURRENT_HEADER->value_len + 1);
	memcpy(CURRENT_HEADER->value + current_len, p, len);
	CURRENT_HEADER->value[CURRENT_HEADER->value_len] = '\0';

	return 0;
}

int on_client_headers_complete(http_parser *parser) {
	client *c = (client *) parser->data;
	c->headers_complete = true;
	return 0;
}

static int handle_ws_message(ws_client *c, const char *at, size_t length) {
	printf("got message. \"%s\" (%ld)\n", at, length);
	return 0;
}

static void handle_client_request(client *c, char *body, size_t body_len) {
	char *not_found_response = "HTTP/1.0 404 Not Found\r\nAccess-Control-Allow-Origin: *\r\n\r\nfalse\n";
	char *ok_response = "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin: *\r\n\r\ntrue\n";
	
	bool ok = false;
	size_t len = strlen(c->path);

	if ((len == 27 || len == 41 || len == 57) && strstr(c->path, "play/") == c->path) {
		sxxxxxxx_play(c->session, c->path + len - 22);
		ok = true;
	}
	else if (!strcmp("resume", c->path)) {
		sxxxxxxx_resume(c->session);
		ok = true;
	}
	else if (!strcmp("stop", c->path)) {
		sxxxxxxx_stop(c->session);
		ok = true;
	}
	else if (!strcmp("monitor", c->path)) {
		char *key1 = get_header(c, "Sec-WebSocket-Key1");
		char *key2 = get_header(c, "Sec-WebSocket-Key2");
		char key3[9];
		char *host = get_header(c, "Host");
		char *origin = get_header(c, "Origin");

		if (body_len >= 8) {
			memcpy(key3, body, 8);
		}
		else {
			memcpy(key3, body, body_len);
			// TODO just pretending this will always work
			recv(c->fd, key3 + body_len, 8 - body_len, 0);
		}
		key3[8] = '\0';

		if (key1 && key2) {
			ok = true;

			ws_client ws_client;
			ws_client.data = c;
			ws_client.fd = c->fd;
			ws_client.callback = handle_ws_message;
			c->ws_client = &ws_client;

			// TODO broken
//			char signature[16];
//			ws_generate_signature(key1, key2, key3, signature);

			uint32_t num1 = htonl(ws_parse_key(key1));
			uint32_t num2 = htonl(ws_parse_key(key2));

			MD5_CTX mdContext;
			MD5Init(&mdContext);
			MD5Update(&mdContext, &num1, 4);
			MD5Update(&mdContext, &num2, 4);
			MD5Update(&mdContext, &key3, 8);
			MD5Final(&mdContext);

			char *status = "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
							"Upgrade: WebSocket\r\n"
							"Connection: Upgrade\r\n"
							"Sec-WebSocket-Origin: ";
			send_client(c, status);
			send_client(c, origin);
			send_client(c, "\r\nSec-WebSocket-Location: ws://");
			send_client(c, host);
			send_client(c, "/monitor\r\n\r\n");
			send(c->fd, mdContext.digest, 16, 0);

			sxxxxxxx_monitor(c);
			// TODO if body_len < 8
			ws_run(&ws_client, c->fd, body + 8, body_len - 8);
			sxxxxxxx_monitor_end(c);
			return;
		}
	}
	
	if (!ok) {
		fprintf(stderr, "invalid url: %s\n", c->path);
		send_client(c, not_found_response);
		close(c->fd);
		c->fd = -1;
	}
	
	send_client(c, ok_response);
	close(c->fd);
	c->fd = -1;
}
