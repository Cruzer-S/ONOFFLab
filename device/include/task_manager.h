#ifndef TASK_MANAGER_H__
#define TASK_MANAGER_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_TASK		1024
#define TASK_NAME_SIZE	30

struct task_manager;

struct task_manager *create_task_manager(size_t size);

int register_task(struct task_manager *tm, char *name, char *buffer, int bsize);

#endif
