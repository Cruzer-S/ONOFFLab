#ifndef PROCESS_HTTP_H__
#define PROCESS_HTTP_H__

#include <stdio.h>
#include <inttypes.h>

struct http_data {
	char *header;
	uint8_t *body;

	size_t header_size;
	size_t body_size;
};

int http_make_header(struct http_data *data, size_t body_size);
int http_make_packet(struct http_data *data, uint8_t *body, size_t body_size);
void http_data_destroy(struct http_data *data);
int http_send_packet(struct http_data *data, int fd);

#endif
