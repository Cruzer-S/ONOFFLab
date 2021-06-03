#define _POSIX_C_SOURCE 200112L

#include "socket_manager.h"

#include "logger.h"
#include <sys/epoll.h>
#include <inttypes.h>

struct epoll_handler {
	int fd;
	int cnt;
	int max_events;

	struct epoll_event *events;
};

int socket_reuseaddr(int sock)
{
	int sockopt = 1;

	if (setsockopt(
			sock, SOL_SOCKET, SO_REUSEADDR, 
			&sockopt, sizeof(sockopt)) == -1)
		return -1;

	return 0;
}

int change_nonblocking(int fd)
{
	int pflag;

	pflag = fcntl(fd, F_GETFL);
	if (pflag == -1)
		return -1;

	if (fcntl(fd, F_SETFL, O_NONBLOCK | pflag) == -1)
		return -2;

	return 0;
}

struct socket_data *socket_data_create(
		uint16_t port,
		int backlog,
		enum make_listener_option option)
{
	struct socket_data *data;
	char portstr[10];
	struct addrinfo ai, *ai_ret;
	int ret;

	data = malloc(sizeof(struct socket_data));
	if (data == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));
		goto FAILED_TO_MALLOC;
	}

	sprintf(portstr, "%" PRIu16, port);
	
	memset(&ai, 0x00, sizeof(struct addrinfo));
	ai.ai_family = (option & MAKE_LISTENER_IPV6) 
		         ? AF_INET6 : AF_INET;
	ai.ai_socktype = (option & MAKE_LISTENER_DGRAM) 
		           ? SOCK_DGRAM : SOCK_STREAM;
	ai.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;

	if ((ret = getaddrinfo(NULL, portstr, &ai, &ai_ret)) != 0) {
		pr_err("failed to getaddrinfo(): %s", gai_strerror(ret));
		goto FAILED_TO_GETADDRINFO;
	}

	if ((data->fd = socket(
			ai_ret->ai_family, 
			ai_ret->ai_socktype,
			ai_ret->ai_protocol)) == -1) {
		pr_err("failed to socket(): %s", strerror(errno));
		goto FAILED_TO_SOCKET;
	}

	if (!(option & MAKE_LISTENER_DONTREUSE))
		if ((ret = socket_reuseaddr(data->fd)) == -1) {
			pr_err("failed to socket_reuseaddr(): %s (%d)",
				    strerror(errno), ret);
			goto FAILED_TO_REUSEADDR;
		}

	if (!(option & MAKE_LISTENER_BLOCKING))
		if ((ret = change_nonblocking(data->fd)) < 0) {
			pr_err("failed to change_nonblocking(): %s (%d)",
				    strerror(errno), ret);
			goto FAILED_TO_NONBLOCKING;
		}

	if (bind(data->fd, ai_ret->ai_addr, ai_ret->ai_addrlen) == -1) {
		pr_err("failed to bind(): %s", strerror(errno));
		goto FAILED_TO_BIND;
	}

	data->backlog = backlog;
	if (listen(data->fd, backlog) == -1) {
		pr_err("failed to listen(): %s", strerror(errno));
		goto FAILED_TO_LISTEN;
	}

	data->ai = ai_ret;

	return data;

FAILED_TO_LISTEN:
FAILED_TO_BIND:
FAILED_TO_NONBLOCKING:
FAILED_TO_REUSEADDR:
	close(data->fd);

FAILED_TO_SOCKET:
	freeaddrinfo(ai_ret);

FAILED_TO_GETADDRINFO:
	free(data);

FAILED_TO_MALLOC:
	return NULL;
}

void socket_data_destroy(struct socket_data *data)
{
	close(data->fd);
	freeaddrinfo(data->ai);
	free(data);
}

int epoll_handler_register(EpollHandler Handler, int tgfd, void *ptr, int events)
{
	struct epoll_handler *handler = Handler;
	struct epoll_event event;

	event.events = events;
	if (ptr == NULL)
		event.data.fd = tgfd;
	else
		event.data.ptr = ptr;

	if (epoll_ctl(handler->fd, EPOLL_CTL_ADD, tgfd, &event) == -1) {
		pr_err("failed to epoll_ctl(): %s", strerror(errno));
		return -2;
	}

	return 0;
}

int epoll_handler_unregister(EpollHandler Handler, int tgfd)
{
	struct epoll_handler *handler = Handler;
	if (epoll_ctl(handler->fd, EPOLL_CTL_DEL, tgfd, NULL) == -1) {
		pr_err("failed to epoll_ctl(): %s", strerror(errno));
		return -1;
	}

	return 0;
}

int epoll_handler_wait(EpollHandler Handler, int timeout)
{
	struct epoll_handler *handler = Handler;

	handler->cnt = epoll_wait(handler->fd,
				              handler->events,
							  handler->max_events,
							  timeout);
	if (handler->cnt < 0) {
		pr_err("failed to epoll_wait(): %s", strerror(errno));
		return -1;
	}

	return handler->cnt;
}

struct epoll_event *epoll_handler_pop(EpollHandler Handler)
{
	struct epoll_handler *handler = Handler;
	if (handler->cnt-- <= 0)
		return NULL;

	return &handler->events[handler->cnt];
}

void epoll_handler_destroy(EpollHandler Handler)
{
	struct epoll_handler *handler = Handler;

	close(handler->fd);
	free(handler->events);
	free(handler);
}

EpollHandler epoll_handler_create(size_t max_events)
{
	struct epoll_handler *handler;

	handler = malloc(sizeof(struct epoll_handler));
	if (handler == NULL) {
		pr_err("failed to epoll_handler(): %s", strerror(errno));
		return NULL;
	}

	handler->fd = epoll_create(max_events);
	if (handler->fd == -1) {
		free(handler);
		pr_err("failed to epoll_create(): %s", strerror(errno));
		return NULL;
	}

	handler->max_events = max_events;

	handler->events = malloc(sizeof(struct epoll_event) * max_events);
	if (handler->events == NULL) {
		close(handler->fd);
		free(handler);
		pr_err("failed to malloc(): %s", strerror(errno));
		return NULL;
	}

	return handler;
}


int epoll_handler_get_fd(EpollHandler handler)
{
	return ((struct epoll_handler *)handler)->fd;
}

int extract_addrinfo(
		struct addrinfo *ai,
		char *hoststr,
		char *portstr)
{
	int ret;
	
	if ((ret = getnameinfo(
			ai->ai_addr, ai->ai_addrlen,			  // address
			hoststr, sizeof(hoststr),				  // host name
			portstr, sizeof(portstr),				  // service name
			NI_NUMERICHOST | NI_NUMERICSERV)) != 0)	{ // flags
		pr_err("failed to getnameinfo(): %s", gai_strerror(ret));
		return -1;
	}

	freeaddrinfo(ai);

	return 0;
}
