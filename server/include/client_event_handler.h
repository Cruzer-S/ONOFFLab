#ifndef CLIENT_EVENT_HANDLER_H__
#define CLIENT_EVENT_HANDLER_H__

#include "socket_manager.h"

typedef void *EventHandler;

enum event_data_type {
	ETYPE_SERVER,
	ETYPE_CLIENT,
	ETYPE_TIMER
};

typedef enum event_data_type EventType;

EventHandler event_handler_create(
		EpollHandler epoll,
		size_t header_size,
		size_t max_body);
int event_handler_register(EventHandler event, int type, ...);
int event_handler_process(EventHandler event, void *ret);
int event_handler_unregister(EventHandler event, void *ret);
int event_handler_destroy(EventHandler event);

#endif
