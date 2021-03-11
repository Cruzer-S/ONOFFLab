#include "device_manager.h"

struct device {
	struct node *head;
	int count;
};

struct node {
	int id;
	int sock;

	struct node *next;
};

struct device *init_device(void)
{
	struct device *dev = malloc(sizeof(struct device));

	if (dev == NULL) return NULL;

	dev->count = 0;
	dev->head = NULL;

	return dev;
}

bool insert_device(struct device *dev, int id, int sock)
{
	struct node *new_node = malloc(sizeof(struct node));
	if (new_node == NULL) return false;

	new_node->id = id;
	new_node->sock = sock;
	new_node->next = dev->head;

	dev->count++;
	dev->head = new_node;

	return true;
}

bool delete_device_id(struct device *dev, int id)
{
	for (struct node *dp = dev->head, *prev = NULL;
		 dp != NULL;
		 prev = dp, dp = dp->next)
	{
		if (dp->id == id) {
			if (prev == NULL) {
				prev = dp->next;
				free(dp);
				dev->head = prev;
			} else {
				prev->next = dp->next;
				free(dp);
			}
		}

		return dev;
	}

	return NULL;
}

bool delete_device_sock(struct device *dev, int sock)
{
	for (struct node *dp = dev->head, *prev = NULL;
		 dp != NULL;
		 prev = dp, dp = dp->next)
	{
		if (dp->sock == sock) {
			if (prev == NULL) {
				prev = dp->next;
				free(dp);
				dev->head = prev;
			} else {
				prev->next = dp->next;
				free(dp);
			}
		}

		return dev;
	}

	return NULL;
}

int find_device_id(struct device *dev, int sock)
{
	for (struct node *dp = dev->head;
		 dp != NULL;
		 dp = dp->next)
	{
		if (dp->sock == sock)
			return dp->id;
	}

	return -1;
}

int find_device_sock(struct device *dev, int id)
{
	for (struct node *dp = dev->head;
		 dp != NULL;
		 dp = dp->next)
	{
		if (dp->id == id)
			return dp->id;
	}

	return -1;
}

void release_device(struct device *dev)
{
	for (struct node *head = dev->head, *next;
		 head != NULL;
		 next = head->next, free(head), head = next) ;
}
