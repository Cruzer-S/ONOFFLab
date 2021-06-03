#ifndef CLIENT_SERVER_H__
#define CLIENT_SERVER_H__

#include <stdint.h>
#include "hashtab.h"

struct client_server_argument {
	uint16_t port;
	int backlog;
	int worker;
	int event;
	int filesize;

	Hashtab shared_table;
};

typedef struct client_server_argument CARGS;
typedef void *CServData;

CServData client_server_create(CARGS *carg);
int client_server_start(CServData serv_data);
int client_server_wait(CServData serv_data);
void client_server_destroy(CServData serv_data);

#endif
