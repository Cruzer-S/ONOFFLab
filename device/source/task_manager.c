#include "task_manager.h"

#define TASK_EXTENSION		".gcode"
#define TASK_DIRECTORY		"task/"
#define MANAGER_FILE_NAME	"manager.dat"

struct task {
	char name[TASK_NAME_SIZE + 1];
	int order;

	struct task *next;
};

struct task_manager {
	struct task *head;
	struct task *tail;

	FILE *manager;

	int count;
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

	tm->head = tm->tail = NULL;
	tm->count = 0;

	tm->manager = fopen(TASK_DIRECTORY MANAGER_FILE_NAME, "wb+");
	if (tm->manager == NULL)
		return NULL;

	return tm;
}

void delete_task_manager(struct task_manager *tm)
{
	fclose(tm->manager);
	free(tm);
}

int register_task(struct task_manager *tm, char *name, char *buffer, int bsize)
{
	struct task *new_task;
	FILE *tfp;
	char dirname[1024];

	if (tm->count >= MAX_TASK)
		return -1;

	new_task = (struct task *)malloc(sizeof(struct task));
	if (new_task == NULL)
		return -2;

	strcat(strcat(strcpy(
		   dirname,
		   TASK_DIRECTORY),
		   name),
		   TASK_EXTENSION);
	tfp = fopen(dirname, "wb");
	if (tfp == NULL)
		return -3;

	strcpy(new_task->name, name);
	new_task->next = NULL;
	if (tm->head == NULL) {
		new_task->order = 1;
		tm->head = tm->tail = new_task;
	} else {
		new_task->order = tm->tail->order;
		new_task->next = tm->tail;
		tm->tail = new_task;
	}

	if (fwrite(new_task, sizeof(struct task), 1, tm->manager) != 1)
		return -4;

	if (fwrite(buffer, bsize, 1, tfp) != 1)
		return -5;

	tm->count++;

	fclose(tfp);

	return 0;
}
