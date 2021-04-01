#include "device_manager.h"

struct device {
	struct node *head;
	int count;
};

struct node {
	int sock;

	int32_t id;
	char key[DEVICE_KEY_SIZE];

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

bool insert_device(struct device *dev, int sock, int32_t id, uint8_t *key)
{
	struct node *new_node = malloc(sizeof(struct node));
	if (new_node == NULL) return false;

	for (struct node *cur = dev->head;
		 cur != NULL;
		 cur = cur->next)
	{ if (cur->id == id) return false; }

	new_node->id = id;
	new_node->sock = sock;
	new_node->next = dev->head;
	memcpy(new_node->key, key, DEVICE_KEY_SIZE);

	dev->head = new_node;

	dev->count++;

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

bool check_device_key(struct device *dev, uint32_t id, uint8_t *key)
{
	for (struct node *dp = dev->head;
		 dp != NULL;
		 dp = dp->next)
	{
		if (dp->id == id
		 && memcmp(dp->key, key, DEVICE_KEY_SIZE) == 0)
			return true;
	}

	return false;
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
			return dp->sock;
	}

	return -1;
}

void release_device(struct device *dev)
{
	for (struct node *head = dev->head, *next;
		 head != NULL;
		 next = head->next, free(head), head = next) ;
}
