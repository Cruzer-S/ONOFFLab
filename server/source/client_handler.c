#include "client_handler.h"

#include <stdarg.h>

#include "client_event_handler.h"
#include "logger.h"
/*
void *client_handler_deliverer(void *argument)
{
	struct deliverer_argument arg;
	EventHandler handler;
	int ret;
	void *process_data, *server_data;

	arg = *(struct deliverer_argument *) argument;
	ret = pthread_barrier_wait(arg.barrier);

	if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
		pr_err("failed to pthread_barrier_wait(): %s",
				strerror(ret));
		return (void *) -1;
	}

	handler = event_handler_create(
			arg.epoll, arg.header_size, 
			arg.body_size, arg.timeout,
			arg.sock_data->fd);
	if (handler) {
		pr_err("failed to %s", "event_handler_create()");
		return (void *) -2;
	}

	do {
		char hoststr[128], portstr[128];

		extract_addrinfo(arg.sock_data->ai, hoststr, portstr);
		pr_out("client server running at %s:%s (%d)",
				hoststr, portstr, arg.sock_data->fd);
	} while (false);

	while ((ret = event_handler_process(handler,
					                    &process_data)) > 0)
		if (process_data)
			queue_enqueue(arg.queue, process_data);

	if (ret < 0)
		pr_err("failed to process_event_data(): %d", ret);

	event_handler_destroy(handler);

	return (void *) ((ssize_t) ret);
}

void *client_handler_worker(void *argument)
{
	struct worker_argument arg;
	int ret;

	arg = *(struct worker_argument *) argument;
	ret = pthread_barrier_wait(arg.barrier);
	if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
		pr_err("failed to pthread_barrier_wait(): %s",
				strerror(ret));
		return (void *) -1;
	}

	pr_out("%s", "client_handler_worker() start");

	return (void *) 0;
}

*/
