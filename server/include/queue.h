#ifndef QUEUE_H__
#define QUEUE_H__

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

#define QUEUE_MAX_SIZE 1024

struct queue;

struct queue *queue_create(size_t size, bool is_sync);
int queue_enqueue(struct queue *queue, void *data);
void *queue_dequeue(struct queue *queue);
void *queue_peek(struct queue *queue);
bool queue_empty(struct queue *queue);
void queue_destroy(struct queue *queue);
size_t queue_size(struct queue *queue);
size_t queue_usage(struct queue *queue);
size_t queue_resize(struct queue *queue, size_t resize);

#endif
