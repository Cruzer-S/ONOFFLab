#ifndef TASK_MANAGER_H__
#define TASK_MANAGER_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_TASK	1024

struct task;

struct task *make_task(size_t size);
bool is_task_full(struct task *task);

int register_task(struct task *task, int id);
int delete_task(struct task *task, int id);
int draw_task(void);
char *task_name(int id);

#endif
