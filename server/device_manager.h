#ifndef DEVICE_MANAGER_H__
#define DEVICE_MANAGER_H__

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_DEVICE 1024

struct device;

struct device *init_device(void);

bool insert_device(struct device *dev, int id, int sock);

bool delete_device_id(struct device *dev, int id);
bool delete_device_sock(struct device *dev, int sock);

int find_device_sock(struct device *dev, int id);
int find_device_id(struct device *dev, int sock);

void release_device(struct device *dev);

#endif
