#include "task_manager.h"

#define BASE_TASK_NAME "task_"

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
		return -1;

	for (int i = 0; i < tm->cur + 1; i++)
		if (!tm->task[i])
			return i;

	return -1;
}

int remove_task(struct task_manager *tm, int id)
{
	char name[100];

	if (!tm->task[id])
		return -1;

	if (remove(task_name(id)) == -1)
		return -2;

	return 0;
}

char *task_name(int id)
{
	static char name[100];

	sprintf(name, "%s%03d", BASE_TASK_NAME, id);

	return name;
}
