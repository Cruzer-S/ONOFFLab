#ifndef CLIENT_HANDLER_H__
#define CLIENT_HANDLER_H__

#define _POSIX_C_SOURCE  200112L

#include <stdlib.h>
#include <pthread.h>

#include "socket_manager.h"
#include "queue.h"

struct client_listener_data {
	EpollHandler *epolls;
	int deliverer_count;
	int listener;

	int timeout;

	pthread_t tid;
};

struct client_deliverer_data {
	Queue queue;
	EpollHandler epoll;

	pthread_t tid;
};

struct client_worker_data {
	Queue queue;

	pthread_t tid;
};

typedef struct client_listener_data CListenerData;
typedef struct client_deliverer_data CDelivererData;
typedef struct client_worker_data CWorkerData;

#endif
