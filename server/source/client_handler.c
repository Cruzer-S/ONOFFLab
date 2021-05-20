#include "client_handler.h"

#include "logger.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct client_data *make_client_data(int fd, size_t bodysize, size_t bufsize, bool is_mutex)
{
	struct client_data *clnt_data;

	clnt_data = malloc(sizeof(struct client_data));
	if (clnt_data == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));
		return NULL;
	}

	if (bufsize > 0) {
		clnt_data->buffer = malloc(bufsize);
		if (clnt_data->buffer == NULL) {
			free(clnt_data);
			pr_err("failed to malloc(): %s", strerror(errno));
			return NULL;
		}
	}

	clnt_data->fd = fd;
	clnt_data->busage = 0;
	clnt_data->bsize = bufsize;
	clnt_data->max_body_size = bodysize;

	if (is_mutex) {
		if (pthread_mutex_init(&clnt_data->mutex, NULL) == -1) {
			free(clnt_data->buffer);
			free(clnt_data);
			pr_err("failed_to pthread_mutex_init(): %s", strerror(errno));
			
			return NULL;
		}
	}

	clnt_data->handler = NULL;

	return clnt_data;
}

int handle_client_data(struct client_data *clnt_data, int tid)
{
	char *eoh; //End-of-body
	char *has_body;
	size_t body_len, over_len;

	if (clnt_data->handler != NULL)
		return (clnt_data->bsize == clnt_data->busage);

	eoh = strstr((char *) clnt_data->buffer, "\r\n\r\n");
	if (eoh == NULL) {
		pr_err("failed to strstr()r: %s", "substring not found");
		return -5;
	} else eoh += 4;

	pr_out("[%d] HTTP request from the client: %d", tid, clnt_data->fd);

	// Do NOT call free() function to clnt_data->handler
	// It will be freed by destroy_client_data() function
	clnt_data->handler = malloc(sizeof(struct handler_struct));
	if (clnt_data->handler == NULL) {
		pr_err("[%d] failed to malloc(): %s", tid, strerror(errno));
		return -2;
	} else clnt_data->handler->body = clnt_data->handler->header = NULL;

	if ((has_body = strstr((char *) clnt_data->buffer, "Content-Length: "))) {
		if (sscanf(has_body, "Content-Length: %zu", &body_len) != 1) {
			pr_err("[%d] invalid HTTP Header: %s", tid, "failed to parse Content-Length");
			return -1;
		}

		if (body_len > clnt_data->max_body_size || body_len == 0) {
			pr_err("[%d] request body size is too big/small to receive: %zu / %zu",
					tid, body_len, clnt_data->max_body_size);
			return -4;
		}
		
		clnt_data->handler->body = malloc(sizeof(char) * body_len);
		if (clnt_data->handler->body == NULL) {
			pr_err("[%d] failed to malloc(): %s", tid, strerror(errno));
			return -3;
		}

		clnt_data->handler->header = (char *) clnt_data->buffer;
		clnt_data->buffer = (uint8_t *) clnt_data->handler->body;

		over_len = (clnt_data->handler->header + clnt_data->busage) - eoh;

		strncpy(clnt_data->handler->body, eoh, over_len);

		clnt_data->busage = over_len;
		clnt_data->bsize = body_len;

		pr_out("[%d] HTTP request from the client: %d\n"
			   "body-size: %zu, over-read: %zu", 
			   tid, clnt_data->fd, clnt_data->bsize, clnt_data->busage);
	} else {
		pr_out("[%d] HTTP request from the client: %d\n"
				"header-only, no body", tid, clnt_data->fd);
	}

	return 0;
}

void destroy_client_data(struct client_data *clnt_data)
{
	pthread_mutex_destroy(&clnt_data->mutex);

	if (clnt_data->handler != NULL) {
		free(clnt_data->handler->body);
		free(clnt_data->handler->header);
		free(clnt_data->handler);
		free(clnt_data);
	} else {
		free(clnt_data->buffer);
		free(clnt_data);
	}
}
