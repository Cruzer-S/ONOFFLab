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
	int max;

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
	tm->max = size;

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
	printf("count: %d\n", tm->count);
	for (struct task *cur = tm->head;
		 cur != NULL;
		 cur = cur->next)
		printf("%2d: %s (%d) \n", cur->order, cur->name, cur->quantity);
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
	strncat(strncat(strncpy(
	   dirname,
	   TASK_DIRECTORY, TASK_NAME_SIZE),
	   name, TASK_NAME_SIZE),
	   TASK_EXTENSION, TASK_NAME_SIZE);
}

bool delete_task(struct task_manager *tm, char *name)
{
	char dirname[1024];
	bool is_find = false;

	make_name(dirname, name);

	for (struct task *cur = tm->head, *prev = NULL;
		 cur != NULL;
		 (prev == cur) ? (prev = NULL)
		               : (prev = cur, cur = cur->next))
	{
		if (is_find) {
			cur->order--;
			continue;
		}

		if (!strncmp(cur->name, name, TASK_NAME_SIZE)) {
			if (remove(dirname) < 0)
				return false;

			if (prev == NULL) {
				tm->head = cur->next;
				cur = cur->next;
			} else {
				prev->next = cur->next;
				cur = prev;
			}

			free(cur);
			is_find = true;
		}
	}

	if (is_find) {
		save_task(tm);
		show_task(tm);

		tm->count--;
	}

	return is_find;
}

void delete_task_manager(struct task_manager *tm)
{
	fclose(tm->manager);
	free(tm);
}

int rename_task(struct task_manager *tm, char *fname, char *rname)
{
	for (struct task *cur = tm->head;
		 cur != NULL; cur = cur->next)
	{
		if (!strncmp(cur->name, rname, TASK_NAME_SIZE))
			return -2;
	}

	for (struct task *cur = tm->head;
		 cur != NULL; cur = cur->next)
	{
		if (!strncmp(cur->name, fname, TASK_NAME_SIZE))
		{
			strncpy(cur->name, rname, TASK_NAME_SIZE);
			return 0;
		}
	}

	show_task(tm);

	return -1;
}

int change_task_quantity_and_order(
	struct task_manager *tm,
	char *name, int32_t quantity, int32_t order)
{
	struct task *find = NULL,
				*prev, *cur;

	if (order > tm->count)	return -5;
	if (order < 1)			return -6;

	for (prev = NULL, cur = tm->head;
		 cur != NULL;
		 prev = cur, cur = cur->next)
	{
		if (find) {
			cur->order--;
			continue;
		}

		if (!strncmp(cur->name, name, TASK_NAME_SIZE)) {
			if (prev != NULL) prev->next = cur->next;
			else tm->head = cur->next;

			find = cur;
		}
	}

	if (find == NULL) return -2;

	find->order = (order < 0) ? find->order : order;
	find->quantity = (quantity < 0) ? find->quantity : quantity;
	find->next = NULL;

	for (prev = NULL, cur = tm->head;
		 cur != NULL;
		 prev = cur, cur = cur->next)
	{
		if (find->next) {
			cur->order++;
			continue;
		}

		if (cur->order == find->order) {
			cur->order++;

			if (prev != NULL) prev->next = find;
			else tm->head = find;

			find->next = cur;
		}
	}

	if (find->next == NULL) {
		prev->next = find;
		tm->tail = find;
	}

	save_task(tm);
	show_task(tm);

	return 0;
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

	if (tm->count >= tm->max)
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

	strncpy(new_task->name, name, TASK_NAME_SIZE);
	new_task->quantity = quantity;
	new_task->next = NULL;

	if (tm->head == NULL) {
		new_task->order = 1;
		tm->head = tm->tail = new_task;
	} else {
		new_task->order = tm->tail->order + 1;
		tm->tail->next = new_task;
		tm->tail = new_task;
	}

	if (save_task(tm) < 0) {
		fclose(tfp);
		return -6;
	}

	tm->count++;
	show_task(tm);

	return 0;
}

bool search_task(struct task_manager *tm, char *name)
{
	for (struct task *check = tm->head;
		 check != NULL;
		 check = check->next)
	{
		if (!strncmp(check->name, name, TASK_NAME_SIZE))
			return true;
	}

	return false;
}

