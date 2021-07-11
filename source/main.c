#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>

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

		strcpy(convname, filename);
		if ((ret = encode_video(filename, change_extension(convname, ".mp4"), true)) < 0) {
			pr_err("failed to encode_video(): %d", ret);
			continue;
		}

		pr_out("video saved successfully: %s", filename);

		try = 0;
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
	pthread_t record, ftp;

	logger_create("logg.txt");

	if ((ret = pthread_create(&record, NULL, recorder, &record)) != 0)
		pr_crt("failed to pthread_create(): %d", ret);

	if ((ret = pthread_create(&ftp, NULL, ftp_server, &ftp)) != 0)
		pr_crt("failed to pthread_create(): %d", ret);

	if ((ret = pthread_join(record, retval)) != 0)
		pr_crt("failed to pthread_join(): %s", "record()");

	if ((ret = pthread_join(ftp, retval)) != 0)
		pr_crt("failed to pthread_join(): %s", "ftp_server()");

	logger_destroy();
	
	return 0;
}
