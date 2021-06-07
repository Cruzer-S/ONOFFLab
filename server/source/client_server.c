#define _POSIX_C_SOURCE 200112L

#include "client_server.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

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
	EpollHandler *epoll;
	EpollHandler *listener;

	SockData sock;
	Queue queue;

	int deliverer_count;
	int worker_count;

	int event_probe;

	size_t timeout;

	size_t header_size;
	size_t body_size;
};

typedef struct client_server *Server;

static inline int test_and_set(Server server, ClntServArg *arg)
{
	if (arg->event > EVENT_LIMIT)
		return -1;

	if (arg->deliverer > DELIVERER_LIMIT)
		return -2;

	if (arg->worker > WORKER_LIMIT)
		return -3;

	if (arg->body_size > FILE_LIMIT)
		return -4;

	server->event_probe = arg->event;
	server->deliverer_count = arg->deliverer;
	server->worker_count = arg->worker;
	server->body_size = arg->body_size;
	server->header_size = arg->header_size;
	server->timeout = arg->timeout;

	return 0;
}

static inline void destroy_epoll(Server server)
{
	for (int i = 0; i < server->deliverer_count + 1; i++)
		epoll_handler_destroy(server->epoll[i]);

	free(server->epoll);
}

static inline int epoll_create_arg(Server server, ClntServArg *arg)
{
	int cnt = arg->deliverer;
	int event = arg->event / arg->deliverer;

	server->epoll = malloc(sizeof(EpollHandler) * (cnt + 1));
	if (server->epoll == NULL)
		return -1;

	for (int i = 0; i < cnt + 1; i++) {
		server->epoll[i] = epoll_handler_create(event);
		if (server->epoll[i] == NULL) {
			for (int j = 0; j < i; j++)
				epoll_handler_destroy(server->epoll[j]);
			free(server->epoll);
			return -2;
		}
	}

	server->listener = server->epoll[cnt];

	return 0;
}

ClntServ client_server_create(ClntServArg *arg)
{
	Server server;
	int ret;

	server = malloc(sizeof(struct client_server));
	if (server == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));
		goto FAIL_MALLOC;
	}

	if ((ret = test_and_set(server, arg)) < 0) {
		pr_err("failed to test_limit(): %d", ret);
		goto FREE_SERVER;
	}

	if ((ret = epoll_create_arg(server, arg)) < 0) {
		pr_err("failed to initialize_epoll(): %d", ret);
		goto FREE_SERVER;
	}

	server->queue = queue_create(arg->deliverer, true);
	if (server->queue == NULL) {
		pr_err("failed to queue_create(): %p", NULL);
		goto DESTROY_EPOLL;
	}

	server->sock = socket_data_create(
			arg->port, arg->backlog, MAKE_LISTENER_DEFAULT);
	if (server->sock == NULL) {
		pr_err("failed to socket_data_create(): %p", NULL);
		goto QUEUE_DESTROY;
	}

	return server;
QUEUE_DESTROY:
	queue_destroy(server->queue);
DESTROY_EPOLL:
	destroy_epoll(server);
FREE_SERVER:
	free(server);
FAIL_MALLOC:
	return NULL;
}

void client_server_destroy(ClntServ clnt_serv)
{
	Server server = clnt_serv;

	queue_destroy(server->queue);
	destroy_epoll(server);
	free(server);
}

int client_server_wait(ClntServ clnt_serv)
{
	return 0;
}

int client_server_start(ClntServ clnt_serv)
{
	return 0;
}
