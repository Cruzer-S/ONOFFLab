#define _XOPEN_SOURCE 500

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#include "logger.h"
#include "video.h"

#define RECORD_PROGRAM "/bin/raspivid"
#define ENCODE_PROGRAM "/bin/MP4Box"

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

int system_args(char *fmt, ...)
{
	va_list ap;
	int ret;
	char command[1024];

	va_start(ap, fmt);

	vsprintf(command, fmt, ap);
	ret = system(command);

	va_end(ap);

	return ret;
}

int encode_video(char *origin, char *dest, bool del_origin)
{
	int pid, ret, err;

	system_args("mv \"%s\" \"%s\"", origin, "output.h264");

	if ((pid = vfork()) == -1) {
		pr_err("failed to vfork(): %s", strerror(errno));
		return -1; // failed to fork()
	} else if (pid == 0) {
		ret = execl(ENCODE_PROGRAM, strap_path(ENCODE_PROGRAM),
			    "-add", "output.h264", "output.mp4", NULL);
		_exit(-2);
	}

	err = waitpid(pid, &ret, 0);
	if (err == -1) {
		pr_err("failed to waitpid(): %s", strerror(errno));
		return -3;
	}

	if (WIFSIGNALED(ret)) {
		pr_err("failed to execl(): %s (%d)",
			RECORD_PROGRAM, WTERMSIG(ret));
		return -4;
	}

	system_args("mv \"%s\" \"%s\"", "output.mp4", dest);
	system_args("mv \"%s\" \"%s\"", "output.h264", origin);

	if (del_origin)
		system_args("rm \"%s\"", origin);

	return 0;
}

int record_video(char *filename, int length, enum VIDSIZE size, int fps)
{
	int pid;
	int ret, err;
	char timestr[1024], fpsstr[100];
	char width[100], height[100];

	if ((ret = get_video_size(size, width, height)) < 0) {
		pr_err("failed to get_video_size(): %d", ret);
		return -5;
	}

	sprintf(fpsstr, "%d", fps);
	sprintf(timestr, "%d", length);
	if ((pid = vfork()) == -1) {
		pr_err("failed to vfork(): %s", strerror(errno));
		return -1; // failed to fork()
	} else if (pid == 0) {
		ret = execl(RECORD_PROGRAM, strap_path(RECORD_PROGRAM),
			    "-o", filename, "-t", timestr,
			    "-w", width, "-h", height,
			    "-fps", fpsstr,
			    NULL);
		_exit(-2);
	}

	err = waitpid(pid, &ret, 0);
	if (err == -1) {
		pr_err("failed to waitpid(): %s", strerror(errno));
		return -3;
	}

	if (WIFSIGNALED(ret)) {
		pr_err("failed to execl(): %s (%d)",
			RECORD_PROGRAM, WTERMSIG(ret));
		return -4;
	}

	return 0;
}
