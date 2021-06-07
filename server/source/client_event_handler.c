#include "client_event_handler.h"

#include <stdlib.h>
#include <stdarg.h>

#include "logger.h"

struct event_handler {
	EpollHandler epoll;

	size_t header_size;
	size_t max_body;
	size_t timeout;
	
	int serv_sock;
};

struct event_data {
	int type;
	int fd;
};

EventHandler event_data_create(
		EpollHandler epoll,
		size_t header_size,
		size_t max_body,
		int listener)
{
	struct event_handler *handler;
	struct event_data *data;
	int ret;

	handler = malloc(sizeof(struct event_handler));
	if (handler == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));
		return NULL;
	} else {
		handler->epoll = epoll;
		handler->header_size = header_size;
		handler->max_body = max_body;
	}

	data = malloc(sizeof(struct event_data));
	if (data == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));

		free(handler);
		return NULL;
	}

	if ((ret = epoll_handler_register(
				handler->epoll,
				listener,
				data,
				EPOLLET)) < 0) {
		pr_err("failed to epoll_handler_register(): %d", ret);

		free(data);
		free(handler);
		return NULL;
	}

	return handler;
}

int event_handler_register(EventHandler Handler, int type, ...)
{
	struct event_handler *handler = Handler;
	va_list ap;
	int ret;

	va_start(ap, type);

	while (1) {
		ret = epoll_handler_register(
				handler->epoll,
				va_arg(ap, int),
				va_arg(ap, void *),
				EPOLLIN);
		if (ret < 0) {
			pr_err("failed to epoll_handler_register(): %d",
					strerror(errno));
			va_end(ap);
			return -1;
		}
	}

	va_end(ap);

	return 0;
}

int event_handler_process(EventHandler Handler, void *ret)
{
	struct event_handler *handler = Handler;

	return 0;
}

int event_handler_unregister(EventHandler Handler, void *ret)
{
	struct event_handler *handler = Handler;

	return 0;
}

int event_handler_destroy(EventHandler Handler)
{
	struct event_handler *handler = Handler;

	return 0;
}
