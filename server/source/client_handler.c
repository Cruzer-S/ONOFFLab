#include "client_handler.h"

#include "logger.h"

static inline int wait_other_thread(sem_t *sems)
{
	int sval;

	if (sem_wait(&sems[0]) != 0)
		return -1;

	if (sem_wait(&sems[1]) != 0)
		return -2;

	if (sem_post(&sems[1]) != 0)
		return -5;

	if (sem_getvalue(&sems[2], &sval) != 0)
		return -3;

	if (sval != 1)
		return -4;

	return 0;
}

void *client_handler_listener(void *argument)
{
	CListenerDataPtr listener = argument;
	int ret;

	if ((ret = wait_other_thread(listener->sync)) < 0) {
		pr_err("failed to wait_other_thread: %d", ret);
		return (void *) -1;
	}

	pr_out("listener thread start: %d", listener->id);

	return (void *) 0;
}

void *client_handler_worker(void *argument)
{
	CWorkerDataPtr worker = argument;
	int ret;

	if ((ret = wait_other_thread(worker->sync)) < 0) {
		pr_err("failed to wait_other_thread: %d", ret);
		return (void *) -1;
	}

	pr_out("worker thread start: %d", worker->id);

	return (void *) 0;
}

void *client_handler_deliverer(void *argument)
{
	CDelivererDataPtr deliverer = argument;
	int ret;

	if ((ret = wait_other_thread(deliverer->sync)) < 0) {
		pr_err("failed to wait_other_thread: %d", ret);
		return (void *) -1;
	}

	pr_out("deliverer thread start: %d", deliverer->id);

	return (void *) 0;
}
