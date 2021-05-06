#ifndef QUEUE_H__
#define QUEUE_H__

#include <stdio.h>
#include <stdbool.h>

struct queue;

struct queue *queue_make(size_t size);
int queue_enqueue(struct queue *queue, void *data);
void *queue_dequeue(struct queue *queue);
void *queue_peek(struct queue *queue);
bool queue_empty(struct queue *queue);
void queue_destroy(struct queue *queue);
size_t queue_size(struct queue *queue);
size_t queue_usage(struct queue *queue);
size_t queue_resize(struct queue *queue, size_t resize);

#endif
