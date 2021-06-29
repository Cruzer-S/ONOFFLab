#ifndef UTILITY_H__
#define UTILITY_H__

enum PARSE_ARGUMENT {
	PARSE_ARGUMENT_INT,
	PARSE_ARGUMENT_UINT16,
	PARSE_ARGUMENT_CHARPTR,
};

int create_timer(void);
int set_timer(int fd, int timeout);

int parse_arguments(int argc, char *argv[], ...);

int waitpid_timed(int pid, int *stat, int flags, int timeout);

char *last_strstr(const char *haystack, const char *needle);

int read_timed(int fd, char *buffer, int size, int timeout);

#endif
