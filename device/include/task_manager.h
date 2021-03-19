#ifndef TASK_MANAGER_H__
#define TASK_MANAGER_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>

#define MAX_TASK	1024

struct task_manager;

struct task_manager *create_task_manager(size_t size);
void delete_task_manager(struct task_manager *tm);

int make_task(struct task_manager *tm);
int remove_task(struct task_manager *tm, int id);
int check_task(struct task_manager *tm, int id);

int task_name(int id, char *name);

#endif
