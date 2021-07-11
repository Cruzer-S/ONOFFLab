#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>

#include <sys/socket.h>

#include <pthread.h>
#include <unistd.h>

#include <sys/wait.h>

#include "logger.h"

#define RECORD_PROGRAM "/bin/raspivid"
#define VIDEO_LENGTH (10 * 1000)

struct serv_info {
	uint16_t port;
};

enum VIDSIZE {
	VIDSIZE_SMALL = 1,
	VIDSIZE_NORMAL,
	VIDSIZE_LARGE
};

static inline char *strap_path(const char *path)
{
	return strrchr(path, '/') + 1;
}

static inline int get_video_size(enum VIDSIZE size, char *width, char *height)
{
	switch (size) {
	case VIDSIZE_SMALL:	// 480p
		sprintf(width, "%d", 854);
		sprintf(height, "%d", 480);
		break;

	case VIDSIZE_NORMAL:	// 720p
		sprintf(width, "%d", 1280);
		sprintf(height, "%d", 720);
		break;

	case VIDSIZE_LARGE:	// 1080p
		sprintf(width, "%d", 1920);
		sprintf(height, "%d", 1080);
		break;

	default: return -1;
	}

	return 0;
}

int record_video(char *filename, int length,
		 enum VIDSIZE size)
{
	int pid;
	int ret, err;
	char timestr[1024];
	char width[100], height[100];

	if ((ret = get_video_size(size, width, height)) < 0) {
		pr_err("failed to get_video_size(): %d", ret);
		return -5;
	}

	sprintf(timestr, "%d", length);
	if ((pid = vfork()) == -1) {
		pr_err("failed to vfork(): %s", strerror(errno));
		return -1; // failed to fork()
	} else if (pid == 0) {
		ret = execl(RECORD_PROGRAM, strap_path(RECORD_PROGRAM),
			    "-o", filename,
			    "-t", timestr,
			    NULL);
		_exit(-2);
	}

	err = waitpid(pid, &ret, 0);
	if (err == -1) {
		pr_err("failed to waitpid(): %s", strerror(errno));
		return -3;
	}

	if (!WIFSIGNALED(ret)) {
		pr_err("failed to execl(): %s (%d)",
			RECORD_PROGRAM, WTERMSIG(ret));
		return -4;
	}

	return 0;
}

int get_current_time(char *string, int length)
{
	time_t now = time(NULL);
	int ret;
	
	ret = strftime(string, length, "%c", localtime(&now));

	return (ret != 0);
}

void *recorder(void *arg)
{
	char filename[FILENAME_MAX];
	char timestr[100];
	int ret;

	for (int try = 0; try < 10; try++) {
		if ((ret = get_current_time(timestr, sizeof(timestr))) < 0) {
			pr_err("failed to get_current_time(): %d", ret);
			continue;
		}

		sprintf(filename, "%s.h264", timestr);
		if ((ret = record_video(filename, VIDEO_LENGTH, VIDSIZE_SMALL)) < 0) {
			pr_err("failed to record_video(): %d", ret);
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
