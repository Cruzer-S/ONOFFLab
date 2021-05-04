#include "logger.h"

#define _POSIX_C_SOURCE 199309L
#include <time.h>

char *get_time0(char *buf, size_t sz_buf)
{
#define STR_TIME_FORMAT "%H:%M:%S"
	struct timespec tspec;
	struct tm tm_now;

	size_t sz_ret;

	if (buf == NULL || sz_buf < 20)
		return NULL;

	if (clock_gettime(CLOCK_MONOTONIC, &tspec) == -1)
		return NULL;

	localtime_r((time_t *) &tspec.tv_sec, &tm_now);
	if ((sz_ret = strftime(buf, sz_buf, STR_TIME_FORMAT, &tm_now)) == 0)
		return NULL;

	return buf;
#undef STR_TIME_FORMAT
}
