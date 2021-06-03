#include "client_event_handler.h"

#include <stdlib.h>
#include <stdarg.h>

#include "logger.h"

struct event_handler {
	EpollHandler epoll;

	size_t header_size;
	size_t max_body;
};

EventHandler event_data_create(
		EpollHandler epoll,
		size_t header_size,
		size_t max_body)
{
	struct event_handler *handler;

	handler = malloc(sizeof(struct event_handler));
	if (handler == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));
		return NULL;
	}

	handler->epoll = epoll;
	handler->header_size = header_size;
	handler->max_body = max_body;

	return handler;
}

int event_handler_register(EventHandler event, int type, ...)
{
	return 0;
}

int event_handler_process(EventHandler event, void *ret);
int event_handler_unregister(EventHandler event, void *ret);
int event_handler_destroy(EventHandler event);
