#define _POSIX_C_SOURCE 200112L

#include "client_server.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <inttypes.h>

#include "socket_manager.h" 		// recv
#include "queue.h"	 				// queueu_enqueue
#include "logger.h"	 				// pr_err()
#include "client_handler.h"			// CListenerData, ...

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

	sem_t synchronizer[3];
};

typedef struct client_server *ServData;
// =====================================================
// Etc. 
// =====================================================
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

static inline void announce_client_server(ServData server)
{
	char hoststr[128], portstr[128];

	extract_addrinfo(server->listener->ai, hoststr, portstr);

	pr_out("client server running at %s:%s (%d)\n"
		   "listener thread: %d\t" "worker thread: %d\t"
		   "deliverer thread: %d",
			hoststr, portstr,
			server->listener->backlog,
			1, server->wcount, server->dcount);
}
// =====================================================
// Listener Data
// =====================================================
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
	listener->sync = server->synchronizer;
	listener->id = 1;
	listener->deliverer_count = cnt;
	listener->header_size = server->header_size;

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
// =====================================================
// Deliverer Data
// =====================================================
static inline CDelivererDataPtr create_deliverer_data(
		ServData server)
{
	CDelivererDataPtr deliverer;
	int cnt = server->dcount;

	deliverer = malloc(sizeof(CDelivererData) * (cnt + 1));
	if (deliverer == NULL)
		goto RETURN_NULL;

	for (int i = 0; i < cnt; i++) {
		deliverer[i].epoll = epoll_handler_create(server->eprobe);
		if (deliverer[i].epoll == NULL) {
			for (int j = 0; j < i; j++)
				epoll_handler_destroy(deliverer[j].epoll);

			goto FREE_DELIVERER;
		}

		deliverer[i].queue = server->queue;
		deliverer[i].tid = (pthread_t) 0;
		deliverer[i].sync = server->synchronizer;
		deliverer[i].id = (i + 1);
	}

	deliverer[cnt].epoll = NULL;

	return deliverer;
FREE_DELIVERER:
	free(deliverer);
RETURN_NULL:
	return NULL;
}

static inline void destroy_deliverer_data(CDelivererDataPtr deliverer)
{
	for (CDelivererDataPtr ptr = deliverer;
		 ptr->epoll != NULL;
		 ptr++)
		epoll_handler_destroy(ptr->epoll);

	free(deliverer);
}
// =====================================================
// Worker Data
// =====================================================
static inline CWorkerDataPtr create_worker_data(ServData server)
{
	CWorkerDataPtr worker;
	int count = server->wcount;

	worker = malloc(sizeof(CWorkerData) * (count + 1));
	if (worker == NULL)
		goto RETURN_NULL;

	for (int i = 0; i < count; i++) {
		worker[i].queue = server->queue;

		worker[i].tid = (pthread_t) 0;
		worker[i].id = (i + 1);

		worker[i].sync = server->synchronizer;
	}

	worker[count].queue = NULL;

	return worker;

RETURN_NULL:
	return NULL;
}

static inline void destroy_worker_data(CWorkerDataPtr worker)
{
	free(worker);
}
// =====================================================
// Semaphore 
// =====================================================
static inline int semaphore_initialize(ServData server)
{
	if (sem_init(&server->synchronizer[0], 
				 -1,
				 server->dcount + server->wcount + 1) != 0)
		return -1;

	if (sem_init(&server->synchronizer[1], -1, 0) != 0) {
		sem_destroy(&server->synchronizer[0]);
		return -2;
	}

	if (sem_init(&server->synchronizer[2], -1, 0) != 0) {
		sem_destroy(&server->synchronizer[0]);
		sem_destroy(&server->synchronizer[1]);
		return -3;
	}

	return 0;
}

static inline void semaphore_destroy(ServData server)
{
	for (int i = 0; i < 3; i++)
		sem_destroy(&server->synchronizer[i]);
}
// =====================================================
// XClient 
// =====================================================
static inline void destroy_xclient_thread(
		void *tid, int count,
		int size, sem_t *sem)
{
	void *ret;
	int func_ret;

	sem_post(sem);

	for (int i = 0; i < count; i++) {
		func_ret = pthread_join(
				*(pthread_t *) (tid + (size * i)), 
				&ret);
		if (func_ret != 0)
			pr_err("failed to pthread_join(): %d",
					i + 1);
	}
}

static inline int start_xclient_thread(
		int count,
		int size,
		void *args,
		void *(*func)(void *),
		sem_t *sem)
{
	int i, ret;
	
	for (i = 0; i < count; i++) {
		ret = pthread_create(
				args + (i * size),
				NULL,
				func,
				args + (i * size));
		if (ret != 0) {
			pr_err("failed to pthread_create(): %s",
					strerror(errno));
			destroy_xclient_thread(
					args, i, size, sem
			);
			return -1;
		}
	}

	return 0;
}

static inline void parse_xclients_data(
		ServData server, int *count,
		int *sizeof_data, void **data,
		void *(**func)(void *))
{
	CListenerDataPtr listener = server->ldata;
	CWorkerDataPtr worker = server->wdata;
	CDelivererDataPtr deliverer = server->ddata;

	int err;

	sizeof_data[0] = sizeof(CListenerData);
	sizeof_data[1] = sizeof(CWorkerData);
	sizeof_data[2] = sizeof(CDelivererData);
	
	data[0] = listener;
	data[1] = worker;
	data[2] = deliverer;

	count[0] = 1;
	count[1] = server->wcount;
	count[2] = server->dcount;

	func[0] = client_handler_listener;
	func[1] = client_handler_worker;
	func[2] = client_handler_deliverer;
}

static inline int wait_xclient_thread(sem_t *sync, int timeout)
{
	for (	clock_t start = clock(), end = clock();
		 	end - start < CLOCKS_PER_SEC * timeout;
		 	end = clock()) {
		int sval = -1;
		if (sem_getvalue(&sync[0], &sval) != 0)
			return -1;

		if (sval == 0) {
			sem_post(&sync[2]);
			sem_post(&sync[1]);
			return 0;
		}
	}

	return -2;
}
// =====================================================
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

	if ((ret = semaphore_initialize(server)) < 0) {
		pr_err("failed to semaphore_initialize(): %d", ret);
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
		goto DESTROY_DDATA;
	}

	server->wdata = create_worker_data(server);
	if (server->wdata == NULL) {
		pr_err("failed to create_worker_data(): %p",
				server->wdata);
		goto DESTROY_LDATA;
	}

	server->listener = socket_data_create(
			arg->port, arg->backlog, MAKE_LISTENER_DEFAULT);
	if (server->listener == NULL)
		goto DESTROY_WDATA;

	return server;
DESTROY_WDATA:
	destroy_worker_data(server->wdata);
DESTROY_LDATA:
	destroy_listener_data(server->ldata);
DESTROY_DDATA:
	destroy_deliverer_data(server->ddata);
QUEUE_DESTROY:
	queue_destroy(server->queue);
DESTROY_SYNC:
	semaphore_destroy(server);
FREE_SERVER:
	free(server);
RETURN_NULL:
	return NULL;
}

int client_server_wait(ClntServ clnt_serv)
{
	ServData server = clnt_serv;
	void *ret;
	int err;

	if (pthread_join(server->ldata->tid, &ret) != 0) {
		pr_err("failed to pthread_join(): %s", strerror((intptr_t) ret));
		err = -1;
	} else pr_out("listener thread return: %" PRIdPTR, (intptr_t) ret);

	for (int i = 0; i < server->wcount; i++) {
		if (pthread_join(server->wdata[i].tid, &ret) != 0) {
			pr_err("failed to pthread_join(): %s", strerror((intptr_t) ret));
			err = -2;
		} else pr_out("worker thread return: %" PRIdPTR, (intptr_t) ret);
	}

	for (int i = 0; i < server->dcount; i++) {
		if (pthread_join(server->ddata[i].tid, &ret) != 0) {
			pr_err("failed to pthread_join(): %s", strerror((intptr_t) ret));
			err = -3;
		} else pr_out("deliverer thread return: %" PRIdPTR, (intptr_t) ret);
	}
	
	return 0;
}

int client_server_start(ClntServ clnt_serv)
{
	ServData server = clnt_serv;

	int sizeof_data[3];
	void *data[3];
	void *(*func[3])(void *);
	int count[3];
	pthread_t *tid[3];

	int err;

	parse_xclients_data(
		server, count, 
		sizeof_data, 
		data, func
	);

	if ((err = epoll_handler_register(
			server->ldata->listener,
			server->listener->fd,
			NULL,
			EPOLLIN | EPOLLET)) < 0)
		pr_err("failed to epoll_handler_register(): %d",
				err);

	for (int i = 0; i < 3; i++) {
		if (start_xclient_thread(
				count[i], sizeof_data[i],
				data[i], func[i],
				&server->synchronizer[1]) < 0) {
			epoll_handler_unregister(
					server->ldata->listener,
					server->listener->fd);
			pr_err("failed to start_client_xthread(%s)",
					"client_handler_listener");
			return -1;
		}
	}

	if (wait_xclient_thread(server->synchronizer, 3) < 0) {
		epoll_handler_unregister(
					server->ldata->listener,
					server->listener->fd);
		for (int i = 0; i < 3; i++)
			destroy_xclient_thread(
					data[i], count[i], sizeof_data[i],
					&server->synchronizer[1]
			);

		return -2;
	}

	announce_client_server(server);

	return 0;
}

void client_server_destroy(ClntServ clnt_serv)
{
	ServData server = clnt_serv;

	socket_data_destroy(server->listener);

	destroy_worker_data(server->wdata);
	destroy_listener_data(server->ldata);
	destroy_deliverer_data(server->ddata);

	queue_destroy(server->queue);

	semaphore_destroy(server);

	free(server);
}
