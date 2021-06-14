#ifndef EVENT_DATA_H__
#define EVENT_DATA_H__

#include "socket_manager.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

enum event_type {
	ETYPE_TIMER,
	ETYPE_LISTENER,
	ETYPE_FOREIGNER,
};

struct event_data {
	int type;

	union {
		struct event_data_listener {
			int fd;
		} listener;

		struct event_data_foreigner {
			int fd;

			uint8_t *header;
			uint8_t *body;

			size_t header_size;
			size_t body_size;

			size_t recv_header;
			size_t recv_body;

			struct event_data *timer;
		} foreigner;

		struct event_data_timer {
			int fd;
			struct event_data *target;
		} timer;
	};
};

struct event_data *event_data_create(int type, ...);
void event_data_destroy(struct event_data *data);

#endif
