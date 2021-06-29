#include "device_server.h"

#include <stdlib.h>
#include <pthread.h>

#include "hashtab.h"
#include "socket_manager.h"
#include "logger.h"

struct server_data {
	struct socket_data *sock_data;
	Hashtab table;
	pthread_t pid;
};

static int receive_device_data(struct device_data *dev_data)
{
	if (recv(dev_data->fd, &dev_data->id,
		 sizeof(uint32_t), MSG_DONTWAIT) == -1) {
		pr_err("failed to recv(): %s", strerror(errno));
		return -1;
	}

	if (recv(dev_data->fd, &dev_data->passwd,
		 sizeof(dev_data->passwd), MSG_DONTWAIT) == -1) {
		pr_err("failed to recv(): %s", strerror(errno));
		return -2;
	}
	
	return 0;
}

static void *device_server_running(void *data)
{
	struct server_data *serv_data = data;
	struct device_data *dev_data;
	int clnt_fd, ret;

	while (true) {
		clnt_fd = accept(serv_data->sock_data->fd, NULL, NULL);
		if (clnt_fd == -1) {
			pr_err("failed to accept(): %s", strerror(errno));
			continue;
		}

		dev_data = malloc(sizeof(struct device_data));
		if (dev_data == NULL) {
			pr_err("failed to malloc(): %s", strerror(errno));
			goto CLOSE_CLNT_FD;
		} else dev_data->fd = clnt_fd;

		ret = receive_device_data(dev_data);
		if (ret < 0) {
			pr_err("failed to receive_device_data(): %d", ret);
			goto FREE_DEV_DATA;
		}

		ret = hashtab_insert(serv_data->table, &dev_data->id, dev_data);
		if (ret < 0) {
			pr_err("hashtab_insert() error:  %s", strerror(errno));
			goto FREE_DEV_DATA;
		}

		pr_out("register device: %d:%s (%d)",
			dev_data->id, dev_data->passwd, dev_data->fd);

		continue;
	FREE_DEV_DATA: free(dev_data);
	CLOSE_CLNT_FD: close(clnt_fd);
	}

	return NULL;
}

DevServ device_server_create(uint16_t port, int backlog, Hashtab table)
{
	struct server_data *serv_data;
	struct socket_data *sock_data;

	serv_data = malloc(sizeof(struct device_data));
	if (serv_data == NULL) {
		pr_err("failed to malloc(): %s", strerror(errno));
		return NULL;
	}

	sock_data = socket_data_create(
		port, backlog,
		MAKE_LISTENER_BLOCKING |
		MAKE_LISTENER_DONTREUSE);
	if (sock_data == NULL) {
		free(serv_data);
		return NULL;
	}

	serv_data->table = table;
	serv_data->sock_data = sock_data;

	return serv_data;
}

int device_server_start(DevServ data)
{
	struct server_data *serv_data = data;
	int ret;

	ret = pthread_create(&serv_data->pid, NULL, device_server_running, data);
	if (ret != 0) {
		pr_err("failed to pthread_create(): %s", strerror(errno));
		return -1;
	}

	return 0;
}

int device_server_wait(DevServ data)
{
	struct server_data *serv_data = data;
	void *ret;
	int err, intret;

	if ((err = pthread_join(serv_data->pid, &ret)) != 0) {
		pr_err("failed to pthread_join(): %s", strerror(errno));
		return -1;
	}

	intret = (intptr_t) ret;

	return intret;
}

void device_server_destroy(DevServ Data)
{
	struct server_data *data = Data;

	socket_data_destroy(data->sock_data);
	free(data);
}
