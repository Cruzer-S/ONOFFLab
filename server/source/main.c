#define _POSIX_C_SOURCE 200112L

#include "socket_manager.h"
#include "queue.h"
#include "utility.h"
#include "logger.h"
#include "client_handler.h"
#include "process_http.h"
#include "targs.h"

#define DEFAULT_HTTP_PORT	1584
#define DEFAULT_DEVICE_PORT	3606
#define DEFAULT_HTTP_BACKLOG 30
#define DEFAULT_DEVICE_BACKLOG 30

#define BUFFER_SIZE (1024 * 4) // 4KiB

#define HTTP_MAX_BODY_SIZE ((size_t) (1024UL * 1024UL * 10UL)) // 10.0 MiB

#define HTTP 0
#define DEVICE 1

#define HTTP_CONSUMER_THREADS 3

struct server_data {
	int fd;
	int backlog;
	uint16_t port;
};

int accept_client(int serv_sock, struct epoll_handler *handler);
int receive_request_from_client(struct client_data *clnt_data, struct epoll_handler *handler);

void *http_server(void *args);
void *http_consumer_thread(void *args);

int main(int argc, char *argv[])
{
	struct server_data serv_data[2];
	pthread_t tid[HTTP + DEVICE + HTTP_CONSUMER_THREADS];

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
	
		freeaddrinfo(ai);

		struct thread_args *serv_arg;
		if (i == HTTP) {
			serv_arg = make_thread_argument(i, serv_data[i].fd, false);
			if (serv_arg == NULL)
				pr_crt("failed to make_thread_argument: %d", i);

			for (int j = 0; j < HTTP_CONSUMER_THREADS; j++) {
				struct thread_args *prod_args;

				prod_args = make_thread_argument(j + 2, serv_data[i].fd, true);
				if (prod_args == NULL)
					pr_crt("failed to make_thread_argument: %d", i);

				prod_args->cond = serv_arg->cond;
				prod_args->queue = serv_arg->queue;

				if ((ret = pthread_create(&tid[i], NULL, http_consumer_thread, prod_args)) != 0)
					pr_crt("failed to pthread_create(): %s", strerror(ret));
			}
			
			if ((ret = pthread_create(&tid[i], NULL, http_server, serv_arg)) != 0)
				pr_crt("failed to pthread_create(): %s", strerror(ret));
		} else if (i == DEVICE) {
			continue;

			serv_arg = make_thread_argument(i, serv_data[i].fd, false);
			if (serv_arg == NULL)
				pr_crt("failed to make_thread_argument: %d", i);

			destroy_thread_argument(serv_arg);
		}

		pr_out("%s server running at %s:%s (backlog %d)",
			   (i == HTTP) ? "HTTP" : "DEVICE", hoststr, portstr, serv_data[i].backlog);
	}

	for (int i = 0; i < HTTP + DEVICE + HTTP_CONSUMER_THREADS; i++)
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

		if ((clnt_data = make_client_data(clnt_sock, BUFFER_SIZE, HTTP_MAX_BODY_SIZE)) == NULL) {
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

void *http_server(void *args)
{
	struct thread_args targs = *(struct thread_args *)args;
	struct client_data *clnt_data;
	int ret;

	while (true)
	{
		int nclient = epoll_handler_wait(targs.handler, -1);
		if (nclient < 0) {
			pr_err("[%s] failed to epoll_handler_wait(): %d", "HTTP", nclient);
			continue;
		}

		do {
			clnt_data = epoll_handler_pop(targs.handler)->data.ptr;
			if (clnt_data == NULL) {
				pr_err("[%s] failed to epoll_handler_pop()", "HTTP");
				break;
			}

			if (clnt_data->fd == targs.listener_fd) {
				if ((ret = accept_client(targs.listener_fd, targs.handler)) < 0)
					pr_err("[%s] failed to accept_client(): %d", "HTTP", ret);

				pr_out("[%s] accept new %d client(s)", "HTTP", ret);
			} else { // client handling
				if ((ret = receive_request_from_client(clnt_data, targs.handler)) < 0) {
					pr_err("[%s] failed to receive_request_from_client(): %d", "HTTP", ret);
					close(clnt_data->fd);
					destroy_client_data(clnt_data);
				}

				if ((ret = handle_client_data(clnt_data, targs.tid)) < 0) {
					pr_err("[%s] failed to handle_client_data(): %d", "HTTP", ret);
				} else if (ret == 1) {
					pr_out("[%s] receive all the data from the client: %d", "HTTP", clnt_data->fd);
					epoll_handler_unregister(targs.handler, clnt_data->fd);

					queue_enqueue(targs.queue, (struct queue_data) {
							.type = QUEUE_DATA_PTR,
							.ptr = clnt_data
					});
				}
			}
		} while (--nclient > 0);

		pthread_cond_broadcast(&targs.cond);
	}

	return (void *) 0;
}

void *http_consumer_thread(void *args)
{
	struct thread_args targs = *(struct thread_args *)args;
	struct queue_data data;
	struct client_data *clnt_data;
	int ret;

	free(args);

	pr_out("[%d] start producer_thread()", targs.tid);

	while (true)
	{
		data = queue_dequeue(targs.queue);
		pr_out("[%d] deqeueing data", targs.tid);
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

		send(clnt_data->fd, "hello world!", sizeof("hello world!"), 0);

		close(clnt_data->fd);

		destroy_client_data(clnt_data);
	}

	return NULL;
}

struct http_data {
	char *body;
	char *header;
};

struct http_data *http_data_create(void);

int http_make_packet(struct http_data *http_data)
{
	return 0;
}

int http_make_header(int code)
{
	return 0;
}

void http_data_destroy(void)
{
	return 0;
}
