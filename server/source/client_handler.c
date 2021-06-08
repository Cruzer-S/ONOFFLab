#include "client_handler.h"

#include "logger.h"
#include <pthread.h>

void *client_handler_listener(void *argument)
{
	CListenerDataPtr listener = argument;

	pthread_mutex_lock(listener->sync);
	pthread_mutex_unlock(listener->sync);

	if (!listener->valid)
		return (void *) -1;

	return (void *) 0;
}

void *client_handler_worker(void *argument)
{
	CListenerDataPtr worker = argument;

	pthread_mutex_lock(worker->sync);
	pthread_mutex_unlock(worker->sync);

	if (!worker->valid)
		return (void *) -1;

	return NULL;
}

void *client_handler_deliverer(void *argument)
{
	CListenerDataPtr deliverer = argument;

	pthread_mutex_lock(deliverer->sync);
	pthread_mutex_unlock(deliverer->sync);

	if (!deliverer->valid)
		return (void *) -1;


	return NULL;
}
