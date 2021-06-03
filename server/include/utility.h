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

#endif
