#include "task_manager.h"

#define TASK_EXTENSION		".gcode"
#define TASK_DIRECTORY		"task/"
#define MANAGER_FILE_NAME	"manager.dat"

struct task {
	char name[TASK_NAME_SIZE + 1];
	int quantity;
	int order;

	struct task *next;
};

struct task_manager {
	struct task *head;
	struct task *tail;

	int count;

	FILE *manager;
};

static int load_task_manager(struct task_manager *tm);
static int get_fsize(struct dirent *ep);
static bool search_task(struct task_manager *tm, char *name);
static int save_task(struct task_manager *tm);

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

	if ((tm->manager = fopen(TASK_DIRECTORY MANAGER_FILE_NAME, "rb+")) == NULL) {
		tm->manager = fopen(TASK_DIRECTORY MANAGER_FILE_NAME, "wb+");
		if (tm->manager == NULL)
			return NULL;
	}

	load_task_manager(tm);

	return tm;
}

void show_task(struct task_manager *tm)
{
	for (struct task *cur = tm->head;
		 cur != NULL;
		 cur = cur->next)
		printf("%2d: %s\n", cur->order, cur->name);
}

static int load_task_manager(struct task_manager *tm)
{
	struct task *new_task;

	fseek(tm->manager, 0, SEEK_SET);

	while (!feof(tm->manager)) {
		new_task = (struct task *)malloc(sizeof(struct task));
		if (new_task == NULL)
			return -1;

		if (fread(new_task, sizeof(struct task), 1, tm->manager) != 1) {
			free(new_task);
			return -2;
		} else new_task->next = NULL;

		if (tm->head == NULL) {
			tm->head = tm->tail = new_task;
		} else {
			struct task *cur, *prev;

			for (prev = NULL, cur = tm->head;
				 cur != NULL && cur->order < new_task->order;
				 prev = cur, cur = cur->next);

			if (prev == NULL) {
				new_task->next = tm->head;
				tm->head = new_task;
			} else {
				new_task->next = prev->next;
				prev->next = new_task;

				if (cur == NULL)
					tm->tail = new_task;
			}
		}

		tm->count++;
	}

	show_task(tm);

	return tm->count;
}

static void make_name(char *dirname, char *name)
{
	strcat(strcat(strcpy(
	   dirname,
	   TASK_DIRECTORY),
	   name),
	   TASK_EXTENSION);
}

bool delete_task(struct task_manager *tm, char *name)
{
	char dirname[1024];
	bool is_find = false;

	make_name(dirname, name);

	for (struct task *cur = tm->head, *prev = NULL;
		 cur != NULL;
		 prev = cur, cur = cur->next)
	{
		if (is_find) {
			cur->order++;
		} else {
			if (!strcmp(cur->name, name)) {
				if (remove(dirname) < 0)
					return false;

				if (prev == NULL) {
					tm->head = cur;
				} else {
					prev->next = cur->next;
				}

				save_task(tm);
				show_task(tm);

				return true;
			}
		}
	}

	return is_find;
}

void delete_task_manager(struct task_manager *tm)
{
	fclose(tm->manager);
	free(tm);
}

int save_task(struct task_manager *tm)
{
	int k = 0;

	for (struct task *cur = tm->head;
		 cur != NULL;
		 cur = cur->next)
	{
		fseek(tm->manager, (k++) * sizeof(struct task), SEEK_SET);
		ftruncate(fileno(tm->manager), k * sizeof(struct task));
		if (fwrite(cur, sizeof(struct task), 1, tm->manager) != 1)
			return -1;
		fseek(tm->manager, k * sizeof(struct task), SEEK_SET);
	}

	return 0;
}

int register_task(struct task_manager *tm, char *name, int32_t quantity, uint8_t *buffer, int bsize)
{
	struct task *new_task;
	FILE *tfp;
	char dirname[TASK_NAME_SIZE * 2];

	if (tm->count >= MAX_TASK)
		return -1;

	if (search_task(tm, name))
		return -2;

	new_task = (struct task *)malloc(sizeof(struct task));
	if (new_task == NULL)
		return -3;

	make_name(dirname, name);

	tfp = fopen(dirname, "rb");
	if (tfp != NULL) {
		fclose(tfp);
		return -4;
	}

	tfp = fopen(dirname, "wb");
	if (tfp == NULL)
		return -5;

	strcpy(new_task->name, name);
	new_task->next = NULL;
	if (tm->head == NULL) {
		new_task->order = 1;
		tm->head = tm->tail = new_task;
	} else {
		new_task->order = tm->tail->order + 1;
		tm->tail->next = new_task;
		tm->tail = new_task;
	}

	tm->count++;

	if (save_task(tm) < 0)
		return -6;

	fclose(tfp);

	show_task(tm);

	return 0;
}

bool search_task(struct task_manager *tm, char *name)
{
	for (struct task *check = tm->head;
		 check != NULL;
		 check = check->next)
	{
		if (!strcmp(check->name, name))
			return true;
	}

	return false;
}


