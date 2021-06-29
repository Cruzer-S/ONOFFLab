#ifndef DEVICE_SERVER_H__
#define DEVICE_SERVER_H__

#include <inttypes.h>

#include "hashtab.h"

struct device_data {
	uint32_t id;
	uint8_t passwd[32];
	int fd;

	uint8_t padding[1024 - 36 - sizeof(int)];
} __attribute__((packed));

typedef void *DevServ;

DevServ device_server_create(uint16_t port, int backlog, Hashtab table);
int device_server_start(DevServ data);
int device_server_wait(DevServ data);
void device_server_destroy(DevServ data);

#endif
