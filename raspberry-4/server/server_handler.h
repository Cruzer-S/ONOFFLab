#ifndef SERVER_HANDLER_H__
#define SERVER_HANDLER_H__

#include <stdio.h>			// printf
#include <stdbool.h>		// bool
#include <sys/socket.h>		// socket
#include <sys/unistd.h>		// close
#include <sys/epoll.h>		// epoll
#include <sys/time.h>		// struct timeval
#include <string.h>			// memset
#include <arpa/inet.h>		// htonl, htons
#include <fcntl.h>			// fcntl
#include <errno.h>			// errno

#define MAX_EVENT 1024
#define HEADER_SIZE (1024)

#define EXTRACT(ptr, value) memcpy(&value, ptr, sizeof(value)), ptr += sizeof(value);

enum IPC_COMMAND {
	IPC_REGISTER_DEVICE = 0x01,
};

struct http_header {
	char *method;
	char *url;
	char *version;

	struct {
		char *length;
		char *type;
		char *encoding;
	} content;

	uint8_t *EOH; // End of header
};

int make_server(short port, int backlog);

int change_flag(int fd, int flag);
int change_sockopt(int fd, int level, int flag, int value);

int register_epoll_fd(int epfd, int tgfd, int flag);
struct epoll_event *wait_epoll_event(int epfd, int maxevent, int timeout);
int delete_epoll_fd(int epfd, int tgfd);
int register_epoll_client(int epfd, int serv_sock, int flags);

int parse_http_header(char *raw, size_t size, struct http_header *header);
void show_http_header(struct http_header *header);

#endif
