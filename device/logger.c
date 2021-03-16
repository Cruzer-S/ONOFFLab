#include "logger.h"

#define PRINT(...)

static int ilogger(const char *fmt, ...);
static int clogger(const char *fmt, ...);
static int elogger(const char *fmt, ...);
static char *create_time(void);

struct {
	FILE *dest;
	int (*func)(const char *fmt, ...);
} logger_data[] = {
	[LOGGER_INFORMATION]	= { NULL, ilogger },
	[LOGGER_ERROR]			= { NULL, elogger },
	[LOGGER_CRITICAL]		= { NULL, clogger }
};

struct logger create_logger(enum logger_type type, FILE *dest)
{
	if (dest) logger_data[type].dest = dest;
	else switch ((enum logger_type) type) {
		case LOGGER_INFORMATION:
			logger_data[type].dest = stdout;
			break;
		case LOGGER_ERROR:
			logger_data[type].dest = stderr;
			break;
		case LOGGER_CRITICAL:
			logger_data[type].dest = stderr;
			break;
	}

	return (struct logger) {
		.print = logger_data[type].func
	};
}

static char *create_time(void)
{
	static char tbuf[30];
	time_t tt = time(NULL);
	struct tm *info = localtime(&tt);

	strftime(tbuf, sizeof(tbuf), "%x %X", info);

	return tbuf;
}

static _Noreturn int clogger(const char *fmt, ...)
{
	va_list args;
	FILE *fp = logger_data[LOGGER_CRITICAL].dest;

	va_start(args, fmt);

	fprintf(fp, "[CRI][%s] ", create_time());
	vfprintf(fp, fmt, args);
	fprintf(fp, ": %s", strerror(errno));
	fputc('\n', fp);

	va_end(args);

	fflush(fp);

	exit(EXIT_FAILURE);
}

static int ilogger(const char *fmt, ...)
{
	va_list args;
	FILE *fp = logger_data[LOGGER_CRITICAL].dest;

	va_start(args, fmt);

	fprintf(fp, "[INF][%s] ", create_time());
	vfprintf(fp, fmt, args);
	fputc('\n', fp);

	va_end(args);

	fflush(fp);

	return 0;
}

static int elogger(const char *fmt, ...)
{
	va_list args;
	FILE *fp = logger_data[LOGGER_CRITICAL].dest;

	va_start(args, fmt);

	fprintf(fp, "[ERR][%s] ", create_time());
	vfprintf(fp, fmt, args);
	fprintf(fp, ": %s", strerror(errno));
	fputc('\n', fp);

	va_end(args);

	fflush(fp);

	return 0;
}
