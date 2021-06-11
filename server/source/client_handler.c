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
	int ret, fd, client;
	unsigned int i = 0;

	if ((ret = wait_other_thread(listener->sync)) < 0) {
		pr_err("failed to wait_other_thread: %d", ret);
		return (void *) -1;
	}

	pr_out("listener thread start: %d", listener->id);

	while (true) {
		ret = epoll_handler_wait(listener->listener, -1);
		if (ret < 0) {
			pr_err("failed to epoll_handler_wait(): %d", ret);
			continue;
		}

		struct epoll_event *event = epoll_handler_pop(listener->listener);
		if (event == NULL) {
			pr_err("failed to epoll_handler_pop(): %p", event);
			continue;
		} else fd = event->data.fd;

		for (;	(client = accept(fd, NULL, NULL)) != -1;
			 	i++, i %= listener->deliverer_count) {
			ret = epoll_handler_register(
					listener->deliverer[i],
					client,
					NULL,
					EPOLLIN | EPOLLET);
			if (ret < 0) {
				pr_err("failed to epoll_handler_register(): %d",
						ret);
				continue;
			}

			pr_out("foreign client connect to server [fd:%d => deliverer:%d]",
					client, i);
		}

		if (errno != EAGAIN)
			pr_err("failed to accept(): %s",
					strerror(errno));
	}

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
