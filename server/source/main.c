#define _GNU_SOURCE

#include <stdio.h>		// fprintf, NULL
#include <stdlib.h> 		// exit, EXIT_FAILURE
#include <stdint.h>
#include <signal.h>

#include "client_server.h"
#include "device_server.h"
#include "hashtab.h"
#include "utility.h"
#include "logger.h"

#define CLIENT 0
#define DEVICE 1

#define DEFAULT_CLIENT_PORT 1584
#define DEFAULT_DEVICE_PORT 3606
#define DEFAULT_CLIENT_BACKLOG 30
#define DEFAULT_DEVICE_BACKLOG 30

#define DEFAULT_LOGGER_NAME "logg.txt"

#define SHARED_TABLE_BUCKET_SIZE 128
#define SHARED_TABLE_NODE_COUNT 1024

struct parameter_data {
	uint16_t port[2];
	int backlog[2];
	char *logger;
	int temp;
};

static inline int extract_parameter(
		struct parameter_data *data,
		int argc, 
		char **argv);

static inline void fill_client_server_argument(
		ClntServArg *cserv_arg,
		struct parameter_data *param_data,
		Hashtab shared_table);

static inline int hash_func(void *key);
static inline int comp_func(void *key1, void *key2);

#define ERROR_HANDLING(FMT, ...)			\
	fprintf(stderr, FMT "\n", __VA_ARGS__), 	\
	exit(EXIT_FAILURE)

int main(int argc, char *argv[])
{
	struct parameter_data param_data;

	ClntServArg cserv_arg;
	ClntServ clnt_serv;

	Hashtab shared_table;
	DevServ dev_serv;

	int ret;
	
	if ((ret = extract_parameter(&param_data, argc, argv)) < 0)
		ERROR_HANDLING("failed to extract_parameter(): %d", ret);

	if ((ret = logger_create(param_data.logger)) < 0)
		ERROR_HANDLING("failed to logger_create(): %d", ret);

	shared_table = hashtab_create(
		SHARED_TABLE_BUCKET_SIZE, SHARED_TABLE_NODE_COUNT,
		hash_func, comp_func);
	if (shared_table == NULL)
		ERROR_HANDLING("failed to %s", "hashtab_create()");

	fill_client_server_argument(
		&cserv_arg, &param_data, shared_table);

	dev_serv = device_server_create(
		DEFAULT_DEVICE_PORT, DEFAULT_CLIENT_BACKLOG, shared_table);
	if (ret < 0)
		ERROR_HANDLING("failed to device_server_start(): %d", ret);

	if ((clnt_serv = client_server_create(&cserv_arg)) == NULL)
		ERROR_HANDLING("failed to %s", "client_server_create()");

	if ((ret = device_server_start(dev_serv)) < 0) {
		fprintf(stderr, "failed to device_server_start(): %d", ret);
		ret = EXIT_FAILURE; goto CLEANUP;
	}

	if ((ret = client_server_start(clnt_serv)) < 0) {
		fprintf(stderr, "failed to client_server_start(): %d", ret);
		ret = EXIT_FAILURE; goto CLEANUP;
	}

	if ((ret = device_server_wait(dev_serv)) < 0) {
		fprintf(stderr, "failed to device_server_wait(): %d", ret);
		ret = EXIT_FAILURE; goto CLEANUP;
	}

	if ((ret = client_server_wait(clnt_serv)) < 0) {
		fprintf(stderr, "failed to client_server_wait(): %d", ret);
		ret = EXIT_FAILURE; goto CLEANUP;
	}

	ret = EXIT_SUCCESS;
CLEANUP:device_server_destroy(dev_serv);
	client_server_destroy(clnt_serv);
	hashtab_destroy(shared_table);
	logger_destroy();

	return ret;
}

int extract_parameter(struct parameter_data *data,
		              int argc, char **argv)
{
	int ret;

	data->logger			= DEFAULT_LOGGER_NAME;

	data->port[CLIENT]		= DEFAULT_CLIENT_PORT;
	data->backlog[CLIENT]		= DEFAULT_CLIENT_BACKLOG;

	data->port[DEVICE]		= DEFAULT_DEVICE_PORT;
	data->backlog[DEVICE]		= DEFAULT_DEVICE_BACKLOG;

	if (!(argc == 1 || argc == 5)) {
		fprintf(stderr,
			"usage: %s [logger] [clnt-port] "
			"[clnt-blog] [dev-port] [dev-blog]\n",
			argv[0]);
		return -1;
	}

	ret = parse_arguments(
		argc, argv,
		PARSE_ARGUMENT_CHARPTR, &data->logger,
		PARSE_ARGUMENT_UINT16, &data->port[CLIENT],
		PARSE_ARGUMENT_INT, &data->backlog[CLIENT],
		PARSE_ARGUMENT_UINT16, &data->port[DEVICE],
		PARSE_ARGUMENT_INT, &data->backlog[DEVICE]);
	if (ret < 0) {
		pr_err("failed to parse_arguments(): %d", ret);
		return -2;
	}

	return 0;
}

static inline void fill_client_server_argument(
		ClntServArg *cserv_arg,
		struct parameter_data *param_data,
		Hashtab shared_table)
{
	cserv_arg->port = param_data->port[CLIENT];
	cserv_arg->backlog = param_data->backlog[CLIENT];
	cserv_arg->shared_table = shared_table;

	cserv_arg->deliverer = 8;
	cserv_arg->worker = 4;

	cserv_arg->event = 8192;
	cserv_arg->timeout = 5;

	cserv_arg->header_size = 1024;
	cserv_arg->body_size = 1024 * 1024 * 10;
}

static inline int hash_func(void *key)
{
	return *(uint32_t *) key % 100;
}

static inline int comp_func(void *key1, void *key2)
{
	if (*(uint32_t *) key1 == *(uint32_t *) key2)
		return true;

	return false;
}
