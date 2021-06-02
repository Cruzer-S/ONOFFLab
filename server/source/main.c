#define _POSIX_C_SOURCE 201109L

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "socket_manager.h"
#include "queue.h"
#include "hashtab.h"
#include "utility.h"
#include "event_data.h"
#include "client_handler.h"

#include "logger.h"

struct parameter_data {
	uint16_t port;
	int backlog;
};
/*
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
*/

// int process_event(struct epoll_event *data, struct producer_argument *argument);

// void *device_producer(void *args);
// void *client_producer(void *args);

// void *device_consumer(void *args);

int extract_parameter(struct parameter_data *data, int argc, char **argv, int type);
// int makeup_server(struct producer_argument *arg, int type);
// ===========================================================================================
// =                                   Don't touching it                                     =
// ===========================================================================================
/*
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
		pr_out("timeout, close foreigner session: %d", 
				data->timer.target->foreigner.fd);
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
*/

int extract_parameter(struct parameter_data *data, int argc, char **argv, int type)
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
/*
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

void cleanup_server(struct producer_argument *prod_args, int i)
{
	epoll_handler_destroy(prod_args[i].handler);
	event_data_destroy(prod_args[i].event);
}
void *device_producer(void *args)
{
	return NULL;
}

void *device_consumer(void *args)
{
	return NULL;
}
*/

int main(int argc, char *argv[])
{
	struct producer_argument prod_args[2];
	struct consumer_argument clnt_cons_args[CLIENT_CONSUMER_THREADS];
	struct consumer_argument dev_cons_args[DEVICE_CONSUMER_THREADS];
	int ret;
	FILE *fp;

	fp = fopen("logg.txt", "a+");
	logger_message_redirect(fp);

	for (int i = 0; i < 2; i++, argc -= 2, argv += 2)
		if ((ret = extract_parameter(&prod_args[i].param, argc, argv, i)) < 0)
			pr_crt("failed to extract_parameter(): %d", ret);

	for (int i = 0; i < 2; i++) {
		if ( !(prod_args[i].sock_data = socket_data_create(
						prod_args[i].param.port,
						prod_args[i].param.backlog,
						MAKE_LISTENER_DEFAULT)))
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

	}

	fclose(fp);
	
	return 0;
}
