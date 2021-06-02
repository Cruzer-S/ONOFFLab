#include "queue.h"

#include <stdlib.h>
#include <pthread.h>

struct node {
	void *data;
	struct node *next;
};

struct queue {
	struct node *tail;

	pthread_cond_t cond;
	pthread_mutex_t mutex;

	size_t size;
	size_t usage;

	bool is_sync;
};

struct queue *queue_create(size_t size, bool is_sync)
{
	struct queue *queue;

	if (size > QUEUE_MAX_SIZE)
		return NULL;

	queue = malloc(sizeof(struct queue));
	if (queue == NULL)
		return NULL;

	queue->tail = NULL;

	queue->size = size;
	queue->usage = 0;

	queue->is_sync = is_sync;

	if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
		free(queue);
		return NULL;
	}

	if (queue->is_sync) {
		if (pthread_cond_init(&queue->cond, NULL) != 0) {
			if (pthread_mutex_destroy(&queue->mutex) != 0)
				/* do nothing */ ;
			
			free(queue);

			return NULL;
		}
	}

	return queue;
}

int queue_enqueue(struct queue *queue, void *data)
{
	struct node *new_node;

	new_node = malloc(sizeof(struct node));
	if (new_node == NULL)
		return -1;

	new_node->data = data;
	new_node->next = queue->tail;

	if (queue->is_sync) {
		pthread_mutex_lock(&queue->mutex);
		// -------------------------------------
		// Critical Section Start
		// -------------------------------------
		queue->tail = new_node;
		queue->usage++;
		// -------------------------------------
		// Critical Section End
		// -------------------------------------
		pthread_mutex_unlock(&queue->mutex);

		pthread_cond_broadcast(&queue->cond);
	} else {
		queue->tail = new_node;
		queue->usage++;
	}

	return 0;
}

void *queue_dequeue(struct queue *queue)
{
	struct node *prev;
	void *ret;

	if (queue->is_sync) {
		pthread_mutex_lock(&queue->mutex);
		// -------------------------------------
		// Critical Section Start
		// -------------------------------------
		if (pthread_cond_wait(&queue->cond, &queue->mutex) != 0) {
			pthread_mutex_unlock(&queue->mutex);
			return NULL;
		}

		if (queue->usage == 0) {
			pthread_mutex_unlock(&queue->mutex);
			return NULL;
		}

		ret = queue->tail->data;
		prev = queue->tail;
		queue->tail = queue->tail->next;

		queue->usage--;
		// -------------------------------------
		// Critical Section End
		// -------------------------------------
		pthread_mutex_unlock(&queue->mutex);
	} else {
		if (queue->usage == 0)
			return NULL;

		ret = queue->tail->data;
		prev = queue->tail;
		queue->tail = queue->tail->next;

		queue->usage--;
	}

	free(prev);

	return ret;
}

void *queue_peek(struct queue *queue)
{
	return queue->tail->data;
}

void queue_destroy(struct queue *queue)
{
	while (queue_dequeue(queue) != NULL)
		/* empty loop body */ ;

	pthread_mutex_destroy(&queue->mutex);
	pthread_cond_destroy(&queue->cond);
	
	free(queue);
}

size_t queue_usage(struct queue *queue)
{
	return queue->usage;
}

bool queue_empty(struct queue *queue)
{
	return queue_usage(queue);
}

size_t queue_size(struct queue *queue)
{
	return queue->size;
}

size_t queue_resize(struct queue *queue, size_t resize)
{
	if (resize > QUEUE_MAX_SIZE)
		return queue->size;

	return (resize < queue->usage) ? (queue->size = queue->usage) 
		                           : (queue->size = resize);
}
