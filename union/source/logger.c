#include "logger.h"

#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#define GET_TIME0(X) (get_time0(X, sizeof(X)) == NULL ? "error" : X)

static FILE *redirect = NULL;

static char *get_time0(char *buf, size_t sz_buf)
{
	time_t current = time(NULL);

	strftime(buf, sz_buf, "%b %d %H:%M:%S", gmtime(&current));

	return buf;
}

inline int logger_create(const char *filename)
{
	if (redirect != NULL)
		return -1;

	redirect = fopen(filename, "a+");
	if (redirect == NULL)
		return -2;

	return 0;
}

inline void logger_destroy(void)
{
	if (redirect == NULL)
		return ;

	fclose(redirect);
	redirect = NULL;
}

void __print_message(FILE *fp, const char *fmt, ...)
{
	char tbuf[512];
	char fmtbuf[1024];
	va_list ap;

	va_start(ap, fmt);

	if (redirect)
		fp = redirect;

	get_time0(tbuf, sizeof(tbuf) - 1);
	sprintf(fmtbuf, "[%s] %s", tbuf, fmt);

	vfprintf(fp, fmtbuf, ap);

	fflush(fp);

	va_end(ap);
}
