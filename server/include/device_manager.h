#ifndef DEVICE_MANAGER_H__
#define DEVICE_MANAGER_H__

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define MAX_DEVICE 1024

#define stringify(x) #x
#define UNION_LIBRARY(NAME) stringify(u_ ## NAME)

#include UNION_LIBRARY(ipc_manager.h)

struct device;

struct device *init_device(void);

bool insert_device(struct device *dev, int sock, int32_t id, uint8_t *key);

bool delete_device_id(struct device *dev, int id);
bool delete_device_sock(struct device *dev, int sock);

bool check_device_key(struct device *dev, uint32_t id, uint8_t *key);

int find_device_sock(struct device *dev, int id);
int find_device_id(struct device *dev, int sock);

void release_device(struct device *dev);

#endif
