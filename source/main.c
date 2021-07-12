#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <pthread.h>

#include "video.h"
#include "logger.h"

#define VIDEO_LENGTH (10 * 1000)

struct serv_info {
	uint16_t port;
};

struct recorder_argument {
	pthread_attr_t *attr;
	int timeout;
	bool infinite;
	bool show_time;

	enum VIDSIZE size;
	int fps;
};

int get_current_time(char *string, int length)
{
	time_t now = time(NULL);
	int ret;
	
	ret = strftime(string, length, "%c", localtime(&now));

	return (ret != 0);
}

static inline char *change_extension(char *filename, char *extension)
{
	strcpy(strrchr(filename, '.'), extension);
	return filename;
}

void *recorder(void *arg)
{
	struct recorder_argument *recarg;
	char filename[FILENAME_MAX];
	char convname[FILENAME_MAX];

	char timestr[100];
	int ret, pid, err;
	pthread_t tid;

	recarg = arg;

	for (int try = 0; try < 10; try++) {
		if ((ret = get_current_time(timestr, sizeof(timestr))) < 0) {
			pr_err("failed to get_current_time(): %d", ret);
			continue;
		}

		sprintf(filename, "%s.h264", timestr);
		if ((ret = record_video(filename, recarg->timeout, recarg->size,
					recarg->fps, recarg->show_time)) < 0) {
			pr_err("failed to record_video(): %d", ret);
			continue;
		}

		if (recarg->infinite)
			if ((ret = pthread_create(&tid, recarg->attr, recorder, recarg)) != 0)
				pr_err("failed to pthread_create(): %s", strerror(ret));

		strcpy(convname, filename);
		if ((ret = encode_video(filename, change_extension(convname, ".mp4"), true)) < 0)
			pr_err("failed to encode_video(): %d", ret);

		pr_out("video saved successfully: %s", filename);

		return 0;
	}

	pr_out("%s", "failed to save video!\n");

	return NULL;
}

void *ftp_server(void *arg)
{
	int fd;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		pr_err("failed to socket(): %s", strerror(errno));
		return NULL;
	}

	return arg;
}

struct recorder_argument *recarg_create(int timeout, bool infinite,
					int fps, enum VIDSIZE size, bool time)
{
	struct recorder_argument *rec_arg;

	rec_arg = malloc(sizeof(struct recorder_argument));
	if (!rec_arg) {
		pr_err("failed to malloc(): %s", strerror(errno));
		goto RETURN_NULL;
	} else rec_arg->timeout = timeout;
	
	rec_arg->infinite = infinite;
	rec_arg->fps = fps;
	rec_arg->size = size;

	if (rec_arg->infinite) {
		rec_arg->attr = malloc(sizeof(pthread_attr_t));
		if (!rec_arg->attr) {
			pr_err("failed to malloc(): %s", strerror(errno));
			goto FREE_RECARG;
		}

		if (pthread_attr_init(rec_arg->attr) != 0) {
			pr_err("failed to pthread_attr_init(): %s", strerror(errno));
			goto FREE_ATTR;
		}

		if (pthread_attr_setdetachstate(rec_arg->attr, PTHREAD_CREATE_DETACHED) != 0) {
			pr_err("failed to pthread_attr_setdetachstate(): %s", strerror(errno));
			goto DESTROY_ATTR;
		}
	} else rec_arg->attr = NULL;

	return rec_arg;

DESTROY_ATTR:	pthread_attr_destroy(rec_arg->attr);
FREE_ATTR:	free(rec_arg->attr);
FREE_RECARG:	free(rec_arg);
RETURN_NULL:	return NULL;
}

void recarg_destroy(struct recorder_argument *rec_arg)
{
	if (rec_arg->infinite)
		pthread_attr_destroy(rec_arg->attr);

	free(rec_arg->attr); // free(NULL) is safe-action
	free(rec_arg);
}

int main(int argc, char *argv[])
{
	int ret;
	void *retval;
	pthread_t prev, record, ftp;
	struct recorder_argument *rec_arg;

	if (argc != 3)
		pr_crt("usage: %s <length>", argv[0]);

	if ((ret = logger_create("logg.txt")) < 0)
		pr_crt("failed to logger_create(): %d", ret);

	rec_arg = recarg_create(strtol(argv[1], NULL, 10), true,
				30, VIDSIZE_NORMAL, true);
	if (rec_arg == NULL)
		pr_crt("failed to recarg_create(): %p", rec_arg);
		
	if ((ret = pthread_create(&ftp, rec_arg->attr, ftp_server, &ftp)) != 0)
		pr_crt("failed to pthread_create(): %d", ret);

	if ((ret = pthread_create(&record, rec_arg->attr, recorder, rec_arg)) != 0)
		pr_crt("failed to pthread_create(): %d", ret);

	while (true)
		sleep(10);

	recarg_destroy(rec_arg);
	
	logger_destroy();
	
	return 0;
}
