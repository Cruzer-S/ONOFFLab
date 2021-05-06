#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <inttypes.h>

#define __USE_XOPEN2K	// to use `struct addrinfo`
#include <netdb.h>
#include <sys/epoll.h>

#include <pthread.h>

#include "socket_manager.h"
#include "logger.h"

#define DEFAULT_HTTP_PORT	1584
#define DEFAULT_DEVICE_PORT	3606
#define DEFAULT_HTTP_BACKLOG 30
#define DEFAULT_DEVICE_BACKLOG 30

#define MAX_EVENTS 128

int parse_arguments(int argc, char *argv[], ...);
int start_epoll_server(int serv_sock);
int register_to_epoll(int epfd, int tgfd, int events);
int delete_to_epoll(int epfd, int tgfd);

int main(int argc, char *argv[])
{
	int http_sock, device_sock;
	struct addrinfo *http_ai, *device_ai;
	int http_backlog, device_backlog;
	uint16_t http_port, device_port;
	int ret;

	http_port = DEFAULT_HTTP_PORT;
	device_port = DEFAULT_DEVICE_PORT;
	http_backlog = DEFAULT_HTTP_BACKLOG;
	device_backlog = DEFAULT_DEVICE_BACKLOG;

	if (parse_arguments(argc, argv, 
		    16, &http_port, 
			16, &device_port, 
			 4, &http_backlog,
			 4, &device_backlog) < 0)
		pr_crt("%s", "failed to parse_arguments()");

	http_sock = make_listener(http_port, http_backlog, &http_ai, 
			                  MAKE_LISTENER_DEFAULT);
	device_sock = make_listener(device_port, device_backlog, &device_ai,
								MAKE_LISTENER_DEFAULT);
	if (http_sock < 0 || device_sock < 0)
		pr_crt("%s", "make_listener() error");

	do {char hoststr[INET6_ADDRSTRLEN], portstr[10];

		if ((ret = getnameinfo(
						http_ai->ai_addr, http_ai->ai_addrlen,		// address
						hoststr, sizeof(hoststr),					// host name
						portstr, sizeof(portstr),					// service name (port number)
						NI_NUMERICHOST | NI_NUMERICSERV)) != 0)		// flags
			pr_crt("failed to getnameinfo(): %s (%d)", gai_strerror(ret), ret);

		pr_out("http server running at %s:%s (backlog %d)", hoststr, portstr, http_backlog);

		if ((ret = getnameinfo(
						device_ai->ai_addr, device_ai->ai_addrlen,	// address
						hoststr, sizeof(hoststr),					// host name
						portstr, sizeof(portstr),					// service name (port number)
						NI_NUMERICHOST | NI_NUMERICSERV)) != 0)		// flags
			pr_crt("failed to getnameinfo(): %s (%d)", gai_strerror(ret), ret);

		pr_out("device server running at %s:%s (backlog %d)", hoststr, portstr, device_backlog);

		freeaddrinfo(device_ai);
		freeaddrinfo(http_ai);
	} while (false);
			    
	while (true) {
		if (start_epoll_server(http_sock) < 0)
			pr_crt("%s", "failed to start_epoll_server()");

		if (start_epoll_server(device_sock) < 0)
			pr_crt("%s", "failed to start_epoll_server()");
	}

	close(http_sock); close(device_sock);
	
	return 0;
}

int parse_arguments(int argc, char *argv[], ...)
{
	char *test;
	long overflow;
	va_list ap;
	int ret = 0, type;

	va_start(ap, argv);

	while (argv++, --argc > 0) {
		switch ( (type = va_arg(ap, int)) ) {
		case 16: // uint16_t
		case  4: // int
			overflow = strtol(*argv, &test, 10);
			if (overflow == 0 && *argv == test) {
				pr_err("failed to convert string to long: %s", "Not a number");
				ret = -1; goto CLEANUP;
			}

			if (overflow == LONG_MIN || overflow == LONG_MAX)
				if (errno == ERANGE) {
					pr_err("failed to strtol(): %s", strerror(errno));
					ret = -2; goto CLEANUP;
				}

			break;

		default:
			pr_err("unspecific type: %d", type);
			ret = -3; goto CLEANUP;
		}

		switch (type) {
		case 16: 
			if (overflow < 0 || overflow > UINT16_MAX) {
				pr_err("failed to convert string to uint16_t: %s", "Out of range");
				ret = -4; goto CLEANUP;
			}

			*va_arg(ap, uint16_t *) = overflow;
			break;

		case 4:
			if (overflow < INT_MIN || overflow > INT_MAX) {
				pr_err("failed to convert string to int: %s", "Out of range");
				ret = -5; goto CLEANUP;
			}

			*va_arg(ap, int *) = overflow;
			break;

		}
	}

CLEANUP:
	va_end(ap);
	return ret;
}

int start_epoll_server(int serv_sock)
{
	int epoll_fd;
	int ret;
	struct epoll_event *ep_events;

	epoll_fd = epoll_create(MAX_EVENTS);
	if (epoll_fd == -1) {
		pr_err("failed to epoll_create(): %s", strerror(errno));
		ret = -1; goto ERROR_EPOLL_CREATE;
	}

	ep_events = malloc(sizeof(struct epoll_event) * MAX_EVENTS);
	if (ep_events == NULL) {
		pr_err("failed to malloc(ep_events): %s", strerror(errno));
		ret = -2; goto ERROR_MALLOC_EVENTS;
	}

	if (register_to_epoll(epoll_fd, serv_sock, EPOLLIN) < 0) {
		pr_err("failed to register_to_epoll(): %s", strerror(errno));
		ret = -3; goto ERROR_MALLOC_EVENTS;
	}

	while (true)
	{
		char buf_time[128];
		int nclient;

		pr_out("[%s] epoll waiting ...", GET_TIME0(buf_time));

		if ((nclient = epoll_wait(epoll_fd, ep_events, MAX_EVENTS, -1)) == -1) {
			pr_err("failed to epoll_wait(%d): %s", serv_sock, strerror(errno));
			continue;
		}

		for (int i = 0; i < nclient; i++) {
			if (ep_events[i].data.fd == serv_sock) {
				while (true)
				{
					int clnt_sock = accept(serv_sock, NULL, NULL);
					if(clnt_sock == -1) {
						if (errno == EAGAIN)
							break;

						pr_err("failed to accept(): %s", strerror(errno));
						break;
					}

					change_nonblocking(clnt_sock);

					register_to_epoll(epoll_fd, clnt_sock, EPOLLIN);

					pr_out("accept: add new socket: %d", clnt_sock);
				} 
			} else { // received data from client socket
				int ret_recv;
				char buf[BUFSIZ];
				int clnt_sock = ep_events[i].data.fd;

				if ((ret_recv = recv(clnt_sock, buf, sizeof(buf), 0)) == -1) {
					pr_err("failed to recv(): %s", strerror(errno));
					continue;
				}

				if (ret_recv == 0) { // disconnect client
					pr_out("closed by foreign host (sfd = %d)", ep_events[i].data.fd);
					if (delete_to_epoll(epoll_fd, clnt_sock) < 0) {
						pr_err("%s", "failed to delete_to_epoll(): force socket disconnection.");
						close(clnt_sock);
					}

					continue;
				}

				pr_out("Message from client(%d): %s", clnt_sock, buf);
			} // end of if - else (ep_events[i].data.fd == serv_sock)
		} // end of for (int i = 0; i < nclient; i++)

	} // end of while (true)

	return 0;

ERROR_MALLOC_EVENTS:
	free(ep_events);

ERROR_EPOLL_CREATE:
	close(epoll_fd);

	return ret;
}

int register_to_epoll(int epfd, int tgfd, int events)
{
	struct epoll_event ev;

	ev.events = events;
	ev.data.fd = tgfd;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, tgfd, &ev) == -1) {
		pr_err("failed to epoll_ctl(): %s", strerror(errno));
		return -1;
	}

	return 0;
}

int delete_to_epoll(int epfd, int tgfd)
{
	if (epoll_ctl(epfd, EPOLL_CTL_DEL, tgfd, NULL) == -1) {
		pr_err("failed to epoll_ctl(): %s", strerror(errno));
		return -1;
	}

	close(tgfd);

	return 0;
}

