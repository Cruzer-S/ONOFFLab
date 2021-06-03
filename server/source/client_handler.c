#include "client_handler.h"

#include <stdarg.h>

#include "client_event_data.h"
#include "logger.h"

void event_data_destroy(EventData data)
{
	free(data);
}

void *client_handler_deliverer(void *argument)
{
	struct deliverer_argument arg;
	EventData data;
	int ret;

	arg = *(struct deliverer_argument *) argument;
	ret = pthread_barrier_wait(arg.barrier);

	if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
		pr_err("failed to pthread_barrier_wait(): %s",
				strerror(ret));
		return (void *) -1;
	}

	data = event_data_create(ETYPE_SERVER, arg.sock_data->fd);
	if (data == NULL) {
		pr_err("failed to event_data_create(%d)",
				arg.sock_data->fd);
		return (void *) -2;
	}

	do {
		char hoststr[128], portstr[128];

		extract_addrinfo(arg.sock_data->ai, hoststr, portstr);
		pr_out("client server running at %s:%s (%d)", 
				hoststr, portstr, arg.sock_data->fd);
	} while (false);

	while ((ret = process_event_data(0)) >= 0) ;

	pr_err("failed to process_event_data(): %d", ret);

	event_data_destroy(data);

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
