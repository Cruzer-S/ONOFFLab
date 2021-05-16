#ifndef TARGS_H__

#include <stdbool.h>
#include <pthread.h>

struct thread_args {
	struct epoll_handler *handler;
	int listener_fd;
	pthread_cond_t cond;
	struct queue *queue;

	struct client_data *serv_data;

	bool is_barrier;
	int tid;
};

struct thread_args *make_thread_argument(int type, int fd, bool is_producer);
void destroy_thread_argument(struct thread_args *serv_arg);

#endif
