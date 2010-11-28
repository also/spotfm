#include "sx.h"
#include "sx_server.h"
#include "sx_spotify.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "md5.h"

#include "http_parser.h"

#define CRLF "\r\n"

#define CURRENT_HEADER (&c->headers[c->header_count - 1])

void* run_client(void *sp);

int on_client_path (http_parser *parser, const char *p, size_t len);
int on_client_header_field(http_parser *parser, const char *p, size_t len);
int on_client_header_value(http_parser *parser, const char *p, size_t len);
int on_client_headers_complete(http_parser *parser);

static void handle_client_request(sx_client *c, char *body, size_t body_len);
static char *get_header(sx_client *c, char *name);

static char *get_header(sx_client *c, char *name) {
	for (int i = 0; i < c->header_count; i++) {
		if (!strcmp(name, c->headers[i].name)) {
			return c->headers[i].value;
		}
	}
	return NULL;
}

static void send_client(sx_client *c, char *data) {
	send(c->fd, data, strlen(data), 0);
}

void* sx_server_loop(void *s) {
	pthread_setname_np("sx_server main");
	sx_session *session = (sx_session *) s;
	int sockfd, newsockfd, err;
	socklen_t clilen;
	struct sockaddr_in cli_addr, serv_addr;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		sx_log(session, "server: can't open stream socket");
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
		sx_log(session, "server: can't bind local address %d", err);
		return NULL;
	}

	listen(sockfd, 5);
	for ( ; ; ) {
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0) {
			sx_log(session, "server: accept error");
			close(sockfd);
			return NULL;
		}

		sx_client *c = (sx_client *) malloc(sizeof(sx_client));
		bzero(c, sizeof(sx_client));
		c->session = session;
		c->headers_complete = false;
		c->fd = newsockfd;

		pthread_t client_thread;
		pthread_create(&client_thread, NULL, run_client, c);
	}

	close(sockfd);
}

void* run_client(void *x) {
	pthread_setname_np("sx_server client");
	sx_client *c = (sx_client *) x;

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
	
	free(parser);

	if (!c->async) {
		sx_server_free_client(c);
	}

	return NULL;
}

void sx_server_free_client(sx_client *c) {
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
}

int on_client_path(http_parser *parser, const char *p, size_t len) {
	sx_client *c = (sx_client *) parser->data;

	c->path = malloc(len);
	c->path[len - 1] = 0;
	memcpy(c->path, p + 1, len - 1);
	sx_log(c->session, "client: \"%s\"", c->path);
	return 0;
}

int on_client_header_field(http_parser *parser, const char *p, size_t len) {
	sx_client *c = (sx_client *) parser->data;

	if (c->header_count == 0 || CURRENT_HEADER->value_len > 0) {
		// this is the first chunk of the name
		c->header_count++;
		if (c->header_count > MAX_HEADERS) {
			c->header_count = MAX_HEADERS;
			sx_log(c->session, "too many headers");
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
	sx_client *c = (sx_client *) parser->data;

	size_t current_len = CURRENT_HEADER->value_len;
	CURRENT_HEADER->value_len += len;
	CURRENT_HEADER->value = realloc(CURRENT_HEADER->value, CURRENT_HEADER->value_len + 1);
	memcpy(CURRENT_HEADER->value + current_len, p, len);
	CURRENT_HEADER->value[CURRENT_HEADER->value_len] = '\0';

	return 0;
}

int on_client_headers_complete(http_parser *parser) {
	sx_client *c = (sx_client *) parser->data;
	c->headers_complete = true;
	return 0;
}

static int handle_ws_message(ws_client *c, const char *at, size_t length) {
	sx_client *sxc = (sx_client *) c->data;
	sx_log(sxc->session, "got message. \"%s\" (%ld)", at, length);
	return 0;
}

static void handle_client_request(sx_client *c, char *body, size_t body_len) {
	char *not_found_response =
		"HTTP/1.0 404 Not Found" CRLF
		"Access-Control-Allow-Origin: *" CRLF
		CRLF
		"false\n";
	char *ok_response =
		"HTTP/1.0 200 OK" CRLF
		"Access-Control-Allow-Origin: *" CRLF
		CRLF
		"true\n";

	bool ok = false;
	size_t len = strlen(c->path);

	if ((len == 27 || len == 41 || len == 57) && strstr(c->path, "play/") == c->path) {
		sx_play(c->session, c->path + len - 22);
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
	else if (!strcmp("current_album_cover", c->path)) {
		sx_spotify_image *image = sx_spotify_get_current_album_cover(c->session);
		if (image) {
			send_client(c, "HTTP/1.0 200 OK" CRLF);
			send_client(c, "Content-Type: image/jpeg" CRLF CRLF);
			send(c->fd, image->data, image->size, 0);
			sx_spotify_free_image(c->session, image);
			return;
		}
	}
	else if (strstr(c->path, "track_album_cover/")) {
		sp_track *track = sx_spotify_track_for_url(c->session, c->path + 18);
		if (track) {
			sp_album *album = sx_spotify_load_album_for_track_sync(c->session, track, 1000);
			if (album) {
				sx_spotify_image *image = sx_spotify_get_album_cover(c->session, album);
				if (image) {
					send_client(c, "HTTP/1.0 200 OK" CRLF);
					send_client(c, "Content-Type: image/jpeg" CRLF CRLF);
					send(c->fd, image->data, image->size, 0);
					sx_spotify_free_image(c->session, image);
					// FIXME lock
					sp_track_release(track);
					return;
				}
			}
		}
	}
	else if (!strcmp("monitor", c->path)) {
		ws_client ws_client;
		ws_client.data = c;
		ws_client.fd = c->fd;
		ws_client.callback = handle_ws_message;
		
		ws_client.host = get_header(c, "Host");
		ws_client.origin = get_header(c, "Origin");
		ws_client.path = "/monitor";

		ws_client.key1 = get_header(c, "Sec-WebSocket-Key1");
		ws_client.key2 = get_header(c, "Sec-WebSocket-Key2");
		char key3[8];
		ws_client.key3 = key3;

		if (body_len >= 8) {
			memcpy(key3, body, 8);
		}
		else {
			memcpy(key3, body, body_len);
			// TODO just pretending this will always work
			recv(c->fd, key3 + body_len, 8 - body_len, 0);
		}

		if (ws_client.key1 && ws_client.key2) {
			c->ws_client = &ws_client;

			ws_send_handshake(&ws_client);

			sx_monitor(c);
			// TODO if body_len < 8
			ws_run(&ws_client, c->fd, body + 8, body_len - 8);
			sx_monitor_end(c);
			return;
		}
	}

	if (!ok) {
		sx_log(c->session, "invalid url: %s (%d)", c->path, len);
		send_client(c, not_found_response);
		close(c->fd);
		c->fd = -1;
	}

	send_client(c, ok_response);
	close(c->fd);
	c->fd = -1;
}
