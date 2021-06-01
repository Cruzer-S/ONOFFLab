#define _POSIX_C_SOURCE 201109L

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

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

struct parameter_data {
	uint16_t port;
	int backlog;
};

struct producer_argument {
	struct socket_data *sock_data;
	struct parameter_data param;

	struct epoll_handler *handler;
	struct queue *queue;
	struct event_data *event;

	pthread_t real_tid;
	int tid;

	int timeout;
	size_t header_size;
};

struct consumer_argument {
	struct queue *queue;
	pthread_t real_tid;
	int tid;
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

int accept_foreigner(
		int fd,
		struct epoll_handler *handler,
		size_t header_size,
		size_t timeout);
int handle_foreigner(
		struct event_data *data,
		struct epoll_handler *handler,
		struct queue *queue);
int sever_foreigner(
		struct event_data *foreigner,
		struct epoll_handler *handler);

void *device_producer(void *args);
void *client_producer(void *args);

void *device_consumer(void *args);
void *client_consumer(void *args);

int process_event(struct epoll_event *data, struct producer_argument *argument);
bool verify_checksum(struct client_packet *packet);

int initialize_server_data(struct parameter_data *data, int argc, char **argv, int type);

struct event_data *event_data_create(int type, ...);
void event_data_destroy(struct event_data *data);

int makeup_server(struct producer_argument *arg, int type);

int create_timer(void);
int set_timer(int fd, int timeout);

FILE *logger;

void *client_consumer(void *args)
{
	struct consumer_argument *arg = (struct consumer_argument *) args;
	char *err_str;

	while (true) {
		pr_out("[%d] waiting data from the producer", arg->tid);
		struct queue_data qdata = queue_dequeue(arg->queue);
		if (qdata.type == QUEUE_DATA_ERROR) {
			err_str = "QUEUE_DATA_ERROR";
			goto ERROR_DEQUEUE;
		} else if (qdata.type == QUEUE_DATA_UNDEF) {
			err_str = "QEUEUE_DATA_UNDEF";
			goto ERROR_DEQUEUE;
		}

		struct event_data *edata;
		struct client_packet *packet;

		edata = qdata.ptr;
		packet = (struct client_packet *) edata->foreigner.header;

		pr_out("[%d] dequeuing packet: %d",
				arg->tid, edata->foreigner.fd);
		pr_out("id: %d", packet->id);
		pr_out("passwd: %s", packet->passwd);
		pr_out("request: %d", packet->request);
		pr_out("quality: %d", packet->quality);
		pr_out("filesize: %zu", packet->filesize);

		if (!verify_checksum(packet)) {
			pr_out("broken packet: %s", "wrong checksum");
			goto DESTROY_CLIENT;
		}
		
		continue;
DESTROY_CLIENT:
		close(edata->foreigner.fd);
		event_data_destroy(edata);
		continue;

ERROR_DEQUEUE:
		pr_err("failed to queue_dequeue(): %s", err_str);
	}

	return NULL;
}

bool verify_checksum(struct client_packet *packet)
{
	uint32_t result = packet->id;

	result ^= packet->filesize;
	result ^= packet->quality;
	result ^= packet->request;
	result ^= packet->passwd[0];

	return (result == packet->checksum);
}

// ===========================================================================================
// =                                   Don't touching it                                     =
// ===========================================================================================
ssize_t recv_request(int fd, uint8_t *buffer, size_t *readsize, size_t *needsize)
{
	ssize_t recvbyte;
	
	*readsize += (recvbyte = recv(
		fd,
		buffer + *readsize,
		*needsize - *readsize,
		0
	));

	return recvbyte; 
}

int read_header(struct event_data *data, struct queue *queue)
{
	struct event_data_foreigner *frgn;

	ssize_t recvbyte;

	frgn = &data->foreigner;
	if (frgn->header == NULL) {
		frgn->header = malloc(frgn->header_size);

		if (frgn->header == NULL) {
			pr_err("failed to malloc(): %s", strerror(errno));
			return -1;
		}
	}

	recvbyte = recv_request(
		frgn->fd,
		frgn->header,
		&frgn->recv_header,
		&frgn->header_size);
	
	if (recvbyte == -1)
		pr_err("failed to recv(): %s", strerror(errno));
	else if (recvbyte == 0)
		pr_out("session closed: %d", frgn->fd);

	return recvbyte;
}
int read_body(struct event_data *data, struct queue *queue)
{
	struct event_data_foreigner *frgn;
	frgn = &data->foreigner;

	if (frgn->body == NULL) {
		size_t filesize;
		filesize = ((struct client_packet *) frgn->header)->filesize;

		frgn->body_size = filesize;
		frgn->recv_body = 0;

		if (filesize == 0) {
			pr_out("there's no body: %s", "filesize is zero");
			return 0;
		} else if (filesize > MAX_FILE_SIZE) {
			pr_err("file is too big to allocate: %zu", filesize);
			return -1;
		}

		frgn->body = malloc(filesize);
		if (frgn->body == NULL) {
			pr_err("failed to malloc(): %s", strerror(errno));
			return -2;
		}

		if (frgn->timer) {
			struct event_data_timer *timer;
			timer = &frgn->timer->timer;

			timer->fd = create_timer();
			if (timer->fd < 0) {
				pr_err("failed to create_timer(): %d", timer->fd);
				return -3;
			}

			if (set_timer(timer->fd, filesize / (1024 * 1024) + 3) < 0) {
				pr_err("failed to timerfd_settime(): %s", strerror(errno));
				return -4;
			}
		}
	}

	ssize_t recvbyte = recv_request(
		frgn->fd,
		frgn->body,
		&frgn->recv_body,
		&frgn->body_size
	);

	if (recvbyte == -1)
		pr_err("failed to recv(): %s", strerror(errno));
	else if (recvbyte == 0)
		pr_out("session closed: %d", frgn->fd);

	return recvbyte;
}

int sever_foreigner(
		struct event_data *foreigner,
		struct epoll_handler *handler)
{
	struct event_data *timer;

	timer = foreigner->foreigner.timer;
	if (timer) {
		if (epoll_handler_unregister(
				handler, 
				timer->timer.fd) < 0)
			pr_err("failed to epoll_handler_unregister(): %d", timer->timer.fd);
		event_data_destroy(timer);
	}

	if (epoll_handler_unregister(
			handler,
			foreigner->foreigner.fd) < 0)
		pr_err("failed to epoll_handler_unregister(): %d", foreigner->foreigner.fd);
	event_data_destroy(foreigner);

	return 0;
}

int handle_foreigner(struct event_data *data, struct epoll_handler *handler, struct queue *queue)
{
	int recvbytes;
	int ret;

	if(data->foreigner.recv_header < data->foreigner.header_size)
		return read_header(data, queue);

	if (data->foreigner.recv_body < data->foreigner.body_size) {
		recvbytes = read_body(data, queue);
		if (recvbytes == 0)
			if (data->foreigner.body_size == 0)
				goto NOBODY;

		return recvbytes;
	} else recvbytes = 1;

NOBODY:	
	if ((ret = epoll_handler_unregister(handler, data->foreigner.fd)) < 0) {
		pr_err("failed to epoll_handler_unregister(): %d", ret);
		return -1;
	}

	struct event_data *timer = data->foreigner.timer;

	if (timer != NULL) {
		if ((ret = epoll_handler_unregister(
				handler,
				timer->timer.fd)) < 0)
			pr_err("failed to epoll_handler_unregister(): %d", ret);

		event_data_destroy(timer);
	}

	struct queue_data qdata = {
		.type = QUEUE_DATA_PTR,
		.ptr = data
	};

	queue_enqueue(queue, qdata);
	pr_out("enqueuing packet: %d", data->foreigner.fd);

	return recvbytes;
}

int accept_foreigner(
		int fd,
		struct epoll_handler *handler,
		size_t header_size,
		size_t timeout)
{
	int accepted = 0;

	while (true) {
		int frgn_fd = accept(fd, NULL, NULL);
		if (frgn_fd == -1) {
			if (errno == EAGAIN)
				break;

			pr_err("failed to accept(): %s", strerror(errno));
			goto ACCEPT_ERROR;
		} else accepted++;

		struct event_data *frgn_data;
		frgn_data = event_data_create(ETYPE_FOREIGNER, frgn_fd, header_size);
		if (frgn_data == NULL) {
			pr_err("failed to event_data_create(%s)",
				   "ETYPE_FOREIGNER");
			goto EDATA_CREATE_FOREIGNER;
		}

		int ret = epoll_handler_register(
				handler,
				frgn_fd,
				frgn_data,
				EPOLLIN | EPOLLET);
		if (ret < 0) {
			pr_err("failed to epoll_handler_register(): %d", ret);
			goto EDATA_REGISTER_FOREGINER;
		}

		struct event_data *timer;
		if (timeout > 0) {
			timer = event_data_create(ETYPE_TIMER, timeout);
			if (timer == NULL) {
				pr_err("failed to event_data_create(%s)",
					   "ETYPE_TIMER");
				goto EDATA_CREATE_TIMER;
			}

			ret = epoll_handler_register(
					handler,
					timer->timer.fd,
					timer,
					EPOLLIN | EPOLLET);
			if (ret < 0) {
				pr_err("failed to epoll_handler_register(): %d", ret);
				goto EDATA_REGISTER_TIMER;
			}

			timer->timer.target = frgn_data;
			frgn_data->foreigner.timer = timer;
		} else frgn_data->foreigner.timer = NULL;

		continue;
EDATA_REGISTER_TIMER:
		if (epoll_handler_unregister(handler, frgn_fd) < 0)
			pr_err("failed to epoll_handler_unregister(): %d", frgn_fd);
EDATA_CREATE_TIMER:
		event_data_destroy(frgn_data);
EDATA_REGISTER_FOREGINER:
		event_data_destroy(timer);
EDATA_CREATE_FOREIGNER:
		close(frgn_fd);
ACCEPT_ERROR:
		continue;
	}

	return accepted;
}

int process_event(struct epoll_event *event, struct producer_argument *argument)
{
	struct event_data *data = event->data.ptr;
	int ret = 0;

	switch (data->type) {
	case ETYPE_LISTENER:
		if ((ret = accept_foreigner(
				data->listener.fd,
				argument->handler,
				argument->header_size,
				argument->timeout)) < 0) {
			pr_err("failed to accept_foreigner(): %d", ret);
			return -1;
		}

		pr_out("accept %d foreigner(s)", ret);
		break;

	case ETYPE_FOREIGNER:
		pr_out("receive data from client: %d", data->foreigner.fd);
		if ((ret = handle_foreigner(data, argument->handler, argument->queue)) <= 0) {
			if (ret != 0)
				pr_err("failed to process_request(): %d", ret);

			sever_foreigner(data, argument->handler);
		}
		break;

	case ETYPE_TIMER:
		pr_out("timeout, close foreigner session: %d", data->timer.fd);
		sever_foreigner(data->timer.target, argument->handler);
		break;
	}

	return ret;
}

void *client_producer(void *argument)
{
	struct epoll_event *event;
	struct producer_argument *arg = argument;
	int nrequest;
	int ret;

	while (true) {	
		pr_out("%s", "Waiting event...");
		nrequest = epoll_handler_wait(arg->handler, -1);
		if (nrequest < 0) {
			pr_err("failed to epoll_handler_wait(): %d", nrequest);
			continue;
		}

		do {
			event = epoll_handler_pop(arg->handler);
			if (!event) {
				pr_err("%s", "failed to epoll_handler_pop()");
				break;
			}

			if ((ret = process_event(event, arg)) < 0)
				pr_err("failed to process_event(): %d", ret);
		} while (--nrequest > 0);
	}

	return NULL;
}

void event_data_destroy(struct event_data *data)
{
	switch (data->type) {
	case ETYPE_LISTENER:
		/* do nothing */
		break;

	case ETYPE_FOREIGNER:
		free(data->foreigner.header);
		free(data->foreigner.body);
		break;

	case ETYPE_TIMER:
		close(data->timer.fd);
		break;
	}

	free(data);
}

struct event_data *event_data_create(int type, ...)
{
	struct event_data *data;
	va_list ap;

	va_start(ap, type);

	if (!(data = malloc(sizeof(struct event_data))) ) {
		pr_err("failed to malloc(): %s", strerror(errno));
		return NULL;
	}

	data->type = type;
	switch (data->type) {
	DECLARATION:;
		size_t header_size;
		size_t timeout;
		int fd;
		int ret;

	case ETYPE_LISTENER:
		fd = va_arg(ap, int);
		data->listener.fd = fd;
		break;

	case ETYPE_FOREIGNER:
		fd = va_arg(ap, int);
		header_size = va_arg(ap, size_t);
		
		data->foreigner.fd = fd;
		data->foreigner.header_size = header_size;

		data->foreigner.body = NULL;
		data->foreigner.header = NULL;
		break;

	case ETYPE_TIMER:
		timeout = va_arg(ap, size_t);

		fd = create_timer();
		if (fd < 0) {
			pr_err("failed to create_timer(): %d", fd);
			free(data);
			return NULL;
		}

		if (timeout > 0) {
			ret = set_timer(fd, timeout);
			if (ret < 0) {
				pr_err("failed to set_timer(%d): %d", fd, ret);
				close(fd);
				free(data);

				return NULL;
			}
		}

		data->timer.fd = fd;
		break;
	}
	
	return data;
}

int initialize_server_data(struct parameter_data *data, int argc, char **argv, int type)
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
	
	ret = parse_arguments(
		argc, argv,
		16, &data->port,
		4, &data->backlog);
	if (ret < 0) {
		pr_err("failed to parse_arguments(): %d", ret);
		return -2;
	}

	return 0;
}

int makeup_server(struct producer_argument *arg, int type)
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

	arg->event = event_data_create(
		ETYPE_LISTENER,		// type
		arg->sock_data->fd,	// event file descriptor
		-1,					// timeout
		0
	);
	if (!arg->event) {
		pr_err("failed to event_data_create(): %s", strerror(errno));
		ret = -4;
		goto FAILED_TO_EVENT_DATA_CREATE;
	}

	if ((ret = epoll_handler_register(
			arg->handler,
			arg->sock_data->fd,
			arg->event,
			EPOLLIN | EPOLLET)) < 0) {
		pr_err("failed to epoll_handler_register(): %d", ret);
		ret = -5;
		goto FAILED_TO_EPOLL_HANDLER_REGISTER;
	}

	if (type == DEVICE) {
		arg->header_size = DEVICE_PACKET_SIZE;
		arg->timeout = DEVICE_FOREIGNER_PACKET_TIMEOUT;
	} else if (type == CLIENT) {
		arg->header_size = CLIENT_PACKET_SIZE;
		arg->timeout = CLIENT_FOREIGNER_PACKET_TIMEOUT;
	} else {
		ret = -6;
		goto INVALID_TYPE_NUMBER;
	}

	return 0;

INVALID_TYPE_NUMBER:
	if (epoll_handler_unregister(arg->handler, arg->sock_data->fd) < 0)
		pr_err("failed to epoll_handler_unregister(): %d", arg->sock_data->fd);

FAILED_TO_EPOLL_HANDLER_REGISTER:
	event_data_destroy(arg->event);

FAILED_TO_EVENT_DATA_CREATE:
	epoll_handler_destroy(arg->handler);

FAILED_TO_EPOLL_HANDLER_CREATE:
	queue_destroy(arg->queue);

FAILED_TO_QUEUE_CREATE:
	return ret;
}

void *device_producer(void *args)
{
	return NULL;
}


void *device_consumer(void *args)
{
	return NULL;
}

int create_timer(void)
{
	int fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if (fd == -1) {
		return -1;
	}

	return fd;
}

void use_logger(void)
{
	logger = fopen("log.txt", "w+");
	if (logger == NULL) {
		fprintf(stderr, "failed to fopen(): %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	setvbuf(logger, NULL, _IONBF, 1);
}

int set_timer(int fd, int timeout)
{
	struct itimerspec rt_tspec = {
		.it_value.tv_sec = timeout
	};

	if (timerfd_settime(fd, 0, &rt_tspec, NULL) == -1) {
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct producer_argument prod_args[2];
	struct consumer_argument clnt_cons_args[CLIENT_CONSUMER_THREADS];
	struct consumer_argument dev_cons_args[DEVICE_CONSUMER_THREADS];
	int ret;

	use_logger();

	for (int i = 0; i < 2; i++, argc -= 2, argv += 2)
		if ((ret = initialize_server_data(&prod_args[i].param, argc, argv, i)) < 0)
			pr_crt("failed to initialize_server_data(): %d", ret);

	for (int i = 0; i < 2; i++) {
		if ( !(prod_args[i].sock_data = socket_data_create(
						prod_args[i].param.port,
						prod_args[i].param.backlog,
						MAKE_LISTENER_DEFAULT)) )
			pr_crt("make_listener() error: %d", prod_args[i].sock_data->fd);

		if ((ret = makeup_server(&prod_args[i], i)) < 0)
			pr_crt("failed to makeup_thread_argument(): %d", ret);
		
		ret = pthread_create(
			&prod_args[i].real_tid,
			NULL,
			(i == CLIENT) ? client_producer : device_producer,
			&prod_args[i]);
		if (ret != 0)
			pr_crt("failed to pthread_create(): %s", strerror(ret));

		struct consumer_argument *arg;
		void *(*func)(void *);
		int cnt;
		int timeout;

		if (i == CLIENT) {
			arg		= clnt_cons_args;
			cnt		= CLIENT_CONSUMER_THREADS;
			func    = client_consumer;
		} else if (i == DEVICE) {
			arg		= dev_cons_args;
			cnt		= DEVICE_CONSUMER_THREADS;
			func	= device_consumer;
		} else pr_crt("invalid type: %d", i);
		
		for (int j = 0; j < cnt; j++) {
			arg[j].tid = j;
			arg[j].queue = prod_args[i].queue;

			ret = pthread_create(
				&arg[j].real_tid,
				NULL,
				func,
				&arg[j]
			);

			if (ret != 0)
				pr_crt("failed to pthread_create(): %s", strerror(ret));
		}

		char hoststr[INET6_ADDRSTRLEN], portstr[10];
		if ((ret = extract_addrinfo(prod_args[i].sock_data->ai, hoststr, portstr)) < 0)
			pr_crt("failed to extract_addrinfo(): %d", ret);

		pr_out("%s server running at %s:%s (backlog %d)",
			   (i == CLIENT) ? "client" : "device",
			   hoststr, portstr,
			   prod_args[i].sock_data->backlog
		);
	}

	for (int i = 0; i < 2; i++) {
		int cnt;
		struct consumer_argument *consumer;

		if (i == CLIENT) {
			cnt      = CLIENT_CONSUMER_THREADS;
			consumer = clnt_cons_args;
		} else if (i == DEVICE) {
			cnt      = DEVICE_CONSUMER_THREADS;
			consumer = dev_cons_args;
		}

		if ((ret = pthread_join(prod_args[i].real_tid, NULL)) != 0)
			pr_crt("failed to pthread_join(): %s", strerror(ret));

		for (int j = 0; j < cnt; j++)
			if ((ret = pthread_join(consumer[j].real_tid, NULL)) != 0)
				pr_crt("failed to pthread_join(): %s", strerror(ret));

		socket_data_destroy(prod_args[i].sock_data);
		queue_destroy(prod_args[i].queue);
		epoll_handler_destroy(prod_args[i].handler);
		event_data_destroy(prod_args[i].event);
	}

#if defined(REDIRECTION)
	fclose(logger);
#endif
	
	return 0;
}

// ===========================================================================================
// =                                   Don't touching it                                     =
// ===========================================================================================
