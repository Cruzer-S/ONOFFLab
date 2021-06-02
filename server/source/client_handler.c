#include "client_handler.h"

#include <stdint.h>
#include <stdio.h>

#include "socket_manager.h" 		// recv
#include "event_data.h" 			// struct event_data
#include "queue.h"	 				// queueu_enqueue
#include "logger.h"	 				// pr_err()
#include "utility.h" 				// set_timer(), create_timer()

#define MAX_FILE_SIZE 1024 * 1024 * 10 // 10 MiB

static ssize_t recv_request(int fd, uint8_t *buffer, size_t *readsize, size_t *needsize)
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

static int read_header(struct event_data *data, struct queue *queue)
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

static int read_body(struct event_data *data, struct queue *queue)
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

	queue_enqueue(queue, data);
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

int send_response(int fd, uint32_t retval, uint8_t *response, struct client_packet *packet)
{
	packet->retval = -1;
	sprintf((char *) packet->response, "broken packet: wrong checksum");
	pr_out("%s", packet->response);

	if (send(fd, packet, sizeof(struct client_packet), 0) != 1024)
		return -1;

	return 0;
}

bool verify_checksum(struct client_packet *packet)
{
	uint32_t result = packet->id;

	result ^= packet->filesize;
	result ^= packet->quality;
	result ^= packet->request;

	return (result == packet->checksum);
}

void *client_consumer(void *args)
{
	struct consumer_argument *arg = (struct consumer_argument *) args;
	char *err_str;
	int fd;

	while (true) {
		pr_out("[%d] waiting data from the producer", arg->tid);
		
		void *qdata = queue_dequeue(arg->queue);
		if (qdata == NULL) {
			pr_err("%s", "failed to queue_dequeue()");
			continue;
		}

		struct event_data *edata = qdata;
		struct client_packet *packet;

		packet = (struct client_packet *) edata->foreigner.header;
		fd = edata->foreigner.fd;

		pr_out("[%d] dequeuing packet: %d",
				arg->tid, edata->foreigner.fd);
		pr_out("id: %d", packet->id);
		pr_out("passwd: %s", packet->passwd);
		pr_out("request: %d", packet->request);
		pr_out("quality: %d", packet->quality);
		pr_out("filesize: %zu", packet->filesize);

		if (!verify_checksum(packet)) {
			send_response(fd, -1, (uint8_t *) "broken packet: wrong checksum", packet);
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
