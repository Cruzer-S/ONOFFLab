#define _POSIX_C_SOURCE 201109L

#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/timerfd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "socket_manager.h"
#include "queue.h"
#include "utility.h"
#include "logger.h"

#define DEFAULT_CLIENT_PORT	1584
#define DEFAULT_DEVICE_PORT	3606
#define DEFAULT_CLIENT_BACKLOG 30
#define DEFAULT_DEVICE_BACKLOG 30

#define DEVICE_PACKET_SIZE 1024
#define CLIENT_PACKET_SIZE 1024

#define CLIENT_CONSUMER_THREADS 3
#define DEVICE_CONSUMER_THREADS 3

#define CLIENT_FOREIGNER_TIMEOUT 3
#define DEVICE_FOREIGNER_TIMEOUT 3

#define CLIENT 0
#define DEVICE 1

enum event_type {
	ETYPE_TIMER,
	ETYPE_LISTENER,
	ETYPE_FOREIGNER,
};

struct event_data {
	int type;
	int timer_fd;
	int event_fd;
};

struct thread_argument {
	struct epoll_handler *handler;
	struct socket_data sock_data;
	int listener_fd;

	pthread_cond_t cond;
	struct queue *queue;

	pthread_t real_tid;
	int tid;
	int timeout;

	size_t packet_size;
};

struct client_packet {
	uint32_t id;
	uint8_t passwd[32];

	uint8_t request;

	uint8_t quality;

	uint64_t filesize;

	uint32_t checksum;
};

int accept_client(int serv_sock, struct epoll_handler *handler, int timeout);
int delete_client(int clnt_sock, struct epoll_handler *handler);

void *device_producer(void *args);
void *client_producer(void *args);

void *device_consumer(void *args);
void *client_consumer(void *args);

int process_request(int sock, size_t header_size, 
	struct epoll_handler *handler, struct queue *queue);

int initialize_server_data(struct socket_data *serv_data, int argc, char **argv, int type);

struct event_data *event_data_create(int type, int fd, int timeout);

int makeup_thread_argument(struct thread_argument *arg, int type);

int main(int argc, char *argv[])
{
	struct thread_argument producer[2];
	struct thread_argument clnt_cons_args[CLIENT_CONSUMER_THREADS];
	struct thread_argument dev_cons_args[DEVICE_CONSUMER_THREADS];
	int ret;

	for (int i = 0; i < 2; i++, argc -= 2, argv += 2)
		if ((ret = initialize_server_data(&producer[i].sock_data, argc, argv, i)) < 0)
			pr_crt("failed to initialize_server_data(): %d", ret);

	for (int i = 0; i < 2; i++) {
		if (make_listener(&producer[i].sock_data, MAKE_LISTENER_DEFAULT) < 0)
			pr_crt("make_listener() error: %d", producer[i].sock_data.fd);

		if ((ret = makeup_thread_argument(&producer[i], i)) < 0)
			pr_crt("failed to makeup_thread_argument(): %d", ret);			
		
		ret = pthread_create(
			&producer[i].real_tid,
			NULL,
			(i == CLIENT) ? client_producer : device_producer,
			&producer[i]);
		if (ret != 0)
			pr_crt("failed to pthread_create(): %s", strerror(ret));

		struct thread_argument *consumer;
		void *(*func)(void *);
		int cnt;
		int timeout;

		if (i == CLIENT) {
			consumer = clnt_cons_args;
			cnt      = CLIENT_CONSUMER_THREADS;
			func     = client_consumer;
		} else if (i == DEVICE) {
			consumer = dev_cons_args;
			cnt      = DEVICE_CONSUMER_THREADS;
			func     = device_consumer;
		} else pr_crt("invalid type: %d", i);
		
		for (int j = 0; j < cnt; j++) {
			consumer[j].tid = j;
			consumer[j].cond = producer[i].cond;
			consumer[j].queue = producer[i].queue;
			consumer[j].timeout = timeout;

			ret = pthread_create(
				&consumer[j].real_tid,
				NULL,
				func,
				&consumer[j]
			);

			if (ret != 0)
				pr_crt("failed to pthread_create(): %s", strerror(ret));
		}

		char hoststr[INET6_ADDRSTRLEN], portstr[10];
		if ((ret = get_addr_from_ai(producer[i].sock_data.ai, hoststr, portstr)) < 0)
			pr_crt("failed to get_addr_from_ai(): %d", ret);

		pr_out("%s server running at %s:%s (backlog %d)", (i == CLIENT) ? 
			   "client" : "device",
			   hoststr, portstr,
			   producer[i].sock_data.backlog
		);
	}

	for (int i = 0; i < 2; i++) {
		int cnt;
		struct thread_argument *consumer;

		if (i == CLIENT) {
			cnt = CLIENT_CONSUMER_THREADS;
			consumer = clnt_cons_args;
		} else if (i == DEVICE) {
			cnt = DEVICE_CONSUMER_THREADS;
			consumer = dev_cons_args;
		}

		if ((ret = pthread_join(producer[i].real_tid, NULL)) != 0)
			pr_crt("failed to pthread_join(): %s", strerror(ret));

		for (int j = 0; j < cnt; j++)
			if ((ret = pthread_join(consumer[j].real_tid, NULL)) != 0)
				pr_crt("failed to pthread_join(): %s", strerror(ret));
	}
	
	return 0;
}

void *client_producer(void *argument)
{
	struct thread_argument arg = *(struct thread_argument *) argument;
	char *header = NULL;
	int clnt_sock, nclient;
	int ret;
	struct epoll_event *event;
	struct event_data *data;

	while (true) {
		if (header == NULL)
			if ( !(header = malloc(arg.packet_size)) )
				pr_err("failed to malloc(): %s", strerror(errno));	

		pr_out("%s", "Waiting event...");
		nclient = epoll_handler_wait(arg.handler, -1);
		if (nclient < 0) {
			pr_err("failed to epoll_handler_wait(): %d", nclient);
			continue;
		}

		do {
			event = epoll_handler_pop(arg.handler);
			if (!event) {
				pr_err("%s", "failed to epoll_handler_pop()");
				break;
			} else data = event->data.ptr;

			if (data->type == ETYPE_LISTENER) {
				ret = accept_client(data->event_fd, arg.handler, arg.timeout);
				if (ret < 0)
					pr_err("failed to accept_client(): %d", ret);

				pr_out("accept new %d client(s)", ret);
			} else if (data->type == ETYPE_FOREIGNER) { // client handling
				clnt_sock = data->event_fd;
				pr_out("receive data from client: %d", clnt_sock);
				if ((ret = process_request(
						clnt_sock, arg.packet_size,
						arg.handler, arg.queue)))
					pr_err("failed to process_request(): %d", ret);
			} else if (data->type == ETYPE_TIMER) {
				pr_out("timeout, close foreigner session: %d", data[-1].event_fd);

				epoll_handler_unregister(arg.handler, data[-1].event_fd);
				epoll_handler_unregister(arg.handler, data[0].event_fd);
				close(data[-1].event_fd);
				close(data[0].event_fd);
			}
		} while (--nclient > 0);

		pthread_cond_broadcast(&arg.cond);
	}

	return NULL;
}

int process_request(int sock, size_t header_size,
	struct epoll_handler *handler, struct queue *queue)
{
	char header[header_size];
	ssize_t read_bytes;

	read_bytes = recv(sock, header, header_size, MSG_PEEK);
	switch (read_bytes) {
	case -1: pr_err("failed to recv(): %s", strerror(errno));
	case  0: pr_out("session closed: %d", sock);
		if (delete_client(sock, handler) < 0) {
			pr_err("%s", "failed to delete_client()");
			return -1;
		}
		break;

	default:
		pr_out("read_bytes / header_size = %zd / %zu", read_bytes, header_size);
		if (read_bytes == header_size) {
			struct queue_data data;

			data.type = QUEUE_DATA_PTR;
			data.ptr = header;
			if (epoll_handler_unregister(handler, sock) < 0) {
				pr_err("%s", "failed to epoll_handler_unregister()");
				close(sock);
				return -2;
			}

			queue_enqueue(queue, data);

			pr_out("enqueuing packet: %d", sock);
		}
		break;
	}

	return 0;
}

void *client_consumer(void *args)
{
	struct thread_argument arg = *(struct thread_argument *) args;
	char *err_str;

	while (true) {
		struct queue_data data = queue_dequeue(arg.queue);
		if (data.type == QUEUE_DATA_ERROR) {
			err_str = "QUEUE_DATA_ERROR";
			goto ERROR_DEQUEUE;
		} else if (data.type == QUEUE_DATA_UNDEF) {
			err_str = "QEUEUE_DATA_UNDEF";
			goto ERROR_DEQUEUE;
		}

		pr_out("%s", (char *) data.ptr);

		continue;

ERROR_DEQUEUE:
		pr_err("failed to queue_dequeue(): %s", err_str);
	}

	return NULL;
}

void *device_consumer(void *args)
{
	return NULL;
}

void *device_producer(void *args)
{
	return NULL;
}

struct event_data *event_data_create(int type, int fd, int timeout)
{
	struct event_data *data;

	if (timeout > 0)
		data = malloc(sizeof(struct event_data) * 2);
	else
		data = malloc(sizeof(struct event_data));

	if (!data)
		return NULL;

	data[0].type = type;
	data[0].event_fd = fd;
	
	if (timeout > 0) {
		struct itimerspec rt_tspec = { .it_value.tv_sec = timeout };
		int timer;

		timer = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
		if (timer == -1) {
			free(data);
			return NULL;
		}

		if (timerfd_settime(timer, 0, &rt_tspec, NULL) == -1) {
			close(timer);
			free(data);
			return NULL;
		}

		data[0].timer_fd = timer;

		data[1].type = ETYPE_TIMER;
		data[1].event_fd = timer;
	} else data[0].timer_fd = -1;

	return data;
}

int accept_client(int serv_sock, struct epoll_handler *handler, int timeout)
{
	int ret;
	int nclient = 0;
	struct event_data *data;
	
	while (true) {
		int clnt_sock = accept(serv_sock, NULL, NULL);
		if (clnt_sock == -1) {
			if (errno == EAGAIN)
				break;

			pr_err("failed to accept(): %s", strerror(errno));
			return -1;
		} else change_nonblocking(clnt_sock);

		data = event_data_create(ETYPE_FOREIGNER, clnt_sock, timeout);
		if (!data) {
			pr_err("failed to malloc(): %s", strerror(errno));
			close(clnt_sock);
			return -2;
		}

		ret = epoll_handler_register(handler, data[0].event_fd, &data[0], EPOLLIN | EPOLLET);
		if (ret < 0) {
			close(clnt_sock);
			free(data);
			pr_err("failed to epoll_handler_register(): %d", ret);
			return -3;
		}

		ret = epoll_handler_register(handler, data[1].event_fd, &data[1], EPOLLIN);
		if (ret < 0) {
			epoll_handler_unregister(handler, data[0].event_fd);
			close(clnt_sock);
			free(data);
			pr_err("failed to epoll_handler_register(): %s", strerror(errno));
			return -4;
		}

		nclient++;
	}

	return nclient;
}

int delete_client(int clnt_sock, struct epoll_handler *handler)
{
	int ret = epoll_handler_unregister(handler, clnt_sock);
	if (ret < 0) {
		pr_err("failed to epoll_handler_unregister(): %d", ret);
		return -1;
	}

	close(clnt_sock);

	return 0;
}

int initialize_server_data(struct socket_data *data, int argc, char **argv, int type)
{
	int ret;

	if (type == CLIENT) {
		data->port = DEFAULT_CLIENT_PORT;
		data->backlog = DEFAULT_CLIENT_BACKLOG;
	} else if (type == DEVICE) {
		data->port = DEFAULT_DEVICE_PORT;
		data->backlog = DEFAULT_DEVICE_BACKLOG;
	} else {
		pr_err("invalid type number: %d", type);
		return -1;
	}
	
	ret = parse_arguments(argc, argv,
		16, &data->port,
		 4, &data->backlog);
	if (ret < 0) {
		pr_err("failed to parse_arguments(): %d", ret);
		return -2;
	}

	return 0;
}

int makeup_thread_argument(struct thread_argument *arg, int type)
{
	int ret;

	if ((ret = pthread_cond_init(&arg->cond, NULL)) != 0) {
		pr_err("failed to pthread_cond_init(): %s", strerror(ret));
		ret = -1;
		goto FAILED_TO_COND_INIT;
	}

	arg->queue = queue_create(QUEUE_MAX_SIZE, &arg->cond);
	if (!arg->queue) {
		pr_err("failed to queue_create(): %s", strerror(errno));
		ret = -2;
		goto FAILED_TO_QUEUE_CREATE;
	}

	arg->handler = epoll_handler_create(EPOLL_MAX_EVENTS);
	if (!arg->handler) {
		pr_err("failed to epoll_handler_create(): %s", strerror(ret));
		ret = -3;
		goto FAILED_TO_EPOLL_HANDLER_CREATE;
	}

	struct event_data *data = event_data_create(ETYPE_LISTENER, arg->sock_data.fd, -1);
	if (!data) {
		pr_err("failed to event_data_create(): %s", strerror(errno));
		ret = -4;
		goto FAILED_TO_EVENT_DATA_CREATE;
	}

	if ((ret = epoll_handler_register(
			arg->handler,
			arg->sock_data.fd,
			data,
			EPOLLIN | EPOLLET)) < 0) {
		pr_err("failed to epoll_handler_register(): %d", ret);
		ret = -5;
		goto FAILED_TO_EPOLL_HANDLER_REGISTER;
	}

	if (type == DEVICE) {
		arg->packet_size = DEVICE_PACKET_SIZE;
		arg->timeout = DEVICE_FOREIGNER_TIMEOUT;
	} else if (type == CLIENT) {
		arg->packet_size = CLIENT_PACKET_SIZE;
		arg->timeout = CLIENT_FOREIGNER_TIMEOUT;
	} else {
		ret = -6;
		goto INVALID_TYPE_NUMBER;
	}

	return 0;

INVALID_TYPE_NUMBER:
	epoll_handler_unregister(arg->handler, arg->sock_data.fd);

FAILED_TO_EPOLL_HANDLER_REGISTER:
	free(data);

FAILED_TO_EVENT_DATA_CREATE:
	epoll_handler_destroy(arg->handler);

FAILED_TO_EPOLL_HANDLER_CREATE:
	queue_destroy(arg->queue);

FAILED_TO_QUEUE_CREATE:
	pthread_cond_destroy(&arg->cond);

FAILED_TO_COND_INIT:
	return ret;
}

