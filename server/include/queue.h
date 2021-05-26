#ifndef QUEUE_H__
#define QUEUE_H__

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

#define QUEUE_MAX_SIZE 1024

enum queue_data_type {
	QUEUE_DATA_CHAR,
	QUEUE_DATA_UCHAR,

	QUEUE_DATA_SHRT,
	QUEUE_DATA_USHRT,

	QUEUE_DATA_INT,
	QUEUE_DATA_UINT,

	QUEUE_DATA_LONG,
	QUEUE_DATA_ULONG,

	QUEUE_DATA_LLONG,
	QUEUE_DATA_ULLONG,

	QUEUE_DATA_PTR,

	QUEUE_DATA_UNDEF,

	QUEUE_DATA_ERROR,
};

struct queue_data {
	int type;
	union {
		char c;
		unsigned char uc;

		short s;
		unsigned short us;

		int i;
		unsigned int ui;

		long l;
		unsigned long ul;

		long long ll;
		unsigned long long ull;

		void *ptr;
	};
};

struct queue;

struct queue *queue_create(size_t size, bool is_sync);
int queue_enqueue(struct queue *queue, struct queue_data data);
struct queue_data queue_dequeue(struct queue *queue);
struct queue_data queue_peek(struct queue *queue);
bool queue_empty(struct queue *queue);
void queue_destroy(struct queue *queue);
size_t queue_size(struct queue *queue);
size_t queue_usage(struct queue *queue);
size_t queue_resize(struct queue *queue, size_t resize);

#endif
