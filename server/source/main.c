#include <stdio.h>

#include "client_server.h"
#include "hashtab.h"
#include "utility.h"
#include "logger.h"
#include "shared_data.h"

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
};

int extract_parameter(struct parameter_data *data,
		              int argc, char **argv);

int extract_parameter(struct parameter_data *data,
		              int argc, char **argv)
{
	int ret;

	data->logger			= DEFAULT_LOGGER_NAME;

	data->port[CLIENT]		= DEFAULT_CLIENT_PORT;
	data->backlog[CLIENT]	= DEFAULT_CLIENT_BACKLOG;

	data->port[DEVICE]		= DEFAULT_DEVICE_PORT;
	data->backlog[DEVICE]	= DEFAULT_DEVICE_BACKLOG;

	if (!(argc == 1 || argc == 5))
		fprintf(stderr, "usage: %s "
				"[logger]"
				"[clnt-port] [clnt-blog]" 
				"[dev-port] [dev-blog]\n",
				argv[0]);

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

#define ERROR_HANDLING(FMT, ...)			\
	fprintf(stderr, FMT "\n", __VA_ARGS__), \
	exit(EXIT_FAILURE)

int main(int argc, char *argv[])
{
	struct parameter_data param_data;
	struct client_server_argument clnt_serv_arg;
	Hashtab shared_table;
	CServData serv_data;
	int ret;
	
	if ((ret = extract_parameter(&param_data, argc, argv)) < 0)
		ERROR_HANDLING("failed to extract_parameter(): %d", ret);

	if ((ret = logger_create(param_data.logger)) < 0)
		ERROR_HANDLING("failed to logger_create(): %d", ret);

	shared_table = hashtab_create(
		SHARED_TABLE_BUCKET_SIZE, SHARED_TABLE_NODE_COUNT,
		shared_data_hash, shared_data_compare);
	if (shared_table == NULL)
		ERROR_HANDLING("failed to %s", "hashtab_create()");

	clnt_serv_arg.port = param_data.port[CLIENT];
	clnt_serv_arg.backlog = param_data.backlog[CLIENT];
	clnt_serv_arg.shared_table = shared_table;
	clnt_serv_arg.worker = 4;
	clnt_serv_arg.event = 1024;
	clnt_serv_arg.filesize = 1024 * 1024 * 10;

	if ((serv_data = client_server_create(&clnt_serv_arg)) == NULL)
		ERROR_HANDLING("failed to %s", "client_server_create()");

	if ((ret = client_server_start(serv_data)) < 0)
		ERROR_HANDLING("failed to client_server_start(): %d",ret);

	client_server_wait(serv_data);

	client_server_destroy(serv_data);
	hashtab_destroy(shared_table);
	logger_destroy();
	
	return 0;
}
