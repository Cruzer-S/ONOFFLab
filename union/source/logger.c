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

inline void logger_message_redirect(FILE *fp)
{
	redirect = fp;
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
