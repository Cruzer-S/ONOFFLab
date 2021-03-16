#ifndef LOGGER_H__
#define LOGGER_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <string.h>

enum logger_type {
	LOGGER_INFORMATION = 0,
	LOGGER_CRITICAL,
	LOGGER_ERROR
};

struct logger {
	int (*print)(const char *fmt, ...);
};

struct logger create_logger(enum logger_type type, FILE *dest);

#endif
