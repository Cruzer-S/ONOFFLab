#ifndef CLIENT_HANDLER_H__
#define CLIENT_HANDLER_H__

#include <inttypes.h>
#include <stdio.h>
#include <pthread.h>

struct client_data {
	int fd;
	uint8_t *buffer;
	size_t bsize;
	size_t busage;

	size_t max_body_size;

	pthread_mutex_t mutex;

	struct handler_struct {
		char *header;
		char *body;
	} *handler;
};

struct client_data *make_client_data(int fd, int buffer_size, size_t max_body_size);
int handle_client_data(struct client_data *clnt_data, int tid);
void destroy_client_data(struct client_data *clnt_data);

#endif
