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
#include <time.h>			// clock
#include <netdb.h>			// gethostbyname

#define MAX_EVENT	1024
#define CPS			CLOCKS_PER_SEC
#define BUFFER_SIZE BUFSIZ

#define PACKET_SIZE			sizeof(struct packet_header)

enum IPC_COMMAND {
	IPC_REGISTER_DEVICE = 0x01,
	IPC_REGISTER_GCODE = 0x02,
	IPC_DELETE_GCODE,
	IPC_RENAME_GCODE,
	IPC_CHANGE_QUANTITY_AND_ORDER,
};

struct packet_header {
	int32_t method;

	int32_t bsize;
	uint8_t *body;

	char fname[128];
	char rname[128];

	int32_t quantity;
	int32_t order;
};

int make_listener(short port, int backlog);
int connect_to_target(const char *host, uint16_t port);

int change_flag(int fd, int flag);
int change_sockopt(int fd, int level, int flag, int value);
int flush_socket(int sock);
int flush_socket2(int sock, clock_t clk);

int register_epoll_fd(int epfd, int tgfd, int flag);
struct epoll_event *wait_epoll_event(int epfd, struct epoll_event *events, int maxevent, int timeout);
int delete_epoll_fd(int epfd, int tgfd);
int accept_epoll_client(int epfd, int serv_sock, int flags);

int readall(int sock, char *buffer, int length);
int recv_until(int sock, char *buffer, int bsize, char *end);

int sendt(int sock, void *buffer, int size, clock_t timeout);
int recvt(int sock, void *buffer, int size, clock_t timeout);

#endif
