#include "client_handler.h"

#include "logger.h"

void *client_handler_listener(void *argument)
{
	CListenerDataPtr listener = argument;

	return (void *) 0;
}

void *client_handler_worker(void *argument)
{
	CListenerDataPtr worker = argument;

	return NULL;
}

void *client_handler_deliverer(void *argument)
{
	CListenerDataPtr deliverer = argument;

	return NULL;
}
