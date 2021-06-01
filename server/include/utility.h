#ifndef UTILITY_H__
#define UTILITY_H__

int create_timer(void);
int set_timer(int fd, int timeout);

int parse_arguments(int argc, char *argv[], ...);

#endif
