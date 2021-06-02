#ifndef CLIENT_HANDLER_H__
#define CLIENT_HANDLER_H__

#include "socket_manager.h"
#include "event_data.h"
#include "queue.h"

struct __attribute__((packed)) client_packet {
	uint32_t id;
	uint8_t passwd[32];

	uint8_t request;

	uint8_t quality;

	uint64_t filesize;

	uint32_t checksum;

	uint32_t retval;
	uint8_t response[128];

	uint8_t dummy[1024 - 182];
};

struct consumer_argument {
	struct queue *queue;
	pthread_t real_tid;

	struct hashtab *device_tab;
	int tid;
};

int sever_foreigner(
		struct event_data *foreigner,
		struct epoll_handler *handler);
int handle_foreigner(
		struct event_data *data,
		struct epoll_handler *handler,
		struct queue *queue);
int accept_foreigner(
		int fd,
		struct epoll_handler *handler,
		size_t header_size,
		size_t timeout);
#endif
