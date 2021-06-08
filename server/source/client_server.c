#define _POSIX_C_SOURCE 200112L

#include "client_server.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#include "socket_manager.h" 		// recv
#include "queue.h"	 				// queueu_enqueue
#include "logger.h"	 				// pr_err()

#include "client_server.h"
#include "client_handler.h"

#define FILE_LIMIT (1024 * 1024 * 10) // 10 MiB
#define DELIVERER_LIMIT 8
#define WORKER_LIMIT 8
#define EVENT_LIMIT 8192

struct client_server {
	SockData listener;
	Queue queue;

	int dcount;
	int wcount;

	int eprobe;

	size_t timeout;

	size_t header_size;
	size_t body_size;

	CListenerDataPtr ldata;
	CDelivererDataPtr ddata;
	CWorkerDataPtr wdata;

	pthread_mutex_t sync;
};

typedef struct client_server *ServData;

static inline int test_and_set(ServData server, ClntServArg *arg)
{
	if (arg->event > EVENT_LIMIT)
		return -1;

	if (arg->deliverer > DELIVERER_LIMIT)
		return -2;

	if (arg->worker > WORKER_LIMIT)
		return -3;

	if (arg->body_size > FILE_LIMIT)
		return -4;

	server->eprobe = arg->event;
	server->dcount = arg->deliverer;
	server->wcount = arg->worker;
	server->body_size = arg->body_size;
	server->header_size = arg->header_size;
	server->timeout = arg->timeout;

	return 0;
}

static inline void destroy_epoll(ServData server)
{
	CListenerDataPtr listener = server->ldata;
	CDelivererDataPtr deliverer = server->ddata;

	int cnt = server->dcount;
	int event = server->eprobe / cnt;

	for (int i = 0; i < cnt; i++) {
		epoll_handler_destroy(deliverer[i].epoll);
	}

	epoll_handler_destroy(listener->listener);
	free(listener->deliverer);
}

static inline int epoll_create_arg(ServData server)
{
	CListenerDataPtr listener = server->ldata;
	CDelivererDataPtr deliverer = server->ddata;

	int cnt = server->dcount;
	int event = server->eprobe / cnt;

	listener->deliverer = malloc(sizeof(EpollHandler) * cnt);
	if (listener->deliverer == NULL)
		return -1;

	listener->listener = epoll_handler_create(event);
	if (listener->listener == NULL) {
		free(listener->deliverer);
		return -2;
	}

	for (int i = 0; i < cnt; i++) {
		deliverer[i].epoll = epoll_handler_create(event);
		if (deliverer[i].epoll == NULL) {
			for (int j = 0; j < i; j++)
				epoll_handler_destroy(deliverer[i].epoll);

			epoll_handler_destroy(listener->listener);
			free(listener->deliverer);
			return -2;
		}

		listener->deliverer[i] = deliverer[i].epoll;
	}

	return 0;
}

static inline CListenerDataPtr create_listener_data(
		ServData server, CDelivererDataPtr deliverer)
{
	CListenerDataPtr listener;
	int cnt = server->dcount;

	listener = malloc(sizeof(CListenerData));
	if (listener == NULL)
		goto RETURN_NULL;

	listener->listener = epoll_handler_create(server->eprobe);
	if (listener->listener == NULL)
		goto FREE_LISTENER;

	listener->deliverer = malloc(sizeof(EpollHandler) * cnt);
	if (listener->deliverer == NULL)
		goto DESTROY_HANDLER;

	for (int i = 0; i < cnt; i++)
		listener->deliverer[i] = deliverer[i].epoll;

	listener->timeout = server->timeout;
	listener->tid = (pthread_t) 0;

	listener->valid = true;

	return listener;
DESTROY_HANDLER:
	epoll_handler_destroy(listener->listener);
FREE_LISTENER:
	free(listener);
RETURN_NULL:
	return NULL;
}

static inline void destroy_listener_data(CListenerDataPtr listener)
{
	free(listener->deliverer);
	epoll_handler_destroy(listener->listener);
	free(listener);
}

static inline CDelivererDataPtr create_deliverer_data(
		ServData server)
{
	CDelivererDataPtr deliverer;
	int cnt = server->dcount + 1;

	deliverer = malloc(sizeof(CDelivererData) * cnt);
	if (deliverer == NULL)
		goto RETURN_NULL;

	for (int i = 0; i < cnt - 1; i++) {
		deliverer[i].epoll = epoll_handler_create(server->eprobe);
		if (deliverer[i].epoll == NULL) {
			for (int j = 0; j < i; j++)
				epoll_handler_destroy(deliverer[j].epoll);

			goto FREE_DELIVERER;
		}

		deliverer[i].queue = server->queue;

		deliverer[i].tid = (pthread_t) 0;

		deliverer[i].valid = true;
	}

	deliverer[cnt].valid = false;

	return deliverer;
FREE_DELIVERER:
	free(deliverer);
RETURN_NULL:
	return NULL;
}

static inline void destroy_deliverer_data(CDelivererDataPtr deliverer)
{
	for (CDelivererDataPtr ptr = deliverer;
		 ptr->valid == true;
		 ptr++)
		epoll_handler_destroy(ptr->epoll);

	free(deliverer);
}

static inline CWorkerDataPtr create_worker_data(ServData server)
{
	CWorkerDataPtr worker;
	int count = server->wcount + 1;

	worker = malloc(sizeof(CWorkerData) * count);
	if (worker == NULL)
		goto RETURN_NULL;

	for (int i = 0; i < count - 1; i++) {
		worker[i].queue = server->queue;

		worker[i].body_size = server->body_size;
		worker[i].header_size = server->header_size;

		worker[i].tid = (pthread_t) 0;

		worker[i].valid = true;
	}

	worker[count].valid = false;

	return worker;

RETURN_NULL:
	return NULL;
}

static inline void destroy_worker_data(CWorkerDataPtr worker)
{
	free(worker);
}

// =================================================================

ClntServ client_server_create(ClntServArg *arg)
{
	ServData server;
	int ret;

	server = malloc(sizeof(struct client_server));
	if (server == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));
		goto RETURN_NULL;
	}

	if ((ret = test_and_set(server, arg)) < 0) {
		pr_err("failed to test_limit(): %d", ret);
		goto FREE_SERVER;
	}

	if (pthread_mutex_init(&server->sync, NULL) != 0) {
		pr_err("failed to pthread_cond_init(): %s",
				strerror(errno));
		goto FREE_SERVER;
	}

	server->queue = queue_create(QUEUE_MAX_SIZE, true);
	if (server->queue == NULL) {
		pr_err("failed to queue_create(): %s", server->queue);
		goto DESTROY_SYNC;
	}
		
	server->ddata = create_deliverer_data(server);
	if (server->ddata == NULL) {
		pr_err("failed to create_deliverer_data(): %p",
				server->ddata);
		goto QUEUE_DESTROY;
	}

	server->ldata = create_listener_data(server, server->ddata);
	if (server->ldata == NULL) {
		pr_err("failed to create_listener_data(): %p",
				server->ldata);
		goto DESTROY_DELIVERER;
	}

	server->wdata = create_worker_data(server);
	if (server->wdata == NULL) {
		pr_err("failed to create_worker_data(): %p",
				server->wdata);
		goto DESTROY_LISTENER;
	}
	
	return server;
DESTROY_LISTENER:
	destroy_listener_data(server->ldata);
DESTROY_DELIVERER:
	destroy_deliverer_data(server->ddata);
QUEUE_DESTROY:
	queue_destroy(server->queue);
DESTROY_SYNC:
	pthread_mutex_destroy(&server->sync);
FREE_SERVER:
	free(server);
RETURN_NULL:
	return NULL;
}

void client_server_destroy(ClntServ clnt_serv)
{
	ServData server = clnt_serv;

	destroy_worker_data(server->wdata);
	destroy_listener_data(server->ldata);
	destroy_deliverer_data(server->ddata);

	queue_destroy(server->queue);

	free(server);
}

int client_server_wait(ClntServ clnt_serv)
{
	return 0;
}

int client_server_start(ClntServ clnt_serv)
{
	ServData server = clnt_serv;
	CListenerDataPtr listener = server->ldata;
	CWorkerDataPtr worker = server->wdata;
	CDelivererDataPtr deliverer = server->ddata;
	int ret;

	if ((ret = pthread_mutex_lock(&server->sync)) != 0) {
		pr_err("failed to pthread_mutex_lock: %s",
				strerror(ret));
		return -1;
	}

	ret = pthread_create(&listener->tid, NULL,
			             client_handler_listener,
						 listener);
	if (ret != 0) {
		pr_err("failed to pthread_create(): %s",
				strerror(errno));
		return -2;
	}

	for (CWorkerDataPtr ptr = worker; ptr->valid; ptr++) {
		ret = pthread_create(
				&ptr->tid, NULL,
				client_handler_worker,
				ptr);
		if (ret < 0) {
			pr_err("failed to pthread_create(): %s",
					strerror(errno));
			goto FAIL_PTHREAD_CREATE;
		}
	}

	for (CDelivererDataPtr ptr = deliverer; ptr->valid; ptr++) {
		ret = pthread_create(
				&ptr->tid, NULL,
				client_handler_deliverer,
				ptr);
		if (ret < 0) {
			pr_err("failed to pthread_create(): %s",
					strerror(errno));
			goto FAIL_PTHREAD_CREATE;
		}
	}

	pthread_mutex_unlock(&server->sync);
	
	return 0;
FAIL_PTHREAD_CREATE:
	listener->valid = false;
	for (int i = 0; i < server->dcount; i++)
		deliverer[i].valid = false;
	for (int i = 0; i < server->wcount; i++)
		worker[i].valid = false;
	pthread_mutex_unlock(&server->sync);

	pthread_mutex_destroy(

	return -3;
}
