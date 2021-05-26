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

#define REDIRECTION logger
#include "logger.h"

#define DEFAULT_CLIENT_PORT	1584
#define DEFAULT_DEVICE_PORT	3606
#define DEFAULT_CLIENT_BACKLOG 30
#define DEFAULT_DEVICE_BACKLOG 30

#define DEVICE_PACKET_SIZE 1024
#define CLIENT_PACKET_SIZE 1024

#define CLIENT_CONSUMER_THREADS 3
#define DEVICE_CONSUMER_THREADS 3

#define CLIENT_FOREIGNER_PACKET_TIMEOUT 3
#define DEVICE_FOREIGNER_PACKET_TIMEOUT 3

#define MAX_FILE_SIZE (1024 * 1024 * 10) // 10 MiB

#define CLIENT_FOREIGNER_DATA_TIMEOUT ((MAX_FILE_SIZE) / (1024 * 1024))
#define DEVICE_FOREIGNER_DATA_TIMEOUT ((MAX_FILE_SIZE) / (1024 * 1024))

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

	uint8_t *header;
	uint8_t *body;

	size_t header_size;
	size_t body_size;

	size_t recv_header;
	size_t recv_body;
};

struct thread_argument {
	struct epoll_handler *handler;
	struct socket_data sock_data;
	int listener_fd;

	struct queue *queue;

	pthread_t real_tid;
	int tid;
	int timeout;

	size_t packet_size;
};

struct __attribute__((packed)) client_packet {
	uint32_t id;
	uint8_t passwd[32];

	uint8_t request;

	uint8_t quality;

	uint64_t filesize;

	uint32_t checksum;

	uint8_t dummy[1024 - 50];
};

int accept_client(int serv_sock, struct epoll_handler *handler,
		          int timeout, size_t header_size);
int delete_client(struct epoll_handler *handler, int clnt_sock);

void *device_producer(void *args);
void *client_producer(void *args);

void *device_consumer(void *args);
void *client_consumer(void *args);

int process_event_data(struct event_data *data, struct queue *queue);

int initialize_server_data(struct socket_data *serv_data,
		                   int argc, char **argv, int type);

struct event_data *event_data_create(int type, int fd,
		                             int timeout, size_t header_size);
void event_data_destroy(struct event_data *data, bool timer_exist);

int makeup_thread_argument(struct thread_argument *arg, int type);

FILE *logger;

int main(int argc, char *argv[])
{
	struct thread_argument producer[2];
	struct thread_argument clnt_cons_args[CLIENT_CONSUMER_THREADS];
	struct thread_argument dev_cons_args[DEVICE_CONSUMER_THREADS];
	int ret;

	logger = fopen("log.txt", "w+");
	if (logger == NULL) {
		fprintf(stderr, "failed to fopen(): %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	setvbuf(logger, NULL, _IONBF, 1);

	pr_out("sizeof(struct client_packet): %zu", sizeof(struct client_packet));

	for (int i = 0; i < 2; i++, argc -= 2, argv += 2)
		if ((ret = initialize_server_data(&producer[i].sock_data, argc, argv, i)) < 0)
			pr_crt("failed to initialize_server_data(): %d", ret);

	for (int i = 0; i < 2; i++) {
		// socket, bind, listen, nonblocking, reuseaddr
		if (make_listener(&producer[i].sock_data, MAKE_LISTENER_DEFAULT) < 0)
			pr_crt("make_listener() error: %d", producer[i].sock_data.fd);

		// queue, epoll_handler_create, event_data_create, epoll_handler_register
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

	fclose(logger);
	
	return 0;
}

void *client_producer(void *argument)
{
	struct thread_argument arg = *(struct thread_argument *) argument;
	char *header = NULL;
	int nclient;
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
			int idx;
			event = epoll_handler_pop(arg.handler);
			if (!event) {
				pr_err("%s", "failed to epoll_handler_pop()");
				break;
			} else data = event->data.ptr;

			if (data->type == ETYPE_LISTENER) {
				ret = accept_client(data->event_fd, arg.handler, arg.timeout, arg.packet_size);
				if (ret < 0)
					pr_err("failed to accept_client(): %d", ret);
				else
					pr_out("accept new %d client(s)", ret);
			} else if (data->type == ETYPE_FOREIGNER) { // client handling
				pr_out("receive data from client: %d", data->event_fd);
				if ((ret = process_event_data(data, arg.queue)) <= 0) {
					if (ret != 0)
						pr_err("failed to process_request(): %d", ret);

					idx = 0; goto DESTROY_CLIENT_DATA_STRUCTURE;
				}
			} else if (data->type == ETYPE_TIMER) {
				pr_out("timeout, close foreigner session: %d", data[-1].event_fd);
				idx = -1; goto DESTROY_CLIENT_DATA_STRUCTURE;
			}

			continue; DESTROY_CLIENT_DATA_STRUCTURE:
			delete_client(arg.handler, data[idx].event_fd);
			delete_client(arg.handler, data[idx + 1].event_fd);
			event_data_destroy(&data[idx], true);
		} while (--nclient > 0);
	}

	return NULL;
}

void *client_consumer(void *args)
{
	struct thread_argument arg = *(struct thread_argument *) args;
	char *err_str;

	while (true) {
		pr_out("[%d] waiting data from the producer", arg.tid);
		struct queue_data qdata = queue_dequeue(arg.queue);
		if (qdata.type == QUEUE_DATA_ERROR) {
			err_str = "QUEUE_DATA_ERROR";
			goto ERROR_DEQUEUE;
		} else if (qdata.type == QUEUE_DATA_UNDEF) {
			err_str = "QEUEUE_DATA_UNDEF";
			goto ERROR_DEQUEUE;
		}

		struct event_data *edata = qdata.ptr;
		struct client_packet *packet = (struct client_packet *) edata->header;

		pr_out("[%d] dequeuing packet: %d", arg.tid, edata->event_fd);

		pr_out("id: %d", packet->id);
		pr_out("passwd: %s", packet->passwd);
		pr_out("request: %d", packet->request);
		pr_out("quality: %d", packet->quality);
		pr_out("filesize: %zu", packet->filesize);
		
		continue;

ERROR_DEQUEUE:
		pr_err("failed to queue_dequeue(): %s", err_str);
	}

	return NULL;
}

void *device_producer(void *args)
{
	return NULL;
}

void *device_consumer(void *args)
{
	return NULL;
}

int process_event_data(struct event_data *data, struct queue *queue)
{
	ssize_t recvbyte;
	size_t *readsize;
	size_t *needsize;
	uint8_t *buffer;

	if (data->header == NULL) {
		data->header = malloc(data->header_size);

		if (data->header == NULL)
			return 0;
	}

	if (data->body == NULL) {
		readsize = &data->recv_header;
		needsize = &data->header_size;

		buffer = data->header;
	} else {
		readsize = &data->recv_body;
		needsize = &data->body_size;

		buffer = data->body;
	}

	*readsize += (recvbyte = recv(
		data->event_fd,
		buffer + *readsize,
		*needsize - *readsize,
		0
	));

	switch (recvbyte) {
	case -1: pr_err("failed to recv(): %s", strerror(errno));
	case  0: pr_out("session closed: %d", data->event_fd);
			 break;
	default:
		pr_out("readsize (recvbyte) / needsize = %zu (%zu) / %zu",
			   *readsize, recvbyte, *needsize);
		if (*readsize == *needsize) {
			struct queue_data qdata;

			qdata.type = QUEUE_DATA_PTR;
			qdata.ptr = data;

			if (queue_enqueue(queue, qdata) < 0) {
				pr_err("failed to queue_enqueue(): %s", strerror(errno));
				return -1;
			}

			/*
			size_t filesize = 
				(((struct client_packet *) data)->filesize);


			if ( filesize == 0 ) {
				queue_enqueue(queue, qdata);
				pr_out("enqueuing packet: %d", data->event_fd);
			} else {
				data->body = malloc(filesize);
				if (data->body == NULL)
					pr_err("failed to malloc(): %s", strerror(errno));
			}
			*/
		}
		break;
	}

	return recvbyte;
}

struct event_data *event_data_create(int type, int fd, int timeout, size_t header_size)
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

	data[0].header = NULL;
	data[0].header_size = header_size;

	data[0].body = NULL;
	data[0].body_size = 0;
	
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

void event_data_destroy(struct event_data *data, bool timer_exist)
{
	if (timer_exist)
		close(data[1].timer_fd);

	if (data->header != NULL)
		free(data->header);

	if (data->body != NULL)
		free(data->body);
		
	free(data);
}

int accept_client(int serv_sock, struct epoll_handler *handler, int timeout, size_t header_size)
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

		data = event_data_create(ETYPE_FOREIGNER, clnt_sock, timeout, header_size);
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

int delete_client(struct epoll_handler *handler, int clnt_sock)
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

	arg->queue = queue_create(QUEUE_MAX_SIZE, true);
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

	struct event_data *data = event_data_create(
		ETYPE_LISTENER,		// type
		arg->sock_data.fd,	// event file descriptor
		-1,					// timeout
		0
	);
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
		arg->timeout = DEVICE_FOREIGNER_PACKET_TIMEOUT;
	} else if (type == CLIENT) {
		arg->packet_size = CLIENT_PACKET_SIZE;
		arg->timeout = CLIENT_FOREIGNER_PACKET_TIMEOUT;
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
	return ret;
}

