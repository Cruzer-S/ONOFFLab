#include "client_handler.h"

#include "logger.h"
#include "utility.h"

#include <inttypes.h>

struct header_data {
	uint32_t id;			// 4
	uint8_t passwd[32];		// 36
	uint8_t method;			// 37
	uint8_t quality;		// 38
	uint32_t filesize;		// 42
	uint32_t checksum;		// 46

	uint8_t dummy[1024 - 46];
} __attribute__((packed));

struct event_data {
	int fd;

	struct header_data header;

	uint8_t *body;
	size_t body_recv;
	size_t header_recv;
	size_t sz_header;
	size_t sz_body;
};

typedef struct event_data *EventData;

// ==========================================================
// =					Private Function					=
// ==========================================================
// Event Data
// ==========================================================
static inline EventData event_data_create(
		int client,
		size_t timeout)
{
	struct event_data *data;

	data = malloc(sizeof(struct event_data) + sizeof(int));
	if (data == NULL)
		return NULL;

	data[1].fd = create_timer();
	if (data[1].fd < 0) {
		free(data);
		return NULL;
	}

	if (set_timer(data[1].fd, timeout) < 0) {
		close(data[1].fd);
		free(data);
	} else data[1].fd = -data[1].fd;

	data[0].body = NULL;
	data[0].body_recv = 0;
	data[0].header_recv = 0;
	data[0].sz_header = sizeof(struct header_data);
	data[0].sz_body = 0;
	data[0].fd = client;

	return data;
}

static inline int event_data_register(
		EpollHandler handler,
		EventData data)
{
	int ret = epoll_handler_register(
			handler,
			data[0].fd,
			&data[0],
			EPOLLIN | EPOLLET);
	if (ret < 0)
		return -1;

	ret = epoll_handler_register(
			handler,
			-data[1].fd,
			&data[1],
			EPOLLIN | EPOLLET);
	if (ret < 0) {
		epoll_handler_unregister(
				handler,
				data[0].fd);
		return -2;
	}

	return 0;
}

static inline void event_data_destroy(
		struct event_data *data, EpollHandler handler)
{
	if (handler) {
		epoll_handler_unregister(handler, data[0].fd);
		epoll_handler_unregister(handler, -data[1].fd);
	}

	close(-data[1].fd);
	free(data);
}
// ==========================================================
// Etc.
// ==========================================================
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
// ==========================================================
// Process Listener
// ==========================================================
static inline int process_listener_data(
		struct epoll_event *event,
		CListenerDataPtr listener,
		// Never use the `trace` as a static variables.
		int *trace)
{
	struct event_data *data;
	int ret, fd, client;
	int timerfd;

	fd = event->data.fd;

	for (;	(client = accept(fd, NULL, NULL)) != -1;
			(*trace)++, *trace %= listener->deliverer_count) {
		if ((ret = change_nonblocking(client)) < 0) {
			pr_err("failed to change_nonblocking(): %d", ret);
			close(client);
			continue;
		}

		data = event_data_create(client, listener->timeout);
		if (data == NULL) {
			pr_err("failed to event_data_create(): %p",
					data);
			close(client);
			continue;
		}

		if ((ret = event_data_register(
					listener->deliverer[*trace], 
					data)) < 0) {
			pr_err("failed to event_data_register(): %d", 
					ret);
			event_data_destroy(data, NULL);
			close(client);
			continue;
		}

		pr_out("foreign client connect to server "
			   "[fd:%d => deliverer:%d]",
				client, *trace);
	}

	if (errno != EAGAIN) {
		pr_err("failed to accept(): %s",
				strerror(errno));
		return -1;
	}

	return 0;
}
// ==========================================================
// Process Deliverer
// ==========================================================
static inline int check_checksum(struct header_data *data)
{
	uint32_t checksum = 0;

	checksum ^= data->id;
	checksum ^= data->passwd[0];
	checksum ^= data->filesize;
	checksum ^= data->quality;
	checksum ^= data->method;

	if (checksum != data->checksum)
		return -1;

	return 0;
}

static inline void sever_deliverer_data(
		EventData data,
		EpollHandler handler,
		int ret)
{
	int real_fd = data->fd < 0 ? data[-1].fd : data->fd;

	if (data->fd < 0)
		pr_out("client %d time-out: close session", real_fd);
	else if (ret == 0)
		pr_out("client %d request closing session", real_fd);
	else
		pr_out("failed to process_deliverer_data(%d): %d",
				real_fd, ret);

	if (data->fd < 0)
		data--;

	free(data->body);

	event_data_destroy(data, handler);
	close(real_fd);
}

static inline void show_deliverer_data(
		const char *str,
		EventData data)
{
	struct header_data *header = &data->header;

	pr_out("%s\n"
		   "id: %"			PRIu32  "\n"
		   "passwd: %"		"s"		"\n"
		   "method: %"		PRIu8	"\n"
		   "quality: %"		PRIu8	"\n"
		   "filesize: %"	PRIu32	"\n"
		   "checksum: %"	PRIu32	,
		   str,
		   header->id,
		   header->passwd,
		   header->method,
		   header->quality,
		   header->filesize,
		   header->checksum);
}

static inline int process_deliverer_data(
		struct epoll_event *event,
		CDelivererDataPtr deliverer)
{
	struct event_data *data = event->data.ptr;

	uint8_t *ptr;
	size_t to_read;
	size_t *received;
	size_t maximum;

	if (data[0].fd < 0)
		return 0;

	while (true) {
		if (data->body == NULL) {
			ptr = (uint8_t *) &data->header 
				            + data->header_recv;
			to_read = data->sz_header - data->header_recv;
			received = &data->header_recv;
			maximum = data->sz_header;
		} else {
			ptr = data->body + data->body_recv;
			to_read = data->sz_body - data->body_recv;
			received = &data->body_recv;
			maximum = data->sz_body;
		}

		int ret = recv(data->fd, ptr, to_read, 0);
		if (ret == -1) {
			if (errno == EAGAIN)
				return 1;

			return -1;
		} else if (ret == 0) {
			return 0;
		} else *received += ret;

		if (maximum != *received)
			continue;

		if (data->body == NULL) {
			if (check_checksum(&data->header) < 0)
				return -2;

			data->body = malloc(data->header.filesize);
			if (data->body == NULL) {
				return -3;
			} else {
				data->body_recv = 0;
				data->sz_body = data->header.filesize;
			}

			show_deliverer_data("received body data", data);
		} else {
			pr_out("received all the data from client: %d",
					data->fd);
		}
	}

	return 1;
}
// ==========================================================
// =					Public Function						=
// ==========================================================
void *client_handler_listener(void *argument)
{
	CListenerDataPtr listener = argument;
	struct epoll_event *event;
	int ret, trace = 0;

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
		} else for (int i = 0; i < ret; i++) {
			event = epoll_handler_pop(listener->listener);

			if (event == NULL) {
				pr_err("failed to epoll_handler_pop(): %p",
						event);
				continue;
			} else ret = process_listener_data(
					event, listener, &trace
			);

			if (ret < 0) {
				pr_err("failed to %s: %d",
						"process_listener_data()", ret);
				continue;
			}
		}
	}

	return (void *) 0;
}

void *client_handler_deliverer(void *argument)
{
	CDelivererDataPtr deliverer = argument;
	struct epoll_event *event;
	int ret;

	if ((ret = wait_other_thread(deliverer->sync)) < 0) {
		pr_err("failed to wait_other_thread: %d", ret);
		return (void *) -1;
	}

	pr_out("deliverer thread start: %d", deliverer->id);

	while (true) {
		ret = epoll_handler_wait(deliverer->epoll, -1);
		if (ret < 0) {
			pr_err("failed to epoll_handler_wait(): %d",
					ret);
			continue;
		} else for (int i = 0; i < ret; i++) {
			event = epoll_handler_pop(deliverer->epoll);

			if (event == NULL) {
				pr_err("failed to epoll_handler_pop(): %p",
						event);
				continue;
			} else ret = process_deliverer_data(
				event, deliverer
			);

			if (ret <= 0) sever_deliverer_data(
				event->data.ptr,
				deliverer->epoll,
				ret
			);
		}
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

