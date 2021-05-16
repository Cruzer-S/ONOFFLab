#include "targs.h"

#include "socket_manager.h"
#include "logger.h"
#include "client_handler.h"
#include "queue.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct thread_args *make_thread_argument(int type, int fd, bool is_producer)
{
	int ret;

	struct thread_args *serv_arg = malloc(sizeof(struct thread_args));
	if (serv_arg == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));
		goto FAILED_TO_MALLOC_THREAD_ARGS;
	}

	serv_arg->handler = epoll_handler_create(EPOLL_MAX_EVENTS);
	if (serv_arg->handler == NULL) {
		pr_err("%s", "failed to epoll_handler_create()");
		goto FAILED_TO_CREATE_EPOLL_HANDLER;
	}

	serv_arg->serv_data = malloc(sizeof(struct client_data));
	if (serv_arg->serv_data == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));
		goto FAILED_TO_MALLOC_SERV_DATA;
	} else serv_arg->serv_data->fd = fd;

	if ((ret = epoll_handler_register(serv_arg->handler,
					                  serv_arg->serv_data->fd, serv_arg->serv_data,
									  EPOLLIN)) < 0)
	{
		pr_err("failed to epoll_handler_register(): %d", ret);
		goto FAILED_TO_REGISTER_EPOLL_HANDLER;
	}

	if (!is_producer) {
		if ((ret = pthread_cond_init(&serv_arg->cond, NULL)) != 0) {
			pr_err("failed to pthread_cond_init(): %s", strerror(ret));
			goto FAILED_TO_INIT_COND_VAR;
		}

		serv_arg->queue = queue_create(EPOLL_MAX_EVENTS, &serv_arg->cond);
		if (serv_arg->queue == NULL) {
			pr_err("failed to queue_create(%d)", QUEUE_MAX_SIZE);
			goto FAILED_TO_CREATE_QUEUE;
		}
	}

	serv_arg->listener_fd = fd;
	serv_arg->tid = type;
	
	return serv_arg;

	// Error Handler
FAILED_TO_INIT_BARRIER:
	queue_destroy(serv_arg->queue);

FAILED_TO_CREATE_QUEUE:
	pthread_cond_destroy(&serv_arg->cond);

FAILED_TO_INIT_COND_VAR:
	/* empty statement */ ;

FAILED_TO_REGISTER_EPOLL_HANDLER:
	free(serv_arg->serv_data);

FAILED_TO_MALLOC_SERV_DATA:
	epoll_handler_destroy(serv_arg->handler);

FAILED_TO_CREATE_EPOLL_HANDLER:
	free(serv_arg);

FAILED_TO_MALLOC_THREAD_ARGS:
	return NULL;
}

void destroy_thread_argument(struct thread_args *serv_arg)
{
	queue_destroy(serv_arg->queue);
	pthread_cond_destroy(&serv_arg->cond);
	free(serv_arg->serv_data);
	epoll_handler_destroy(serv_arg->handler);
	free(serv_arg);
}

