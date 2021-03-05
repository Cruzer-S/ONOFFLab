#ifndef SERVER_HANDLER_H__
#define SERVER_HANDLER_H__

#include <sys/socket.h>		// socket
#include <sys/unistd.h>		// close
#include <sys/epoll.h>		// epoll
#include <sys/time.h>		// struct timeval
#include <string.h>			// memset
#include <arpa/inet.h>		// htonl, htons
#include <fcntl.h>			// fcntl

#define MAX_EVENT 1024
#define HEADER_SIZE 1024

#define EXTRACT(ptr, value) memcpy(&value, ptr, sizeof(value)), ptr += sizeof(value);

enum IPC_COMMAND {
	IPC_REGISTER_DEVICE = 0x01,
};

int make_server(short port, int backlog);

int change_flag(int fd, int flag);
int change_sockopt(int fd, int level, int flag, int value);

int register_epoll_fd(int epfd, int tgfd, int flag);
struct epoll_event *wait_epoll_event(int epfd, int maxevent, int timeout);
int delete_epoll_fd(int epfd, int tgfd);

#endif
