#include "sxxxxxxx_private.h"
#include "server.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "http_parser.h"

typedef struct client {
	struct sxxxxxxx_session *session;
	int fd;
} client;

void* run_client(void *sp);

int on_client_path (http_parser *parser, const char *p, size_t len);

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
		c->session = session;
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
		
		if (nparsed != recved) {
			/* TODO Handle error. Usually just close the connection. */
		}
	} while (recved > 0);
	

	if (c->fd > 0) {
		close(c->fd);
	}
	
	free(c);
	free(parser);
	return NULL;
}

int on_client_path(http_parser *parser, const char *p, size_t len) {
	client *c = (client *) parser->data;
	
	char *path = malloc(len);
	path[len] = 0;
	memcpy(path, p + 1, len - 1);
	fprintf(stderr, "client: %s\n", path);
	
	bool ok = false;
	
	if ((len == 28 || len == 42 || len == 58) && strstr(path, "play/") == path) {
		sxxxxxxx_play(c->session, path + len - 23);
		ok = true;
	}
	
	if (!ok) {
		// TODO get id
		send(c->fd, "HTTP/1.0 404 Not Found\r\n\r\nNot Found\n", 36, 0);
		close(c->fd);
		c->fd = -1;
		return 0;
	}
	
	// TODO send response
	close(c->fd);
	c->fd = -1;
	return 0;
}
