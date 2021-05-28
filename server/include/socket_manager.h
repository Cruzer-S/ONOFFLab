#ifndef SOCKET_MANAGER_H__
#define SOCKET_MANAGER_H__

#ifndef _POSIX_C_SOURCE
	#define _POSIX_C_SOURCE 200112L
#elif _POSIX_C_SOURCE < 200112L
	#undef _POSIX_C_SOURCE
	#define _POSIX_C_SOURCE 200112L
#endif

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>

#define EPOLL_MAX_EVENTS 1024

enum make_listener_option {
	MAKE_LISTENER_DEFAULT       = 0x00,
	MAKE_LISTENER_BLOCKING		= 0x01,
	MAKE_LISTENER_IPV6			= 0x02,
	MAKE_LISTENER_DONTREUSE		= 0x04,
	MAKE_LISTENER_DGRAM			= 0x08,
};

struct epoll_handler;

struct socket_data {
	int fd;
	uint16_t port;
	int backlog;
	struct addrinfo *ai;
};

int socket_reuseaddr(int sock);
int change_nonblocking(int fd);


struct socket_data *socket_data_create(uint16_t port, int backlog, enum make_listener_option option);
void socket_data_destroy(struct socket_data *data);

int epoll_handler_register(struct epoll_handler *handler, int tgfd, void *ptr, int events);
int epoll_handler_unregister(struct epoll_handler *handler, int tgfd);
int epoll_handler_wait(struct epoll_handler *handler, int timeout);
struct epoll_event *epoll_handler_pop(struct epoll_handler *handler);
void epoll_handler_destroy(struct epoll_handler *handler);
struct epoll_handler *epoll_handler_create(size_t max_events);

int extract_addrinfo(struct addrinfo *ai, char *hoststr, char *portstr);

#endif
