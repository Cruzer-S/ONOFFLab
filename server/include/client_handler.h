#ifndef CLIENT_HANDLER_H__
#define CLIENT_HANDLER_H__

#define _POSIX_C_SOURCE  200112L

#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#include "socket_manager.h"
#include "queue.h"

struct client_listener_data {
	EpollHandler listener;
	int deliverer_count;

	EpollHandler *deliverer;

	int timeout;

	pthread_t tid;

	sem_t *sync;

	bool valid;
};

struct client_deliverer_data {
	Queue queue;
	EpollHandler epoll;

	pthread_t tid;

	sem_t *sync;

	bool valid;
};

struct client_worker_data {
	Queue queue;

	size_t header_size;
	size_t body_size;

	pthread_t tid;

	sem_t *sync;

	bool valid;
};

typedef struct client_listener_data CListenerData;
typedef struct client_deliverer_data CDelivererData;
typedef struct client_worker_data CWorkerData;

typedef CListenerData *CListenerDataPtr;
typedef CDelivererData *CDelivererDataPtr;
typedef CWorkerData *CWorkerDataPtr;

void *client_handler_listener(void *);
void *client_handler_deliverer(void *);
void *client_handler_worker(void *);

#endif
