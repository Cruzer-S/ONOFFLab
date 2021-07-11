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
	char filename[FILENAME_MAX];
	char convname[FILENAME_MAX];

	char timestr[100];
	int ret, pid, err;
	pthread_t tid;

	for (int try = 0; try < 10; try++) {
		if ((ret = get_current_time(timestr, sizeof(timestr))) < 0) {
			pr_err("failed to get_current_time(): %d", ret);
			continue;
		}

		sprintf(filename, "%s.h264", timestr);
		if ((ret = record_video(filename, VIDEO_LENGTH, VIDSIZE_SMALL, 30)) < 0) {
			pr_err("failed to record_video(): %d", ret);
			continue;
		}

		if ((ret = pthread_create(&tid, arg, recorder, arg)) != 0)
			pr_crt("failed to pthread_create(): %s", strerror(ret));

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

int main(void)
{
	int ret;
	void *retval;
	pthread_t prev, record, ftp;
	pthread_attr_t attr;

	if ((ret = logger_create("logg.txt")) < 0)
		pr_crt("failed to logger_create(): %d", ret);

	if (pthread_attr_init(&attr) != 0)
		pr_crt("failed to pthread_attr_init(): %s", strerror(errno));

	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0)
		pr_crt("failed to pthread_attr_setdetachstate(): %s",
			strerror(errno));
	
	if ((ret = pthread_create(&ftp, &attr, ftp_server, &attr)) != 0)
		pr_crt("failed to pthread_create(): %d", ret);

	if ((ret = pthread_create(&record, &attr, recorder, &record)) != 0)
		pr_crt("failed to pthread_create(): %d", ret);

	while (true)
		sleep(10);
	
	logger_destroy();
	
	return 0;
}
