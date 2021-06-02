#define _POSIX_C_SOURCE 201109L

#include "utility.h"
#include "logger.h"

#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

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

int parse_arguments(int argc, char *argv[], ...)
{
	char *test;
	long overflow;
	va_list ap;
	int ret = 0, type;

	va_start(ap, argv);

	while (argv++, --argc > 0) {
		switch ( (type = va_arg(ap, int)) ) {
		case 16: // uint16_t
		case  4: // int
			overflow = strtol(*argv, &test, 10);
			if (overflow == 0 && *argv == test) {
				pr_err("failed to convert string to long: %s", "Not a number");
				ret = -1; goto CLEANUP;
			}

			if (overflow == LONG_MIN || overflow == LONG_MAX)
				if (errno == ERANGE) {
					pr_err("failed to strtol(): %s", strerror(errno));
					ret = -2; goto CLEANUP;
				}

			break;

		default:
			pr_err("unspecific type: %d", type);
			ret = -3; goto CLEANUP;
		}

		switch (type) {
		case 16: 
			if (overflow < 0 || overflow > UINT16_MAX) {
				pr_err("failed to convert string to uint16_t: %s", "Out of range");
				ret = -4; goto CLEANUP;
			}

			*va_arg(ap, uint16_t *) = overflow;
			break;

		case 4:
			if (overflow < INT_MIN || overflow > INT_MAX) {
				pr_err("failed to convert string to int: %s", "Out of range");
				ret = -5; goto CLEANUP;
			}

			*va_arg(ap, int *) = overflow;
			break;

		}
	}

CLEANUP:
	va_end(ap);
	return ret;
}
