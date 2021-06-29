#define _POSIX_C_SOURCE 201109L

#include "utility.h"
#include "logger.h"

#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/wait.h>

#include <sys/timerfd.h>

int create_timer(void)
{
	int fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if (fd == -1) {
		return -1;
	}

	return fd;
}

int set_timer(int fd, int timeout)
{
	struct itimerspec rt_tspec = {
		.it_value.tv_sec = timeout
	};

	if (timerfd_settime(fd, 0, &rt_tspec, NULL) == -1) {
		return -1;
	}

	return 0;
}

int parse_arguments(int cnt, char *vector[], ...)
{
	char *test;
	long overflow;
	va_list ap;
	int ret = 0, type;

	va_start(ap, vector);

	for (int i = 1; i < cnt; i++) {
		type = va_arg(ap, int);

		switch (type) {
		case PARSE_ARGUMENT_UINT16:
		case PARSE_ARGUMENT_INT:
			errno = 0;
			overflow = strtol(vector[i], &test, 10);
			if (overflow == 0 && vector[i] == test) {
				pr_err("failed to convert string to long: %s", "Not a number");
				ret = -1; goto CLEANUP;
			}

			if (errno == ERANGE) {
				pr_err("failed to strtol(): %s", strerror(errno));
				ret = -2; goto CLEANUP;
			}
			break;
		}

		switch (type) {
		case PARSE_ARGUMENT_UINT16:
			if (overflow < 0 || overflow > UINT16_MAX) {
				pr_err("failed to convert string to uint16_t: %s", "Out of range");
				ret = -4; goto CLEANUP;
			}

			*va_arg(ap, uint16_t *) = overflow;
			break;

		case 4:	// int
			if (errno == ERANGE) {
				pr_err("failed to strtol(): %s", strerror(errno));
				ret = -2; goto CLEANUP;
			}

			if (overflow < INT_MIN || overflow > INT_MAX) {
				pr_err("failed to convert string to int: %s", "Out of range");
				ret = -5; goto CLEANUP;
			}

			*va_arg(ap, int *) = overflow;
			break;

		case PARSE_ARGUMENT_CHARPTR: // char *
			*va_arg(ap, char **) = vector[i];
			break;
		}
	}

CLEANUP:
	va_end(ap);
	return ret;
}

char *last_strstr(const char *haystack, const char *needle)
{
    if (*needle == '\0')
        return (char *) haystack;

    char *result = NULL;
    for (;;) {
        char *p = strstr(haystack, needle);
        if (p == NULL)
            break;
        result = p;
        haystack = p + 1;
    }

    return result;
}

int read_timed(int fd, char *buffer, int size, int timeout)
{
	int ret;

	for (clock_t start, end = start = clock();
	     (end - start) < (CLOCKS_PER_SEC * timeout);
	     end = clock())
	{
		ret = read(fd, buffer, size);
		if (ret == -1 && errno == EAGAIN)
			continue;

		return ret;
	}

	return 0;
}

int waitpid_timed(int pid, int *stat, int flags, int timeout)
{
	int ret;

	for (clock_t start, end = start = clock();
	     (end - start) < timeout * CLOCKS_PER_SEC;
	     end = clock())
	{
		ret = waitpid(pid, stat, flags | WNOHANG);
		if (ret == 0)
			continue;

		return ret;
	}

	return 0;
}
