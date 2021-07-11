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

#define RECORD_PROGRAM "raspivid"
#define VIDEO_LENGTH (10 * 60 * 1000)

struct serv_info {
	uint16_t port;
};

int record_video(char *filename, int length)
{
	int pid;
	int ret;
	int err;
	char timestr[1024];

	switch (pid = vfork()) {
	case -1:return -1; // failed to fork()
		break;

	case  0:sprintf(timestr, "%d", length);
		ret = execl(RECORD_PROGRAM, 
			    "-o", filename,
			    "-t", timestr);
		pr_err("failed to execl(): %s", strerror(errno));
		return -2;
		break;

	defualt:err = waitpid(pid, &ret, 0);
		if (err == -1) {
			pr_err("failed to waitpid(): %s", strerror(errno));
			return -3;
		}

		if (!WIFSIGNALED(ret)) {
			pr_err("failed to execl(): %s (%d)",
				RECORD_PROGRAM, WTERMSIG(ret));
			return -4;
		}
		break;
	}

	return 0;
}

int get_current_time(char *string, int length)
{
	time_t now = time(NULL);
	int ret;
	
	ret = strftime(string, length, "%x %X", localtime(&now));

	return (ret != 0);
}

void *recorder(void *arg)
{
	char filename[FILENAME_MAX];
	int ret, try = 0;

	while (true) {
		if (try >= 10) {
			pr_err("unrecoverable error is occured: %d", try);
			return NULL;
		}

		if ((ret = get_current_time(filename, sizeof(filename))) < 0) {
			pr_err("failed to get_current_time(): %d", ret);
			try++;
			continue;
		} else try = 0;

		strncat(filename, ".h264", sizeof(filename) - 1);
		if ((ret = record_video(filename, VIDEO_LENGTH)) < 0) {
			pr_err("failed to record_video(): %d", ret);
			continue;
		}

		pr_out("video saved successfully: %s", filename);
	}

	return arg;
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
