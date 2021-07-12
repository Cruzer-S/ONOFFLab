#ifndef VIDEO_H__
#define VIDEO_H__

#include <stdbool.h>

enum VIDSIZE {
	VIDSIZE_SMALL = 1,
	VIDSIZE_NORMAL,
	VIDSIZE_LARGE,
	VIDSIZE_TINY
};

int record_video(char *filename, int length, enum VIDSIZE size, 
		 int fps, bool show_time);
int encode_video(char *origin, char *dest, bool del_origin);

#endif
