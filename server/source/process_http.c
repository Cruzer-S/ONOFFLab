#define _POSIX_C_SOURCE 200809L

#include "process_http.h"

#include "logger.h"

#include <sys/socket.h>
#include <string.h>
#include <errno.h>

#include <string.h>

int http_send_packet(struct http_data *data, int fd)
{
	if (send(fd, data->header, data->header_size, 0) != data->header_size) {
		pr_err("failed to send(): %s", strerror(errno));
		return -1;
	}

	if (send(fd, data->body, data->body_size, 0) != data->body_size) {
		pr_err("failed to send(): %s", strerror(errno));
		return -2;
	}

	return 0;
}

int http_make_packet(struct http_data *data, uint8_t *body, size_t body_size)
{
	int ret;

	if (http_make_header(data, body_size) < 0) {
		pr_err("failed to http_make_header(): %d", ret);
		return -1;
	}

	data->body = body;

	return 0;
}

int http_make_header(struct http_data *data, size_t body_size)
{
	char *header = malloc(1024 * 1024 * 4);
	char *ptr = header;

	if (ptr == NULL)
		return -1;

	ptr = stpcpy(ptr, "HTTP/1.1 200 OK\r\n");
	ptr = stpcpy(ptr, "Connection: close\r\n");
	ptr = stpcpy(ptr, "Content-Type: application/json\r\n");
	ptr = stpcpy(ptr, "Content-Length: ");
	ptr += sprintf(ptr, "%zu\r\n", body_size);

	ptr = stpcpy(ptr, "\r\n");

	data->header_size = ptr - header;
	data->body_size = body_size;

	data->header = header;
	
	return 0;
}

void http_data_destroy(struct http_data *data)
{
	free(data->header);
}
