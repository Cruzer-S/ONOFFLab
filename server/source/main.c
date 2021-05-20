#define _POSIX_C_SOURCE 201109L

#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "socket_manager.h"
#include "queue.h"
#include "utility.h"
#include "logger.h"
#include "client_handler.h"
#include "targs.h"

#define DEFAULT_CLIENT_PORT	1584
#define DEFAULT_DEVICE_PORT	3606
#define DEFAULT_CLIENT_BACKLOG 30
#define DEFAULT_DEVICE_BACKLOG 30

#define DEVICE_PACKET_SIZE 1024
#define CLIENT_PACKET_SIZE 1024

#define CLIENT_CONSUMER_THREADS 3
#define DEVICE_CONSUMER_THREADS 3

#define CLIENT 0
#define DEVICE 1

struct server_data {
	int fd;
	int backlog; 
	uint16_t port;
	size_t packet_size;

	void *(*consumer)(void *);
	void *(*producer)(void *);

	char *name;

	int num_of_consumer;

	struct addrinfo *ai;
};

int accept_client(int serv_sock, struct epoll_handler *handler,
		          bool is_edge, size_t bufsize, size_t bodysize);
int receive_request_from_client(struct client_data *clnt_data, struct epoll_handler *handler);

void *device_producer(void *args);
void *client_producer(void *args);

void *device_consumer(void *args);
void *client_consumer(void *args);

int init_serv_data(struct server_data *data, int argc, char **argv);

int main(int argc, char *argv[])
{
	struct server_data serv_data[2];
	pthread_t tid[CLIENT_CONSUMER_THREADS + 2];
	int ret;

	if (init_serv_data(serv_data, argc, argv) < 0)
		pr_crt("%s", "failed to init_serv_data()");

	for (int i = 0; i < 1; i++)
	{
		// socket, bind, listen, addrinfo
		serv_data[i].fd = make_listener(serv_data[i].port, serv_data[i].backlog,
										&serv_data[i].ai, MAKE_LISTENER_DEFAULT);
		if (serv_data[i].fd < 0)
			pr_crt("make_listener() error: %d", serv_data[i].fd);

		// epoll, clnt_data for server, cond, queue
		struct thread_args *serv_arg;
		serv_arg = make_thread_argument(i, serv_data[i].fd, false, 0);
		if (serv_arg == NULL)
			pr_crt("failed to make_thread_argument: %d", i);

		for (int j = 0; j < serv_data[i].num_of_consumer; j++)
		{
			struct thread_args *prod_args;

			prod_args = make_thread_argument(j + 2, serv_data[i].fd, true, serv_data[i].packet_size);
			if (prod_args == NULL)
				pr_crt("failed to make_thread_argument: %d", i);

			prod_args->cond = serv_arg->cond;
			prod_args->queue = serv_arg->queue;

			if ((ret = pthread_create(&tid[i], NULL, serv_data[i].consumer, prod_args)) != 0)
				pr_crt("failed to pthread_create(): %s", strerror(ret));
		}

		if ((ret = pthread_create(&tid[i], NULL, serv_data[i].producer, serv_arg)) != 0)
			pr_crt("failed to pthread_create(): %s", strerror(ret));

		char hoststr[INET6_ADDRSTRLEN], portstr[10];
		if ((ret = get_addr_from_ai(serv_data[i].ai, hoststr, portstr)) < 0)
			pr_crt("failed to get_addr_from_ai(): %d", ret);

		pr_out("%s server running at %s:%s (backlog %d)",
			   serv_data[i].name, hoststr, portstr, serv_data[i].backlog);
	}

	for (int i = 0; i < CLIENT_CONSUMER_THREADS + 2; i++)
		if ((ret = pthread_join(tid[i], NULL)) != 0)
			pr_crt("failed to pthread_join: %s", strerror(ret));

	return 0;
}

void *client_producer(void *args)
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
				if ((ret = accept_client(targs.listener_fd, targs.handler, false, 1024, 1024)) < 0)
					pr_err("[%s] failed to accept_client(): %d", "HTTP", ret);

				pr_out("[%s] accept new %d client(s)", "HTTP", ret);
			} else { // client handling
				
				/*
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
						.type = QUEUE_DATA_PTR, .ptr = clnt_data
					});
				}
			}
		} while (--nclient > 0);

		pthread_cond_broadcast(&targs.cond);
	}

	return (void *) 0;

	return NULL;
}

void *device_consumer(void *args)
{
	return NULL;
}

void *device_producer(void *args)
{
	return NULL;
}

void *client_consumer(void *args)
{
	return NULL;
}

int accept_client(int serv_sock, struct epoll_handler *handler,
		          bool is_edge, size_t bufsize, size_t bodysize)
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

		if ((clnt_data = make_client_data(clnt_sock, bodysize, bufsize, true)) == NULL) {
			pr_err("failed to make_client_data(): %d", clnt_sock);
			close(clnt_sock);

			return -2;
		}

		if ((ret = epoll_handler_register(
				handler, clnt_sock, clnt_data,
				is_edge ? (EPOLLIN | EPOLLET) : (EPOLLIN))) < 0)
		{
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
/*

struct device_data {
	int fd;
	int device_id;
};

void *client_producer(void *args)
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
				if ((ret = accept_client(targs.listener_fd, targs.handler, false, 1024, 1024)) < 0)
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
						.type = QUEUE_DATA_PTR, .ptr = clnt_data
					});
				}
			}
		} while (--nclient > 0);

		pthread_cond_broadcast(&targs.cond);
	}

	return (void *) 0;
}

void *client_consumer(void *args)
{
	struct thread_args targs = *(struct thread_args *)args;
	struct queue_data queue;
	struct client_data *clnt_data;
	int ret;

	free(args);

	pr_out("[%d] start producer_thread()", targs.tid);

	while (true)
	{
		queue = queue_dequeue(targs.queue);
		pr_out("[%d] deqeueing data (awake)", targs.tid);
		if (queue.type == QUEUE_DATA_ERROR) {
			pr_err("[%d] failed to queue_dequeue(): %s",
					targs.tid, "QUEUE_DATA_ERROR");
			continue;
		}

		if (queue.type == QUEUE_DATA_UNDEF) {
			pr_err("[%d] failed to queue_dequeue(): %s",
					targs.tid, "QUEUE_DATA_UNDEF");
			continue;
		}

		clnt_data = queue.ptr;

		pr_out("clnt_data: %zu / %zu", clnt_data->bsize, clnt_data->busage);

		close(clnt_data->fd);
		destroy_client_data(clnt_data);
	}

	return NULL;
}
*/

int init_serv_data(struct server_data *data, int argc, char **argv)
{
	int ret;

	data[CLIENT].port = DEFAULT_CLIENT_PORT;
	data[CLIENT].backlog = DEFAULT_CLIENT_BACKLOG;
	data[DEVICE].port = DEFAULT_DEVICE_PORT;
	data[DEVICE].backlog = DEFAULT_DEVICE_BACKLOG;

	data[CLIENT].name = "client";
	data[DEVICE].name = "device";

	data[CLIENT].packet_size = CLIENT_PACKET_SIZE;
	data[DEVICE].packet_size = DEVICE_PACKET_SIZE;

	data[CLIENT].consumer = client_consumer;
	data[DEVICE].consumer = device_consumer;

	data[CLIENT].producer = client_producer;
	data[DEVICE].consumer = device_consumer;

	data[CLIENT].num_of_consumer = CLIENT_CONSUMER_THREADS;
	data[DEVICE].num_of_consumer = DEVICE_CONSUMER_THREADS;

	if ((ret = parse_arguments(argc, argv,
		    16, &data[CLIENT].port,		// first argument will be port number of HTTP server (data type: uint16_t)
			16, &data[DEVICE].port,     // second argument will be ...
			 4, &data[CLIENT].backlog,	// third argument will be backlog of HTTP server (data type: int)
			 4, &data[DEVICE].backlog   /* fourth arg... */)) < 0)
	{
		pr_err("failed to parse_arguments(): %d", ret);
		return -1;
	}

	return 0;
}

