#include "task_manager.h"

#define BASE_TASK_NAME "t_"
#define TASK_EXTENSION "gcode"
#define TASK_DIRECTORY "task/"

struct task {
	char *name;
	int order;
};

struct task_manager {
	struct task *task;

	size_t cur;
	size_t max;
};

static int get_fsize(struct dirent *ep)
{
	struct stat sb;

	if (stat(ep->d_name, &sb) != 0)
		return -1;

	return sb.st_size;
}

struct task_manager *create_task_manager(size_t size)
{
	struct task_manager *tm = malloc(sizeof(struct task_manager));
	if (tm == NULL)
		return NULL;

	tm->task = malloc(sizeof(struct task) * size);
	if (tm->task == NULL)
		return NULL;

	for (int i = 0; i < size; i++)
		tm->task[i].order = -1;

	tm->cur = 0;
	tm->max = size;

	return tm;
}

int register_task(struct task_manager *tm, char *name, int size, char *buffer)
{
	return 0;
}
