#include "client_handler.h"

#include "logger.h"

void *client_handler_deliverer(void *argument)
{
	struct deliverer_argument arg;
	int ret;

	arg = *(struct deliverer_argument *) argument;

	if ((ret = pthread_barrier_wait(arg.barrier)) != 0) {
		pr_err("failed to pthread_barrier_wait(): %d",
				strerror(ret));
		return (void *) -1;
	}

	pr_out("%s", "client_handler_deliverer() start");

	return (void *) 0;
}

void *client_handler_worker(void *argument)
{
	struct worker_argument arg;
	int ret;

	arg = *(struct worker_argument *) argument;
	
	if ((ret = pthread_barrier_wait(arg.barrier)) != 0) {
		pr_err("failed to pthread_barrier_wait(): %d",
				strerror(ret));
		return (void *) -1;
	}

	pr_out("%s", "client_handler_worker() start");

	return (void *) 0;
}
