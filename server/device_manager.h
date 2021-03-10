#ifndef DEVICE_MANAGER_H__
#define DEVICE_MANAGER_H__

#define MAX_DEVICE 1024

int register_device(int sock, int id);
int find_device(int id);
int count_devcie(void);

#endif
