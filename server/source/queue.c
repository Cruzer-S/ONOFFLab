#include "queue.h"

#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

struct node {
	void *data;
	struct node *next;
};

struct queue {
	struct node *tail;

	sem_t sem;

	size_t size;
	size_t usage;
};

struct queue *queue_make(size_t size)
{
	struct queue *queue;

	queue = malloc(sizeof(struct queue));
	if (queue == NULL)
		return NULL;

	queue->tail = NULL;

	queue->size = size;
	queue->usage = 0;

	if (sem_init(&queue->sem, 0, 1) == -1) {
		free(queue);
		return NULL;
	}

	return queue;
}

int queue_enqueue(struct queue *queue, void *data)
{
	struct node *new_node;

	new_node = malloc(sizeof(struct node));
	if (new_node == NULL)
		return -1;

	sem_wait(&queue->sem);
	// -------------------------------------
	// Critical Section Start
	// -------------------------------------
	new_node->data = data;
	new_node->next = queue->tail;
	queue->tail = new_node;

	queue->usage++;
	// -------------------------------------
	// Critical Section End
	// -------------------------------------
	sem_post(&queue->sem);

	return 0;
}

void *queue_dequeue(struct queue *queue)
{
	void *ret;
	struct node *prev;
	
	sem_wait(&queue->sem);
	// -------------------------------------
	// Critical Section Start
	// -------------------------------------
	if (queue->usage == 0)
		return NULL;

	ret = queue->tail->data;
	prev = queue->tail;
	queue->tail = queue->tail->next;

	queue->usage--;
	// -------------------------------------
	// Critical Section End
	// -------------------------------------
	sem_post(&queue->sem);

	free(prev);

	return ret;
}

void *queue_peek(struct queue *queue)
{
	return NULL;
}

void queue_destroy(struct queue *queue)
{
	while (queue_dequeue(queue))
		/* empty loop body */ ;

	sem_destroy(&queue->sem);
	free(queue);
}

bool queue_empty(struct queue *queue)
{
	return queue->usage;
}

size_t queue_size(struct queue *queue)
{
	return queue->size;
}

size_t queue_resize(struct queue *queue, size_t resize)
{
	return (resize < queue->usage) ? (queue->size = queue->usage) 
		                           : (queue->size = resize);
}
