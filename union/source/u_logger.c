#include "u_logger.h"
#include <unistd.h>
/*
enum log_type {
	LOG_ERR = 0, LOG_INF = 1, LOG_CRI = 2
};
*/

static int logg_fd[3] = {
	[LOG_ERR] = STDERR_FILENO,
	[LOG_CRI] = STDERR_FILENO,
	[LOG_INF] = STDOUT_FILENO
};

void init_logg_with(int err, int inf, int cri)
{
	logg_fd[LOG_ERR] = err;
	logg_fd[LOG_CRI] = cri;
	logg_fd[LOG_INF] = inf;
}

int change_logg_fd(enum log_type type, int fd)
{
	logg_fd[type] = fd;
}

static void make_time(char *tbuf, size_t size)
{
	time_t tt = time(NULL);
	struct tm *info = localtime(&tt);

	strftime(tbuf, size, "%x %X", info);
}

static const char *type_name(enum log_type type)
{
	return (const char *[]){
		"ERR", "INF", "CRI"
	}[type];
}

void logg(enum log_type type, const char *fmt, ...)
{
	char buffer[BUFSIZ], *bp = buffer;
	char tbuf[30];
	va_list args;

	va_start(args, fmt);
	make_time(tbuf, sizeof(tbuf));

	bp += sprintf(buffer, "[%s][%s] ", type_name(type), tbuf);
	vsprintf(bp, fmt, args);
	strcat(bp, "\n");

	write(type, buffer, strlen(buffer));

	va_end(args);

	if (type == LOG_CRI)
		exit(EXIT_FAILURE);
}
