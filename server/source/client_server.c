#define _POSIX_C_SOURCE 200112L

#include "client_server.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

#include "socket_manager.h" 		// recv
#include "queue.h"	 				// queueu_enqueue
#include "logger.h"	 				// pr_err()

#include "client_handler.h"

#define MAX_FILE_SIZE 1024 * 1024 * 10 // 10 MiB

#define MAX_WORKER_THREAD 8

struct server_data {
	EpollHandler handler;
	SockData sock_data;
	Queue *queue;

	pthread_t server_tid;
	pthread_t *worker_tid;

	int worker_count;
	size_t filesize;
};

CServData client_server_create(CARGS *arg)
{
	struct server_data *data;

	data = malloc(sizeof(struct server_data));
	if (data == NULL) {
		pr_err("failed to malloc(struct serv_data): %d",
				strerror(errno));
		goto FAILED_TO_MALLOC_SERV;
	}

	if (arg->worker > MAX_WORKER_THREAD) {
		pr_err("fail: %s", "Too many worker threads to handle");
		goto FAILED_TO_WORKER;
	} else data->worker_count = arg->worker;

	data->worker_tid = malloc(
			sizeof(pthread_t) * data->worker_count);
	if (data->worker_tid == NULL) {
		pr_err("failed to malloc(data->worker_tid): %d",
				strerror(errno));
		goto FAILED_TO_MALLOC_WORKER;
	}

	data->handler = epoll_handler_create(arg->event);
	if (data->handler == NULL) {
		pr_err("failed to %s", "epoll_handler_create()");
		goto FAILED_TO_HANDLER;
	}

	data->queue = queue_create(arg->event, true);
	if (data->queue == NULL) {
		pr_err("failed to %s", "queue_create()");
		goto FAILED_TO_QUEUE;
	}

	data->sock_data = socket_data_create(
			arg->port, arg->backlog,
			MAKE_LISTENER_DEFAULT);
	if (data->sock_data == NULL) {
		pr_err("failed to %s", "socket_data_create()");
		goto FAILED_TO_SOCK;
	}

	data->filesize = arg->filesize;

	return data;
FAILED_TO_SOCK:
	queue_destroy(data->queue);
FAILED_TO_QUEUE:
	epoll_handler_destroy(data->handler);
FAILED_TO_HANDLER:
	free(data->worker_tid);
FAILED_TO_MALLOC_WORKER:
FAILED_TO_WORKER:
	free(data);
FAILED_TO_MALLOC_SERV:
	return NULL;
}

int client_server_start(CServData serv_data)
{
	struct server_data *data = serv_data;
	struct deliverer_argument deliverer;
	struct worker_argument worker;
	pthread_barrier_t barrier;
	int ret;

	ret = pthread_barrier_init(
			&barrier,
			NULL,
			data->worker_count + 1 + 1);
	if (ret != 0) {
		pr_err("failed to pthread_barrier_init(): %s",
				strerror(ret));
		return -1;
	}

	deliverer = (struct deliverer_argument) {
		.handler = data->handler,
		.queue = data->queue,
		.sock_data = data->sock_data,
		.filesize = data->filesize,
		.barrier = &barrier
	};

	worker = (struct worker_argument) {
		.queue = data->queue,
		.barrier = &barrier
	};

	ret = pthread_create(
			&data->server_tid,
			NULL,
			client_handler_deliverer,
			&deliverer);
	if (ret != 0) {
		pthread_barrier_destroy(&barrier);
		pr_err("failed to pthread_create(): %s", strerror(ret));
		return -1;
	}

	for (int i = 0; i < data->worker_count; i++) {
		ret = pthread_create(
				&data->worker_tid[i],
				NULL,
				client_handler_worker,
				&worker);
		if (ret != 0) {
			pr_err("failed to pthread_create: %s", strerror(ret));
			return -2;
		}
	}

	ret = pthread_barrier_wait(&barrier);
	if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD)
		pr_err("failed to pthread_barrier_wait(): %s", 
				strerror(ret));

	pthread_barrier_destroy(&barrier);

	return 0;
}

int client_server_wait(CServData serv_data)
{
	struct server_data *data = serv_data;
	void *ret;

	if (pthread_join(data->server_tid, &ret) != 0)
		return -1;

	pr_out("return value from server: %d", ret);

	for (int i = 0; i < data->worker_count; i++) {
		if (pthread_join(data->worker_tid[i], &ret) != 0)
			return -(2 + i);

		pr_out("return value from worker: %d", ret);
	}

	return 0;
}

void client_server_destroy(CServData serv_data)
{
	struct server_data *data = serv_data;

	socket_data_destroy(data->sock_data);
	queue_destroy(data->queue);
	epoll_handler_destroy(data->handler);
	free(data);
}
