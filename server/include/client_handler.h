#ifndef CLIENT_HANDLER_H__
#define CLIENT_HANDLER_H__

#define _POSIX_C_SOURCE  200112L

#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#include "socket_manager.h"
#include "queue.h"
#include "hashtab.h"

struct client_listener_data {
	pthread_t tid;

	EpollHandler listener;
	int deliverer_count;

	EpollHandler *deliverer;

	int timeout;
	size_t header_size;

	int id;

	sem_t *sync;
};

struct client_deliverer_data {
	pthread_t tid;

	Queue queue;
	EpollHandler epoll;

	int id;

	sem_t *sync;
};

struct client_worker_data {
	pthread_t tid;

	Queue queue;
	Hashtab table;

	int id;
	sem_t *sync;
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
