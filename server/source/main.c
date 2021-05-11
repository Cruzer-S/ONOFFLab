#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <inttypes.h>

#define __USE_XOPEN2K	// to use `struct addrinfo`
#include <netdb.h>
#include <semaphore.h>
#include <sys/epoll.h>

#include <pthread.h>

#include "queue.h"

#include "socket_manager.h"
#include "utility.h"
#include "logger.h"

#define DEFAULT_HTTP_PORT	1584
#define DEFAULT_DEVICE_PORT	3606
#define DEFAULT_HTTP_BACKLOG 30
#define DEFAULT_DEVICE_BACKLOG 30

#define BUFFER_SIZE (1024 * 4) // 4 KiB

#define HTTP_MAX_BODY_SIZE ((size_t) (1024UL * 1024UL * 10UL)) // 5 MiB

#define HTTP 0
#define DEVICE 1

#define PRODUCER_THREADS 3

struct client_data {
	int fd;
	uint8_t *buffer;
	size_t bsize;
	size_t busage;

	pthread_mutex_t mutex;

	struct handler_struct *handler;
};

struct thread_args {
	struct epoll_handler *handler;
	int listener_fd;
	pthread_cond_t cond;
	struct queue *queue;

	int tid;
};

struct server_data {
	int fd;
	int backlog;
	uint16_t port;
};

struct handler_struct {
	char *header;
	char *body;
};

void *start_epoll_server(void *args);
struct client_data *make_client_data(int fd, int buffer_size);
void destroy_client_data(struct client_data *clnt_data);
int handle_client_data(struct client_data *clnt_data, int tid);
void *producer_thread(void *args);

int main(int argc, char *argv[])
{
	struct server_data serv_data[2];
	pthread_t tid[2];
	
	int ret;

	serv_data[HTTP].port = DEFAULT_HTTP_PORT;
	serv_data[HTTP].backlog = DEFAULT_HTTP_BACKLOG;

	serv_data[DEVICE].port = DEFAULT_DEVICE_PORT;
	serv_data[DEVICE].backlog = DEFAULT_DEVICE_BACKLOG;

	if ((ret = parse_arguments(argc, argv,
		    16, &serv_data[HTTP].port,
			16, &serv_data[DEVICE].port,
			 4, &serv_data[HTTP].backlog,
			 4, &serv_data[DEVICE].backlog)) < 0)
		pr_crt("failed to parse_arguments(): %d", ret);

	for (int i = 0; i < 2; i++)
	{
		struct addrinfo *ai;
		serv_data[i].fd = make_listener(serv_data[i].port, serv_data[i].backlog, &ai, MAKE_LISTENER_DEFAULT);
		if (serv_data[i].fd < 0)
			pr_crt("make_listener() error: %d", serv_data[i].fd);

		char hoststr[INET6_ADDRSTRLEN], portstr[10];
		if ((ret = getnameinfo(
						ai->ai_addr, ai->ai_addrlen,			// address
						hoststr, sizeof(hoststr),				// host name
						portstr, sizeof(portstr),				// service name (port number)
						NI_NUMERICHOST | NI_NUMERICSERV)) != 0)	// flags
			pr_crt("failed to getnameinfo(): %s (%d)", gai_strerror(ret), ret);

		pr_out("%s server running at %s:%s (backlog %d)",
			   (i == HTTP) ? "HTTP" : "DEVICE", hoststr, portstr, serv_data[i].backlog);

		freeaddrinfo(ai);
	
		struct thread_args *thd_arg = malloc(sizeof(struct thread_args));
		if (thd_arg == NULL)
			pr_crt("failed to malloc(): %s", strerror(errno));

		thd_arg->handler = epoll_handler_create(EPOLL_MAX_EVENTS);
		if (thd_arg->handler == NULL)
			pr_crt("%s", "failed to epoll_handler_create()");

		struct client_data *clnt_data = malloc(sizeof(struct client_data));
		if (clnt_data == NULL)
			pr_crt("failed to malloc(): %s", strerror(errno));

		clnt_data->fd = serv_data[i].fd;
		if ((ret = epoll_handler_register(thd_arg->handler, clnt_data->fd, clnt_data, EPOLLIN)) < 0)
			pr_crt("failed to epoll_handler_register(): %d", ret);

		if ((ret = pthread_cond_init(&thd_arg->cond, NULL)) != 0)
			pr_crt("failed to pthread_cond_init(): %s", strerror(ret));

		thd_arg->queue = queue_create(EPOLL_MAX_EVENTS, &thd_arg->cond);
		if (thd_arg->queue == NULL)
			pr_crt("failed to queue_create(%d)", QUEUE_MAX_SIZE);
		
		thd_arg->listener_fd = serv_data[i].fd;
		thd_arg->tid = i + 1;

		if ((ret = pthread_create(&tid[i], NULL, start_epoll_server, thd_arg)) != 0)
			pr_crt("failed to pthread_create(): %s", strerror(ret));

		if (i == HTTP) {
			struct thread_args *thd_new_arg = malloc(sizeof(struct thread_args));
			if (thd_new_arg == NULL)
				pr_crt("failed to malloc(): %s", strerror(errno));

			thd_new_arg = 

			for (int i = 0; i < PRODUCER_THREADS; i++)
				if ((ret = pthread_create(&tid[i], NULL, producer_thread, thd_arg)) != 0)
					pr_crt("failed to pthread_create(): %s", strerror(ret));
		}
	}
	
	for (int i = 0; i < 2; i++)
		if ((ret = pthread_join(tid[i], NULL)) != 0)
			pr_crt("failed to pthread_join: %s", strerror(ret));

	return 0;
}

int accept_client(int serv_sock, struct epoll_handler *handler)
{
	struct client_data *clnt_data;
	int ret;
	int nclient = 0;

	while (true)
	{
		int clnt_sock = accept(serv_sock, NULL, NULL);
		if (clnt_sock == -1) {
			if (errno == EAGAIN)
				break;

			pr_err("failed to accept(): %s", strerror(errno));
			return -1;
		} else change_nonblocking(clnt_sock);

		if ((clnt_data = make_client_data(clnt_sock, BUFFER_SIZE)) == NULL) {
			pr_err("failed to make_client_data(): %d", clnt_sock);
			close(clnt_sock);

			return -2;
		}

		if ((ret = epoll_handler_register(handler, clnt_sock, clnt_data, EPOLLIN)) < 0) {
			pr_err("failed to epoll_handler_register(): %d", ret);
			close(clnt_sock);
			destroy_client_data(clnt_data);

			return -3;
		}

		nclient++;
	}

	return nclient;
}

int receive_request_from_client(struct client_data *clnt_data, struct epoll_handler *handler)
{
	int ret_recv, ret;
	int remain_space;

	remain_space = clnt_data->bsize - clnt_data->busage;
	if (remain_space <= 0) {
		pr_err("there's no space to receive data from the client: %d", clnt_data->fd);
		return -1;
	}

	if ((ret_recv = recv(clnt_data->fd,
						 clnt_data->buffer + clnt_data->busage,
						 remain_space, 0)) == -1) 
	{
		pr_err("failed to recv(): %s", strerror(errno));
		if ((ret = epoll_handler_unregister(handler, clnt_data->fd)) < 0)
			pr_err("failed to delete_to_epoll(): %d", ret);

		return -2;
	} else clnt_data->busage += ret_recv;

	if (ret_recv == 0) { // disconnect client
		pr_out("closed by foreign host (sfd = %d)", clnt_data->fd);

		if (epoll_handler_unregister(handler, clnt_data->fd) < 0)
			pr_err("failed to delete_to_epoll(): %s",
				   "force socket disconnection");

		return -3;
	}

	return 0;
}

void *start_epoll_server(void *args)
{
	struct thread_args targs = *(struct thread_args *)args;
	struct client_data *clnt_data;
	int ret;

	free(args);

	if (targs.tid == 2)
		return NULL;

	while (true) 
	{
		int nclient = epoll_handler_wait(targs.handler, -1);
		if (nclient < 0) {
			pr_err("failed to epoll_handler_wait(): %d", nclient);
			continue;
		}

		for (; nclient-- > 0;
			   clnt_data = epoll_handler_pop(targs.handler)->data.ptr)
		{
			if (clnt_data->fd == targs.listener_fd) {
				if ((ret = accept_client(targs.listener_fd, targs.handler)) < 0)
					pr_err("[%d] failed to accept_client(): %d", targs.tid, ret);

				pr_out("accept new %d client(s)", ret);
			} else { // client handling
				if ((ret = receive_request_from_client(clnt_data, targs.handler)) < 0) {
					pr_err("[%d] failed to receive_request_from_client(): %d", targs.tid, ret);
					close(clnt_data->fd);
					destroy_client_data(clnt_data);
				}

				if ((ret = handle_client_data(clnt_data, targs.tid)) < 0) {
					pr_err("[%d] failed to handle_client_data(): %d", targs.tid, ret);
				} else if (ret == 1) {
					pr_out("[%d] receive all the data from the client: %d", targs.tid, clnt_data->fd);
					epoll_handler_unregister(targs.handler, clnt_data->fd);

					queue_enqueue(targs.queue, (struct queue_data) {
							.type = QUEUE_DATA_PTR,
							.ptr = clnt_data
					});
				}
			}
		}

		pthread_cond_broadcast(&targs.cond);
	}

	return (void *) 0;
}

int handle_client_data(struct client_data *clnt_data, int tid)
{
	char *eoh; //End-of-body
	char *has_body;
	size_t body_len, over_len;

	if (clnt_data->handler != NULL)
		return (clnt_data->bsize == clnt_data->busage);

	if ((eoh = strstr((char *) clnt_data->buffer, "\r\n\r\n"))) {
		pr_out("[%d] HTTP request from the client: %d", tid, clnt_data->fd);

		eoh += 4;

		// Do NOT call free() function to clnt_data->handler
		// It will be freed by destroy_client_data() function
		clnt_data->handler = malloc(sizeof(struct handler_struct));
		if (clnt_data->handler == NULL) {
			pr_err("[%d] failed to malloc(): %s", tid, strerror(errno));
			return -2;
		} else clnt_data->handler->body = clnt_data->handler->header = NULL;

		if ((has_body = strstr((char *) clnt_data->buffer, "Content-Length: "))) {
			if (sscanf(has_body, "Content-Length: %zu", &body_len) != 1) {
				pr_err("[%d] invalid HTTP Header: %s", tid, "failed to parse Content-Length");
				return -1;
			}

			if (body_len > HTTP_MAX_BODY_SIZE || body_len == 0) {
				pr_err("[%d] request body size is too big/small to receive: %zu / %zu",
						tid, body_len, HTTP_MAX_BODY_SIZE);
				return -4;
			}
			
			clnt_data->handler->body = malloc(sizeof(char) * body_len);
			if (clnt_data->handler->body == NULL) {
				pr_err("[%d] failed to malloc(): %s", tid, strerror(errno));
				return -3;
			}

			clnt_data->handler->header = (char *) clnt_data->buffer;
			clnt_data->buffer = (uint8_t *) clnt_data->handler->body;

			over_len = (clnt_data->handler->header + clnt_data->busage) - eoh;

			strncpy(clnt_data->handler->body, eoh, over_len);

			clnt_data->busage = over_len;
			clnt_data->bsize = body_len;

			pr_out("[%d] HTTP request from the client: %d\n"
				   "body-size: %zu, over-read: %zu", 
				   tid, clnt_data->fd, clnt_data->bsize, clnt_data->busage);
		} else {
			pr_out("[%d] HTTP request from the client: %d\n"
					"header-only, no body", tid, clnt_data->fd);
		}
	}

	return 0;
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
		free(clnt_data);
		pr_err("failed to malloc(): %s", strerror(errno));
		return NULL;
	}

	clnt_data->fd = fd;
	clnt_data->busage = 0;
	clnt_data->bsize = buffer_size;

	if (pthread_mutex_init(&clnt_data->mutex, NULL) == -1) {
		free(clnt_data->buffer);
		free(clnt_data);
		pr_err("failed_to pthread_mutex_init(): %s", strerror(errno));
		
		return NULL;
	}

	clnt_data->handler = NULL;

	return clnt_data;
}

void destroy_client_data(struct client_data *clnt_data)
{
	pthread_mutex_destroy(&clnt_data->mutex);

	if (clnt_data->handler != NULL) {
		free(clnt_data->handler->body);
		free(clnt_data->handler->header);
		free(clnt_data->handler);
		free(clnt_data);
	} else {
		free(clnt_data->buffer);
		free(clnt_data);
	}
}

void *producer_thread(void *args)
{
	struct thread_args targs = *(struct thread_args *)args;
	struct queue_data data;
	struct client_data *clnt_data;
	int ret;

	while (true)
	{
		data = queue_dequeue(targs.queue);
		if (data.type == QUEUE_DATA_ERROR) {
			pr_err("[%d] failed to queue_dequeue(): %s", 
					targs.tid, "QUEUE_DATA_ERROR");
			continue;
		}

		if (data.type == QUEUE_DATA_UNDEF) {
			pr_err("[%d] failed to queue_dequeue(): %s",
					targs.tid, "QUEUE_DATA_UNDEF");
			continue;
		}

		clnt_data = data.ptr;

		pr_out("clnt_data: %zu / %zu", clnt_data->bsize, clnt_data->busage);

		destroy_client_data(clnt_data);
	}

	return NULL;
}

