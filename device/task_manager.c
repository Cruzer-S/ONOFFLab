#include "task_manager.h"

struct node {
	char *name;
	int id;
};

struct task {
	struct node *tnode;
	struct node *first;
	struct node *last;

	size_t max_size;
	size_t current;
};

struct task *make_task(size_t size);
bool is_task_full(struct task *task);

int register_task(struct task *task, int id);
int delete_task(struct task *task, int id);
int draw_task(void);
char *task_name(int id);
