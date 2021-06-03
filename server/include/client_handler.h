#ifndef CLIENT_HANDLER_H__
#define CLIENT_HANDLER_H__

#define _POSIX_C_SOURCE  200112L

#include <stdlib.h>
#include <pthread.h>

#include "socket_manager.h"
#include "queue.h"

struct deliverer_argument {
	EpollHandler handler;
	Queue queue;
	SockData sock_data;

	size_t filesize;

	pthread_barrier_t *barrier;
};

struct worker_argument {
	Queue queue;
	pthread_barrier_t *barrier;
};

void *client_handler_deliverer(void *deliverer_argument);
void *client_handler_worker(void *worker_argument);

#endif
