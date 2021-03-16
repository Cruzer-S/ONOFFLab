#ifndef LOGGER_H__
#define LOGGER_H__

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

enum log_type {
	LOG_ERR, LOG_INF, LOG_CRI
};

void init_logg_with(int err, int inf, int cri);
int change_logg_fd(enum log_type type, int fd);
void logg(enum log_type type, const char *fmt, ...);

#endif
