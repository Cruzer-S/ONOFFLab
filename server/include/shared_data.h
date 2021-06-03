#ifndef SHARED_DATA_H__
#define SHARED_DATA_H__

#include <stdint.h>		// uint32_t, uint8_t

struct shared_data {
	int fd;
	uint32_t id;
	uint8_t password[32];
};

int shared_data_compare(void *sd1, void *sd2);
int shared_data_hash(void *key);

#endif
