#ifndef TARGS_H__
#define TARGS_H__

#include <stdbool.h>
#include <pthread.h>

enum argument_type {
	TARGS_CLIENT_PRODUCER,
	TARGS_CLIENT_CONSUMER,
	TARGS_DEVICE_PRODUCER,
	TARGS_DEVICE_CONSUMER
};

union thread_args {
	struct epoll_handler *handler;
	int listener_fd;
	pthread_cond_t cond;
	struct queue *queue;

	struct client_data *serv_data;
	enum argument_type type;

	char *name;

	int tid;
	size_t wait_size;
};
/*
struct thread_args *make_thread_argument(int tid, int fd, char *name,
		                                 enum argument_type type, size_t wait_size);
void destroy_thread_argument(struct thread_args *serv_arg);
*/

#endif
