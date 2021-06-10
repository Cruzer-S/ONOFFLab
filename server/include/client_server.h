#ifndef CLIENT_SERVER_H__
#define CLIENT_SERVER_H__

#include <stdint.h>
#include <stdlib.h>
#include <semaphore.h>

#include "hashtab.h"

struct client_server_argument {
	uint16_t port;
	int backlog;

	int worker;
	int deliverer;

	int event;

	size_t header_size;
	size_t body_size;

	size_t timeout;

	Hashtab shared_table;
};

typedef struct client_server_argument ClntServArg;
typedef void *ClntServ;

ClntServ client_server_create(ClntServArg *carg);
int client_server_start(ClntServ clnt_serv);
int client_server_wait(ClntServ clnt_serv);
void client_server_destroy(ClntServ clnt_serv);

#endif
