#define _POSIX_C_SOURCE 200112L

#include "socket_manager.h"

#include "logger.h"
#include <sys/epoll.h>

struct epoll_handler {
	int fd;
	int cnt;
	struct epoll_event *events;
	int max_events;
};

int socket_reuseaddr(int sock)
{
	int sockopt = 1;

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)) == -1)
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

int make_listener(uint16_t port, int backlog,
                  struct addrinfo **ai_ret_arg,
                  enum make_listener_option option)
{
	int sock, ret = 0;
	struct addrinfo ai, *ai_ret;
	char portstr[10];

	sprintf(portstr, "%.9d", port);
	
	memset(&ai, 0x00, sizeof(struct addrinfo));
	ai.ai_family = (option & MAKE_LISTENER_IPV6) ? AF_INET6 : AF_INET;
	ai.ai_socktype = (option & MAKE_LISTENER_DGRAM) ? SOCK_DGRAM : SOCK_STREAM;
	ai.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;

	if ((ret = getaddrinfo(NULL, portstr, &ai, &ai_ret)) != 0) {
		pr_err("failed to getaddrinfo(): %s", gai_strerror(ret));
		return -1;
	}

	if ((sock = socket(ai_ret->ai_family, ai_ret->ai_socktype, ai_ret->ai_protocol)) == -1) {
		pr_err("failed to socket(): %s", strerror(errno));
		ret = -2; goto CLEANUP_AI;
	}

	if (!(option & MAKE_LISTENER_DONTREUSE))
		if ((ret = socket_reuseaddr(sock)) == -1) {
			pr_err("failed to socket_reuseaddr(): %s (%d)", strerror(errno), ret);
			ret = -3; goto CLEANUP_SOCKET;
		}

	if (!(option & MAKE_LISTENER_BLOCKING))
		if ((ret = change_nonblocking(sock)) < 0) {
			pr_err("failed to change_nonblocking(): %s (%d)", strerror(errno), ret);
			ret = -4; goto CLEANUP_SOCKET;
		}

	if (bind(sock, ai_ret->ai_addr, ai_ret->ai_addrlen) == -1) {
		pr_err("failed to bind(): %s", strerror(errno));
		ret = -5; goto CLEANUP_SOCKET;
	}

	if (listen(sock, backlog) == -1) {
		pr_err("failed to listen(): %s", strerror(errno));
		ret = -6; goto CLEANUP_SOCKET;
	}

	if (ai_ret_arg != NULL)
		*ai_ret_arg = ai_ret;

	return sock;

CLEANUP_SOCKET:
	close(sock);

CLEANUP_AI:
	freeaddrinfo(ai_ret);
	return ret;
}

int epoll_handler_register(struct epoll_handler *handler, int tgfd, void *ptr, int events)
{
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

int epoll_handler_unregister(struct epoll_handler *handler, int tgfd)
{
	if (epoll_ctl(handler->fd, EPOLL_CTL_DEL, tgfd, NULL) == -1) {
		pr_err("failed to epoll_ctl(): %s", strerror(errno));
		return -1;
	}

	return 0;
}

int epoll_handler_wait(struct epoll_handler *handler, int timeout)
{
	if ((handler->cnt = epoll_wait(handler->fd,
				                   handler->events, handler->max_events,
								   timeout)) < 0)
	{
		pr_err("failed to epoll_wait(): %s", strerror(errno));
		return -1;
	}

	return handler->cnt;
}

struct epoll_event *epoll_handler_pop(struct epoll_handler *handler)
{
	if (handler->cnt-- <= 0)
		return NULL;

	return &handler->events[handler->cnt];
}

void epoll_handler_destroy(struct epoll_handler *handler)
{
	close(handler->fd);
	free(handler->events);
	free(handler);
}

struct epoll_handler *epoll_handler_create(size_t max_events)
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

int epoll_handler_get_fd(struct epoll_handler *handler)
{
	return handler->fd;
}

int get_addr_from_ai(struct addrinfo *ai, char *hoststr, char *portstr)
{
	int ret;
	
	if ((ret = getnameinfo(
					ai->ai_addr, ai->ai_addrlen,			// address
						hoststr, sizeof(hoststr),				// host name
						portstr, sizeof(portstr),				// service name (port number)
						NI_NUMERICHOST | NI_NUMERICSERV)) != 0)	// flags
	{
		pr_err("failed to getnameinfo(): %s", gai_strerror(ret));
		return -1;
	}

	freeaddrinfo(ai);

	return 0;
}
