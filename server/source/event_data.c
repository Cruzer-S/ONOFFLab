#include "event_data.h"

#include "utility.h"
#include "logger.h"
#include "queue.h"

#include <sys/timerfd.h>
/*
void event_data_destroy(struct event_data *data)
{
	switch (data->type) {
	case ETYPE_LISTENER:
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
*/
