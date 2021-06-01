#include "logger.h"

#define _POSIX_C_SOURCE 199309L
#define __USE_POSIX		// to use localtime_r()
#define __USE_POSIX199309	// to use clock_gettime()
#include <time.h>

char *get_time0(char *buf, size_t sz_buf)
{
	time_t current = time(NULL);

	strftime(buf, sz_buf, "%b %d %H:%M:%S", localtime(&current));

	return buf;
}
