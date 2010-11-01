#include "sxxxxxxx_private.h"
#include "server.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "http_parser.h"

#define MAX_HEADERS 20
#define CURRENT_HEADER (&c->headers[c->header_count - 1])

typedef struct header {
	char *name;
	size_t name_len;
	char *value;
	size_t value_len;
} header;

typedef struct client {
	struct sxxxxxxx_session *session;
	int fd;
	bool headers_complete;
	bool last_was_value;
	char *path;
	char *key1;
	char *key2;
	header headers[MAX_HEADERS];
	int header_count;
} client;

void* run_client(void *sp);

int on_client_path (http_parser *parser, const char *p, size_t len);
int on_client_header_field(http_parser *parser, const char *p, size_t len);
int on_client_header_value(http_parser *parser, const char *p, size_t len);
int on_client_headers_complete(http_parser *parser);

static void handle_client_request(client *c);
static char *get_header(client *c, char *name);

static char * get_header(client *c, char *name) {
	for (int i = 0; i < c->header_count; i++) {
		if (!strcmp(name, c->headers[i].name)) {
			return c->headers[i].value;
		}
	}
	return NULL;
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
			handle_client_request(c);
			fprintf(stderr, "handled!\n");
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
	for (int i; i < c->header_count; i++) {
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

static void handle_client_request(client *c) {
	char *not_found_response = "HTTP/1.0 404 Not Found\r\nAccess-Control-Allow-Origin: *\r\n\r\nfalse\n";
	char *ok_response = "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin: *\r\n\r\ntrue\n";
	
	bool ok = false;
	size_t len = strlen(c->path);
	
	if ((len == 27 || len == 41 || len == 57) && strstr(c->path, "play/") == c->path) {
		fprintf(stderr, "sxxxxxxx_play\n");
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
	
	if (!ok) {
		fprintf(stderr, "invalid url: %s\n", c->path);
		send(c->fd, not_found_response, strlen(not_found_response), 0);
		close(c->fd);
		c->fd = -1;
	}
	
	send(c->fd, ok_response, strlen(ok_response), 0);
	close(c->fd);
	c->fd = -1;
}
