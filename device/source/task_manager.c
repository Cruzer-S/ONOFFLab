#include "task_manager.h"
#include <stdio.h>

#define BASE_TASK_NAME "task_"
#define TASK_EXTENSION "gcd"

struct task_manager {
	bool *task;
	size_t max;
	size_t cur;
};

struct task_manager *create_task_manager(size_t size)
{
	struct task_manager *tm = malloc(sizeof(struct task_manager));
	if (tm == NULL)
		return NULL;

	tm->task = malloc(sizeof(bool) * size);
	if (tm->task == NULL)
		return NULL;

	memset(tm->task, 0x00, sizeof(bool) * size);

	tm->max = size;
	tm->cur = 0;

	return tm;
}

void delete_task_manager(struct task_manager *tm)
{
	free(tm->task);
	free(tm);
}

int make_task(struct task_manager *tm)
{
	if (tm->cur + 1 >= tm->max)
		return -2;

	for (int i = 0; i < tm->cur + 1; i++)
		if (!tm->task[i]) {
			tm->task[i] = true;
			tm->cur++;
			return i;
		}

	return -1;
}

int remove_task(struct task_manager *tm, int id)
{
	char fname[FILENAME_MAX];

	if (!tm->task[id])
		return -1;

	task_name(id, fname);
	if (remove(fname) == -1)
		return -2;

	return 0;
}

int check_task(struct task_manager *tm, int id)
{
	return tm->task[id];
}

int task_name(int id, char *name)
{
	return sprintf(name, "%s%03d.%s", BASE_TASK_NAME, id, TASK_EXTENSION);
}
