#ifndef QUEUE_H__
#define QUEUE_H__

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

#define QUEUE_MAX_SIZE 1024

typedef void *Queue;

Queue queue_create(size_t size, bool is_sync);
int queue_enqueue(Queue queue, void *data);
int queue_enqueue_with_bcast(Queue Queue, void *data);
void *queue_dequeue(Queue queue);
void *queue_peek(Queue queue);
bool queue_empty(Queue queue);
void queue_destroy(Queue queue);
size_t queue_size(Queue queue);
size_t queue_usage(Queue queue);
size_t queue_resize(Queue queue, size_t resize);
int queue_broadcast(Queue queue);

#endif
