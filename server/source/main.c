#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <inttypes.h>

#define __USE_XOPEN2K	// to use `struct addrinfo`
#include <netdb.h>
#include <sys/epoll.h>

#include <pthread.h>

#include "queue.h"

#include "socket_manager.h"
#include "logger.h"

#define DEFAULT_HTTP_PORT	1584
#define DEFAULT_DEVICE_PORT	3606
#define DEFAULT_HTTP_BACKLOG 30
#define DEFAULT_DEVICE_BACKLOG 30

#define MAX_EVENTS 128
#define BUFFER_SIZE (1024 * 4) // 4 KiB

#define HTTP 0
#define DEVICE 1

struct client_data {
	int fd;

	uint8_t *buffer;
	size_t bsize;
	size_t busage;
};

struct thread_args {
	struct epoll_struct *ep_struct;
	int serv_sock;
};

struct epoll_struct {
	int fd;
	struct epoll_event *events;
};

int parse_arguments(int argc, char *argv[], ...);
void *start_epoll_server(void *args);
int register_to_epoll(int epfd, int tgfd, int events);
int delete_to_epoll(int epfd, struct client_data *clnt_data);
struct client_data *make_client_data(int fd, int buffer_size);
void destroy_client_data(struct client_data *clnt_data);
int make_epoll(size_t max_events, struct epoll_struct *ep_struct);

int main(int argc, char *argv[])
{
	int serv_sock[2];
	int backlog[2];
	uint16_t port[2];
	struct addrinfo *ai[2];
	struct epoll_struct ep_struct[2];
	struct thread_args thd_arg[2];
	pthread_t tid[2];

	int ret;

	port[HTTP] = DEFAULT_HTTP_PORT;
	port[DEVICE] = DEFAULT_DEVICE_PORT;

	backlog[HTTP] = DEFAULT_HTTP_BACKLOG;
	backlog[DEVICE] = DEFAULT_DEVICE_BACKLOG;

	if ((ret = parse_arguments(argc, argv, 
		    16, &port[HTTP], 
			16, &port[DEVICE], 
			 4, &backlog[HTTP],
			 4, &backlog[DEVICE])) < 0)
		pr_crt("failed to parse_arguments(): %d", ret);

	for (int i = 0; i < 2; i++)
	{
		serv_sock[i] = make_listener(port[i], backlog[i], &ai[i], MAKE_LISTENER_DEFAULT);
		if (serv_sock[i] < 0)
			pr_crt("make_listener() error: %d", serv_sock[i]);

		char hoststr[INET6_ADDRSTRLEN], portstr[10];
		if ((ret = getnameinfo(
						ai[i]->ai_addr, ai[i]->ai_addrlen,		// address
						hoststr, sizeof(hoststr),				// host name
						portstr, sizeof(portstr),				// service name (port number)
						NI_NUMERICHOST | NI_NUMERICSERV)) != 0)	// flags
			pr_crt("failed to getnameinfo(): %s (%d)", gai_strerror(ret), ret);

		pr_out("%s server running at %s:%s (backlog %d)", 
			   (i == HTTP) ? "HTTP" : "DEVICE", hoststr, portstr, backlog[i]);

		freeaddrinfo(ai[i]);

		if ((ret = make_epoll(MAX_EVENTS, &ep_struct[i])) < 0)
			pr_crt("failed to make_epoll(): %d", ret);
		
		if ((ret = register_to_epoll(ep_struct[i].fd, serv_sock[i], EPOLLIN)) < 0)
			pr_crt("failed to register_to_epoll(): %d", ret);

		thd_arg[i].ep_struct = &ep_struct[i];
		thd_arg[i].serv_sock = serv_sock[i];
		if ((ret = pthread_create(&tid[i], NULL, start_epoll_server, &thd_arg[i])) != 0)
			pr_crt("failed to pthread_create(): %s", strerror(ret));
	}

	for (int i = 0; i < 2; i++)
		if ((ret = pthread_join(tid[i], NULL)) != 0)
			pr_crt("failed to pthread_join: %s", strerror(ret));

	return 0;
}

int make_epoll(size_t max_events, struct epoll_struct *ep_struct)
{
	ep_struct->fd = epoll_create(MAX_EVENTS);
	
	if (ep_struct->fd == -1) {
		pr_err("failed to epoll_create(): %s", strerror(errno));
		return -1;
	}

	ep_struct->events = malloc(sizeof(struct epoll_event) * MAX_EVENTS);
	if (ep_struct->events == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));
		return -2;
	}

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

void *start_epoll_server(void *args)
{
	struct thread_args *targs = args;

	int ret;
	int epoll_fd, serv_sock;
	struct epoll_event *epoll_events;

	epoll_fd = targs->ep_struct->fd;
	epoll_events = targs->ep_struct->events;
	serv_sock = targs->serv_sock;
	
	while (true)
	{
		struct client_data *clnt_data;
		char buf_time[128];
		int nclient;

		pr_out("[%s] epoll waiting ...", GET_TIME0(buf_time));

		if ((nclient = epoll_wait(targs->ep_struct->fd, epoll_events, MAX_EVENTS, -1)) == -1) {
			pr_err("failed to epoll_wait(): %s", strerror(errno));
			continue;
		}

		for (int i = 0; i < nclient; i++)
		{
			clnt_data = epoll_events[i].data.ptr;

			if (clnt_data->fd == serv_sock) { // accept new socket
				while (true)
				{
					int clnt_sock = accept(serv_sock, NULL, NULL);
					if(clnt_sock == -1) {
						if (errno == EAGAIN)
							break;

						pr_err("failed to accept(): %s", strerror(errno));
						break;
					} else change_nonblocking(clnt_sock);
										
					if (register_to_epoll(epoll_fd, clnt_sock, EPOLLIN) < 0) {
						pr_err("%s", "failed to register_to_epoll()");
						close(clnt_sock);
						continue;
					}

					pr_out("accept: add new socket: %d", clnt_sock);
				} 
			} else { // client handling
				// received data from client
				int ret_recv;
				int remain_space = clnt_data->bsize - clnt_data->busage;

				if (remain_space == 0) {
					pr_out("there's no space to receive data from client(%d)", clnt_data->fd);
					continue;
				}

				if ((ret_recv = recv(clnt_data->fd,
									 clnt_data->buffer + clnt_data->busage,
									 remain_space, 0)) == -1) {
					pr_err("failed to recv(): %s", strerror(errno));
					
					if (delete_to_epoll(epoll_fd, clnt_data) < 0)
						pr_err("%s", "failed to delete_to_epoll()");

					continue;
				} else clnt_data->busage += ret_recv;

				if (ret_recv == 0) { // disconnect client
					pr_out("closed by foreign host (sfd = %d)", clnt_data->fd);

					if (delete_to_epoll(epoll_fd, clnt_data) < 0)
						pr_err("failed to delete_to_epoll(): %s", "force socket disconnection");

					continue;
				}
			} // end of if - else (ep_events[i].data.fd == serv_sock)
		} // end of for (int i = 0; i < nclient; i++)
	} // end of while (true)

	return 0;

ERROR_MALLOC_EVENTS:
	free(epoll_events);

ERROR_EPOLL_CREATE:
	close(epoll_fd);

	return NULL;
}

int register_to_epoll(int epfd, int tgfd, int events)
{
	struct epoll_event ev;

	ev.events = events;

	struct client_data *clnt_data = make_client_data(tgfd, BUFFER_SIZE);
	if (clnt_data == NULL) {
		pr_err("%s", "failed to make_client_data(): %s");
		return -1;
	} else ev.data.ptr = clnt_data;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, tgfd, &ev) == -1) {
		pr_err("failed to epoll_ctl(): %s", strerror(errno));
		destroy_client_data(clnt_data);
		return -2;
	}

	return 0;
}

int delete_to_epoll(int epfd, struct client_data *clnt_data)
{
	int ret = 0;

	ret = epoll_ctl(epfd, EPOLL_CTL_DEL, clnt_data->fd, NULL);
	close(clnt_data->fd);
	destroy_client_data(clnt_data);

	if (ret == -1)
		pr_err("failed to epoll_ctl(): %s", strerror(errno));

	return ret;
}

struct client_data *make_client_data(int fd, int buffer_size)
{
	struct client_data *clnt_data;

	clnt_data = malloc(sizeof(struct client_data));
	if (clnt_data == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));
		return NULL;
	}

	clnt_data->buffer = malloc(buffer_size);
	if (clnt_data->buffer == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));
		return NULL;
	}

	clnt_data->fd = fd;
	clnt_data->busage = 0;
	clnt_data->bsize = buffer_size;

	return clnt_data;
}

void destroy_client_data(struct client_data *clnt_data)
{
	free(clnt_data->buffer);
	free(clnt_data);
}

